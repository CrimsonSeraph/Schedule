import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 课程编辑器：新增 / 修改一门课程及其全部上课时间段。
//
// 交互约定（严格遵守项目规范）：
//  - QML 中没有任何 onClicked / Connections / onXxx 处理器；
//  - C++ 侧（app 层 UiConnector）显式连接本文件中的具名控件：
//      * courseSaveButton / courseCancelButton
//      * sessionAddButton / sessionUpdateButton / sessionRemoveButton
//      * sessionRowClick（sessionList 委托内的热区）-> 写回 sessionList.currentIndex
//      * sessionList 的 currentIndexChanged -> 把选中行回填到表单
//  - 课程级字段由 C++ 在保存时直接读取各 TextField 的 text；
//  - 时间段草稿保存在 `sessionDraftModel`（QML ListModel）中，C++ 通过
//    QMetaObject::invokeMethod 调用其 `appendDraft` / `updateDraft` / `rowDraft` / `removeDraft`
//    封装函数（Qt 6 的 append / set / get / remove 直接调用不可用，原因见模型处注释）。
//
// 响应式策略：
//  - 基本信息 / 时间段两个 GridLayout 不再固定 4 列：宽表单 4 列（两对“标签+输入框”
//    并排），中等宽度 2 列，窄屏 1 列（标签在上、输入框在下），输入框始终 fillWidth；
//  - 时间段操作按钮改用 Flow，窄屏自动换行；
//  - 对话框尺寸继续用 Math.min 限制在父窗口内，内部内容由 ScrollView 承载。
Dialog {
    id: courseEditor

    objectName: "courseEditor"

    // C++ 在打开编辑器时写入：课程 id（新增时为空）
    property string editingCourseId: ""

    // C++ 在打开编辑器时写入：冲突提示文本（为空表示无冲突）
    property string conflictHint: ""

    // 表单断点：宽表单 4 列、中等 2 列、窄屏 1 列（输入框一律 fillWidth），断点来自 Responsive
    readonly property int formColumns: Responsive.editorColumns(courseEditor.width)
    readonly property bool wideForm: courseEditor.formColumns === 4
    // 课程名称在宽表单里跨 3 列（凑满一行），其余情况占 1 列
    readonly property int nameFieldSpan: courseEditor.wideForm ? 3 : 1

    title: courseEditor.editingCourseId.length > 0 ? qsTr("编辑课程") : qsTr("新建课程")
    modal: true
    closePolicy: Popup.CloseOnEscape
    width: Responsive.dialogWidth(Responsive.editorDialogWidth, parent ? parent.width : -1)
    height: Responsive.dialogHeight(Responsive.editorDialogHeight, parent ? parent.height : -1)
    anchors.centerIn: parent

    // 时间段草稿模型：C++ 通过 objectName "sessionDraftModel" 定位并调用其方法。
    //
    // Qt 6 的 `QQmlListModel` 只把 `append` / `insert` / `remove` 暴露为 `QQmlV4FunctionPtr`，
    // `set` 收 `QJSValue`、`get` 返回 `QJSValue`，C++ 侧都无法直接传 `QVariantMap` / 接 `QVariant`，
    // 因此这里提供具名参数的封装函数供 C++ 的 `QMetaObject::invokeMethod` 调用
    // （`clear()` 与 `count` 属性仍可直接使用，无需封装）。
    ListModel {
        id: sessionDraft

        objectName: "sessionDraftModel"

        function appendDraft(row) {
            sessionDraft.append(row);
        }

        function updateDraft(index, row) {
            sessionDraft.set(index, row);
        }

        // `ListModel.get()` 返回的是对象包装（C++ 侧会拿到 QObject*，取不到字段），
        // 因此在这里摊平成纯 JS 对象，C++ 才能拿到完整的 QVariantMap。
        function rowDraft(index) {
            const row = sessionDraft.get(index);
            return {
                sessionId: row.sessionId,
                dayOfWeek: row.dayOfWeek,
                dayName: row.dayName,
                startSlot: row.startSlot,
                slotCount: row.slotCount,
                endSlot: row.endSlot,
                weeks: row.weeks,
                weeksDisplay: row.weeksDisplay,
                location: row.location,
                teacher: row.teacher,
                summary: row.summary
            };
        }

        function removeDraft(index) {
            sessionDraft.remove(index, 1);
        }
    }

    contentItem: ColumnLayout {
        spacing: Metrics.spacingXl

        ScrollView {
            id: editorScroll

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            // 关闭横向滚动条并让内容宽度直接跟随 ScrollView 宽度，
            // 避免 contentWidth 与 availableWidth 互相依赖造成绑定循环
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ColumnLayout {
                width: editorScroll.width
                spacing: Metrics.spacingXl

                // ------------------------------------------------------ 课程基本信息
                GroupBox {
                    Layout.fillWidth: true
                    title: qsTr("基本信息")

                    GridLayout {
                        anchors.fill: parent
                        columns: courseEditor.formColumns
                        columnSpacing: Metrics.spacingLg
                        rowSpacing: Metrics.spacingLg

                        Label {
                            text: qsTr("课程名称 *")
                        }
                        TextField {
                            id: editorNameField

                            objectName: "editorNameField"
                            Layout.columnSpan: courseEditor.nameFieldSpan
                            Layout.fillWidth: true
                            placeholderText: qsTr("如 高等数学 A")
                        }

                        Label {
                            text: qsTr("课程代码")
                        }
                        TextField {
                            id: editorCodeField

                            objectName: "editorCodeField"
                            Layout.fillWidth: true
                            placeholderText: "MATH101"
                        }
                        Label {
                            text: qsTr("学分")
                        }
                        TextField {
                            id: editorCreditsField

                            objectName: "editorCreditsField"
                            Layout.fillWidth: true
                            placeholderText: "4"
                            validator: DoubleValidator {
                                bottom: 0
                                top: 30
                                decimals: 1
                            }
                        }

                        Label {
                            text: qsTr("任课教师")
                        }
                        TextField {
                            id: editorTeacherField

                            objectName: "editorTeacherField"
                            Layout.fillWidth: true
                            placeholderText: qsTr("张老师")
                        }
                        Label {
                            text: qsTr("上课地点")
                        }
                        TextField {
                            id: editorLocationField

                            objectName: "editorLocationField"
                            Layout.fillWidth: true
                            placeholderText: qsTr("教一 101")
                        }

                        Label {
                            text: qsTr("课卡颜色")
                        }
                        TextField {
                            id: editorColorField

                            objectName: "editorColorField"
                            Layout.fillWidth: true
                            placeholderText: "#4C8DFF（留空自动配色）"
                        }
                        Label {
                            text: qsTr("备注")
                        }
                        TextField {
                            id: editorNotesField

                            objectName: "editorNotesField"
                            Layout.fillWidth: true
                            placeholderText: qsTr("需带教材")
                        }
                    }
                }

                // ---------------------------------------------------------- 时间段草稿
                GroupBox {
                    Layout.fillWidth: true
                    title: qsTr("上课时间段（周几 + 节次 + 周次）")

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: Metrics.spacingLg

                        ListView {
                            id: sessionList

                            objectName: "sessionList"
                            Layout.fillWidth: true
                            Layout.preferredHeight: ListItemStyle.preferredHeight
                            clip: true
                            spacing: ListItemStyle.spacing
                            model: sessionDraft

                            delegate: Rectangle {
                                width: sessionList.width
                                height: ListItemStyle.height
                                radius: ListItemStyle.radius
                                color: ListView.isCurrentItem ? Theme.selectionBg : Theme.surfaceAlt
                                border.width: ListItemStyle.borderWidth
                                border.color: ListView.isCurrentItem ? Theme.selectionBorder : Theme.border

                                Text {
                                    anchors.left: parent.left
                                    anchors.leftMargin: ListItemStyle.padding
                                    anchors.right: parent.right
                                    anchors.rightMargin: ListItemStyle.padding
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: model.summary
                                    elide: Text.ElideRight
                                    font.pixelSize: Typography.fontBodyLarge
                                    color: Theme.textStrong
                                }

                                // 选中热区：只暴露 objectName 与行号，由 C++ 侧 UiConnector
                                // 连接 clicked() 后写回 sessionList.currentIndex；
                                // 委托由 ListView 动态创建，只存在于 contentItem 的可视子树下。
                                MouseArea {
                                    objectName: "sessionRowClick"

                                    anchors.fill: parent
                                    property int rowIndex: index
                                }
                            }

                            Label {
                                anchors.centerIn: parent
                                visible: sessionDraft.count === 0
                                text: qsTr("尚未添加时间段，请在下方填写后点击“添加时间段”")
                                color: Theme.textMuted
                            }
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            // 宽表单两对“标签 + 输入框”并排；中等宽度一对一行；窄屏标签在上
                            columns: courseEditor.formColumns
                            columnSpacing: Metrics.spacingLg
                            rowSpacing: Metrics.spacingLg

                            Label {
                                text: qsTr("星期")
                            }
                            ComboBox {
                                id: sessionDaySelector

                                objectName: "sessionDaySelector"
                                Layout.fillWidth: true
                                textRole: "label"
                                valueRole: "value"
                                // 去掉“整周”选项：时间段必须落在具体某一天
                                model: schedule.dayOptions.slice(1)
                            }

                            Label {
                                text: qsTr("起始节次")
                            }
                            SpinBox {
                                id: sessionStartSpin

                                objectName: "sessionStartSpin"
                                Layout.fillWidth: true
                                from: 1
                                to: Math.max(1, schedule.timeSlots.length)
                                value: 1
                                editable: true
                            }

                            Label {
                                text: qsTr("连续节数")
                            }
                            SpinBox {
                                id: sessionCountSpin

                                objectName: "sessionCountSpin"
                                Layout.fillWidth: true
                                from: 1
                                to: 12
                                value: 2
                                editable: true
                            }

                            Label {
                                text: qsTr("周次")
                            }
                            TextField {
                                id: sessionWeeksField

                                objectName: "sessionWeeksField"
                                Layout.fillWidth: true
                                placeholderText: qsTr("如 1-16、1-16/2、单周")
                            }

                            Label {
                                text: qsTr("地点覆盖")
                            }
                            TextField {
                                id: sessionLocationField

                                objectName: "sessionLocationField"
                                Layout.fillWidth: true
                                placeholderText: qsTr("留空沿用课程地点")
                            }

                            Label {
                                text: qsTr("教师覆盖")
                            }
                            TextField {
                                id: sessionTeacherField

                                objectName: "sessionTeacherField"
                                Layout.fillWidth: true
                                placeholderText: qsTr("留空沿用课程教师")
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Metrics.spacingMd

                            // 按钮行用 Flow：窄屏自动换行，不会把按钮挤出对话框
                            Flow {
                                Layout.fillWidth: true
                                spacing: Metrics.spacingLg

                                Button {
                                    id: sessionAddButton

                                    objectName: "sessionAddButton"
                                    text: qsTr("添加时间段")
                                }

                                Button {
                                    id: sessionUpdateButton

                                    objectName: "sessionUpdateButton"
                                    text: qsTr("更新选中时间段")
                                }

                                Button {
                                    id: sessionRemoveButton

                                    objectName: "sessionRemoveButton"
                                    text: qsTr("删除选中时间段")
                                }
                            }

                            Label {
                                Layout.fillWidth: true
                                // 窄屏优先保证按钮可用，操作提示省略
                                visible: courseEditor.wideForm
                                text: qsTr("选中列表中的行可回填到表单")
                                color: Theme.textMuted
                                font.pixelSize: Typography.fontSmall
                                elide: Text.ElideRight
                            }
                        }
                    }
                }

                Label {
                    Layout.fillWidth: true
                    visible: courseEditor.conflictHint.length > 0
                    text: courseEditor.conflictHint
                    color: Theme.danger
                    wrapMode: Text.WordWrap
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Metrics.spacingLg

            Label {
                Layout.fillWidth: true
                text: qsTr("带 * 的为必填项；保存后会自动写入本地数据库")
                color: Theme.textMuted
                font.pixelSize: Typography.fontSmall
                // 窄屏时可压缩到 0 宽度，保证“取消 / 保存”始终可见
                elide: Text.ElideRight
            }

            Button {
                id: courseCancelButton

                objectName: "courseCancelButton"
                text: qsTr("取消")
            }

            Button {
                id: courseSaveButton

                objectName: "courseSaveButton"
                text: qsTr("保存课程")
            }
        }
    }
}
