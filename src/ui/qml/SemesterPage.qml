import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 学期页：维护学期元数据（名称 / 起始日 / 总周数）并管理课程列表。
//
// 交互约定：所有按钮与列表都不带 onClicked；C++ 侧（app 层 UiConnector）显式连接。
//  - 学期表单的保存按钮：C++ 读取 semesterNameField / semesterStartField / semesterWeeksSpin，
//    根据 schedule.hasSemester 决定调用 create_semester() 还是 update_semester()；
//  - 课程列表使用 ListView 内建的 currentIndex 选中行为，C++ 读取该属性完成编辑 / 删除。
//
// 响应式策略：
//  - SplitView 方向随宽度切换：够宽时左表单 / 右列表；窄屏（< 900）改为上下排列，
//    表单限高、课程列表占据剩余高度，避免左右都被压到不可用；
//  - 课程列表的操作按钮改用 Flow，窄屏自动换行而不是被挤出视口。
Item {
    id: semesterPage

    objectName: "semesterPage"

    // C++ 在打开编辑器时会把选中课程 id 写回这里（供界面高亮，可选）
    property string highlightedCourseId: ""

    // 宽屏：左右分栏；窄屏：上下分栏
    readonly property bool wideLayout: Responsive.isWideSplit(semesterPage.width)

    SplitView {
        anchors.fill: parent
        orientation: semesterPage.wideLayout ? Qt.Horizontal : Qt.Vertical

        // ---------------------------------------------------------------- 学期表单
        ScrollView {
            id: semesterScroll

            // 宽屏时占左侧固定宽度；窄屏时改为限制纵向高度（-1 表示不参与该方向的分配）
            SplitView.preferredWidth: semesterPage.wideLayout ? Metrics.sidePanelWidth : -1
            SplitView.minimumWidth: semesterPage.wideLayout ? Metrics.sidePanelMinWidth : -1
            SplitView.preferredHeight: semesterPage.wideLayout ? -1 : Metrics.stackedPanelHeight
            SplitView.minimumHeight: semesterPage.wideLayout ? -1 : Metrics.stackedPanelHeight
            clip: true

            ColumnLayout {
                // ScrollView 的 contentItem 宽度按内容隐式宽度收缩，直接写 parent.width
                // 会让表单只有半屏宽；这里显式跟随 ScrollView 视口宽度。
                width: semesterScroll.width
                spacing: Metrics.spacingXl

                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: Metrics.spacingLg
                    Layout.rightMargin: Metrics.spacingLg
                    Layout.topMargin: Metrics.spacingLg
                    spacing: Metrics.spacingLg

                    Label {
                        text: qsTr("学期")
                        color: Theme.textSecondary
                    }

                    ComboBox {
                        id: semesterSelector

                        objectName: "semesterSelector"
                        Layout.fillWidth: true
                        textRole: "name"
                        model: schedule.semesterOptions
                        currentIndex: schedule.currentSemesterIndex
                    }

                    AppButton {
                        objectName: "newSemesterButton"
                        text: qsTr("新建")
                        implicitHeight: Metrics.controlHeight
                    }

                    AppButton {
                        objectName: "deleteSemesterButton"
                        text: qsTr("删除")
                        danger: true
                        implicitHeight: Metrics.controlHeight
                    }
                }

                GridLayout {
                    Layout.margins: Metrics.spacingLg
                    Layout.fillWidth: true
                    // 窄屏单列：标签在上、输入框占满整行，避免占位文字被压到不可读
                    columns: semesterPage.wideLayout ? 2 : 1
                    columnSpacing: Metrics.spacingLg
                    rowSpacing: Metrics.spacingMd

                    Label {
                        text: qsTr("学期名称")
                    }
                    TextField {
                        id: semesterNameField

                        objectName: "semesterNameField"
                        Layout.fillWidth: true
                        placeholderText: qsTr("如 2024-2025 学年第一学期")
                    }

                    Label {
                        text: qsTr("起始日期")
                    }
                    TextField {
                        id: semesterStartField

                        objectName: "semesterStartField"
                        Layout.fillWidth: true
                        placeholderText: qsTr("yyyy-MM-dd（第 1 周周一）")
                    }

                    Label {
                        text: qsTr("总周数")
                    }
                    SpinBox {
                        id: semesterWeeksSpin

                        objectName: "semesterWeeksSpin"
                        Layout.fillWidth: true
                        from: 1
                        to: 64
                        value: 20
                        editable: true
                    }
                }

                AppButton {
                    id: saveSemesterButton

                    objectName: "saveSemesterButton"


                    primary: true
                    Layout.margins: Metrics.spacingLg
                    Layout.fillWidth: true
                    implicitHeight: Metrics.controlHeight
                    text: schedule.hasSemester ? qsTr("保存学期信息") : qsTr("创建学期")
                }

                Item {
                    Layout.fillHeight: true
                }
            }
        }

        // ---------------------------------------------------------------- 课程列表
        ColumnLayout {
            // 宽屏：占满右侧剩余宽度；窄屏：占满下方剩余高度
            SplitView.fillWidth: semesterPage.wideLayout
            SplitView.fillHeight: !semesterPage.wideLayout
            spacing: 0

            ColumnLayout {
                Layout.fillWidth: true
                Layout.margins: Metrics.spacing2xl
                spacing: Metrics.spacingLg

                Label {
                    text: qsTr("课程列表")
                    font.bold: true
                    font.pixelSize: Typography.fontHeading
                    color: Theme.textPrimary
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("%1 门课程").arg(schedule.courseCount)
                    color: Theme.textSecondary
                    font.pixelSize: Typography.fontSmall
                }

                // 用 Flow 承载操作按钮：窄屏自动换行，不会被挤出视口
                Flow {
                    Layout.fillWidth: true
                    spacing: Metrics.spacingLg

                    AppButton {
                        id: pageNewCourseButton

                        objectName: "pageNewCourseButton"


                        primary: true
                        text: qsTr("新建课程")
                        implicitHeight: Metrics.controlHeight
                    }

                    AppButton {
                        id: editCourseButton

                        objectName: "editCourseButton"
                        text: qsTr("编辑选中")
                        flat: true
                        implicitHeight: Metrics.controlHeight
                    }

                    AppButton {
                        id: deleteCourseButton

                        objectName: "deleteCourseButton"


                        danger: true
                        text: qsTr("删除选中")
                        flat: true
                        implicitHeight: Metrics.controlHeight
                    }
                }
            }

            ListView {
                id: courseList

                objectName: "courseList"
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: Metrics.spacing2xl
                clip: true
                spacing: ListItemStyle.spacing
                model: schedule.courseModel

                delegate: Rectangle {
                    width: courseList.width
                    height: ListItemStyle.heightTall
                    radius: ListItemStyle.radiusTall
                    color: ListView.isCurrentItem ? Theme.selectionBg : Theme.surface
                    border.width: ListItemStyle.borderWidth
                    border.color: ListView.isCurrentItem ? Theme.selectionBorder : Theme.border

                    Rectangle {
                        id: colorBar

                        width: ListItemStyle.colorBarWidth
                        height: parent.height - ListItemStyle.colorBarVerticalInset
                        anchors.left: parent.left
                        anchors.leftMargin: ListItemStyle.colorBarInset
                        anchors.verticalCenter: parent.verticalCenter
                        radius: ListItemStyle.colorBarRadius
                        color: model.color
                    }

                    Column {
                        anchors.left: colorBar.right
                        anchors.leftMargin: ListItemStyle.textInset
                        anchors.right: parent.right
                        anchors.rightMargin: ListItemStyle.textInset
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: ListItemStyle.textSpacing

                        Text {
                            width: parent.width
                            text: model.name + (model.code.length > 0 ? "（" + model.code + "）" : "")
                            font.bold: true
                            font.pixelSize: Typography.fontSubheading
                            elide: Text.ElideRight
                            color: Theme.textPrimary
                        }

                        Text {
                            width: parent.width
                            text: model.daySummary.length > 0 ? model.daySummary : qsTr("未设置上课时间")
                            font.pixelSize: Typography.fontBody
                            elide: Text.ElideRight
                            color: Theme.textSecondary
                        }

                        Text {
                            width: parent.width
                            text: (model.location.length > 0 ? model.location + " · " : "") + (model.teacher.length > 0 ? model.teacher + " · " : "") + model.weekDisplay
                            font.pixelSize: Typography.fontSmall
                            elide: Text.ElideRight
                            color: Theme.textMuted
                        }
                    }

                    // 选中热区：只暴露 objectName 与行号，由 C++ 侧 UiConnector 连接 clicked() 后
                    // 写回 courseList.currentIndex；QML 中不写任何信号处理器。
                    // 委托由 ListView 动态创建，只存在于 contentItem 的可视子树下。
                    MouseArea {
                        objectName: "courseListItemClick"

                        anchors.fill: parent
                        property int itemIndex: index
                    }
                }

                Label {
                    anchors.centerIn: parent
                    visible: schedule.courseCount === 0
                    text: qsTr("还没有课程\n点击“新建课程”开始录入")
                    horizontalAlignment: Text.AlignHCenter
                    color: Theme.textMuted
                }
            }

            // 冲突列表
            ColumnLayout {
                Layout.fillWidth: true
                Layout.margins: Metrics.spacing2xl
                spacing: Metrics.spacingSm

                Label {
                    text: schedule.conflictSummary
                    font.bold: true
                    color: schedule.hasBlockingConflicts ? Theme.danger : Theme.success
                }

                Repeater {
                    model: schedule.conflicts

                    delegate: Label {
                        Layout.fillWidth: true
                        text: "• " + modelData.message
                        color: modelData.blocking ? Theme.danger : Theme.warning
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }
    }

    Dialog {
        id: semesterDeleteDialog

        objectName: "semesterDeleteDialog"
        title: qsTr("删除学期")
        modal: true
        anchors.centerIn: parent
        width: Math.min(360, parent ? parent.width - 2 * Metrics.spacing2xl : 320)

        contentItem: ColumnLayout {
            spacing: Metrics.spacingLg

            Label {
                Layout.fillWidth: true
                text: qsTr("删除当前学期会同时删除它的课表与课程，且无法恢复。确定继续？")
                wrapMode: Text.WordWrap
                color: Theme.textPrimary
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Metrics.spacingLg

                Item {
                    Layout.fillWidth: true
                }

                AppButton {
                    objectName: "semesterDeleteCancelButton"
                    text: qsTr("取消")
                    implicitHeight: Metrics.controlHeight
                }

                AppButton {
                    objectName: "semesterDeleteConfirmButton"
                    text: qsTr("删除")
                    danger: true
                    implicitHeight: Metrics.controlHeight
                }
            }
        }
    }
}
