import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 课程详情弹层：点击周 / 日视图的课卡后展示该课程的完整信息。
//
// 交互约定（严格遵守项目规范）：
//  - QML 中没有任何 onClicked / Connections / onXxx 处理器；
//  - C++ 侧（app 层 UiConnector）在打开前写入 `course` / `sessions`，并显式连接
//      * courseDetailEditButton  -> 记住当前 courseId，关闭本弹层后打开 courseEditor
//      * courseDetailCloseButton -> 关闭本弹层
//
// 响应式策略：
//  - 尺寸由 Responsive.dialogWidth/Height 限制在父窗口内，内容由 ScrollView 承载；
//  - 基本信息在窄屏下由“标签 + 值”并排降为单列堆叠。
Dialog {
    id: courseDetailDialog

    objectName: "courseDetailDialog"

    // C++ 在打开详情窗前写入：课程字段（含 sessions 列表）
    property var course: ({})

    // C++ 在打开详情窗前写入：全部上课时间段
    property var sessions: []

    readonly property bool narrowForm: Responsive.isDialogNarrow(courseDetailDialog.width)

    title: qsTr("课程详情")
    modal: true
    closePolicy: Popup.CloseOnEscape
    width: Responsive.dialogWidth(Responsive.detailDialogWidth, parent ? parent.width : -1)
    height: Responsive.dialogHeight(Responsive.detailDialogHeight, parent ? parent.height : -1)
    anchors.centerIn: parent

    /** @return 课程字段的展示文本；字段缺失或为空时返回 fallback。 */
    function fieldText(name, fallback) {
        const value = courseDetailDialog.course ? courseDetailDialog.course[name] : undefined;
        if (value === undefined || value === null || value === "") {
            return fallback;
        }
        return String(value);
    }

    contentItem: ColumnLayout {
        spacing: Metrics.spacingXl

        ScrollView {
            id: detailScroll

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ColumnLayout {
                width: detailScroll.width
                spacing: Metrics.spacingXl

                // ------------------------------------------------------ 课程基本信息
                GroupBox {
                    Layout.fillWidth: true
                    title: qsTr("基本信息")

                    GridLayout {
                        anchors.fill: parent
                        columns: courseDetailDialog.narrowForm ? 1 : 2
                        columnSpacing: Metrics.spacingLg
                        rowSpacing: Metrics.spacingLg

                        Label {
                            objectName: "courseDetailName"
                            Layout.fillWidth: true
                            Layout.columnSpan: courseDetailDialog.narrowForm ? 1 : 2
                            text: courseDetailDialog.fieldText("name", qsTr("未命名课程"))
                            font.bold: true
                            font.pixelSize: Typography.fontHeading
                            color: Theme.textPrimary
                            wrapMode: Text.WordWrap
                        }

                        Label {
                            text: qsTr("课程代码")
                            color: Theme.textSecondary
                        }

                        Label {
                            objectName: "courseDetailCode"
                            Layout.fillWidth: true
                            text: courseDetailDialog.fieldText("code", qsTr("未填写"))
                            color: Theme.textPrimary
                            elide: Text.ElideRight
                        }

                        Label {
                            text: qsTr("任课教师")
                            color: Theme.textSecondary
                        }

                        Label {
                            objectName: "courseDetailTeacher"
                            Layout.fillWidth: true
                            text: courseDetailDialog.fieldText("teacher", qsTr("未填写"))
                            color: Theme.textPrimary
                            elide: Text.ElideRight
                        }

                        Label {
                            text: qsTr("上课地点")
                            color: Theme.textSecondary
                        }

                        Label {
                            objectName: "courseDetailLocation"
                            Layout.fillWidth: true
                            text: courseDetailDialog.fieldText("location", qsTr("未填写"))
                            color: Theme.textPrimary
                            elide: Text.ElideRight
                        }

                        Label {
                            text: qsTr("学分")
                            color: Theme.textSecondary
                        }

                        Label {
                            objectName: "courseDetailCredits"
                            Layout.fillWidth: true
                            text: courseDetailDialog.fieldText("credits", qsTr("未填写"))
                            color: Theme.textPrimary
                            elide: Text.ElideRight
                        }

                        Label {
                            text: qsTr("备注")
                            color: Theme.textSecondary
                        }

                        Label {
                            objectName: "courseDetailNotes"
                            Layout.fillWidth: true
                            text: courseDetailDialog.fieldText("notes", qsTr("未填写"))
                            color: Theme.textPrimary
                            wrapMode: Text.WordWrap
                        }
                    }
                }

                // ---------------------------------------------------------- 上课时间段
                GroupBox {
                    Layout.fillWidth: true
                    title: qsTr("上课时间段")

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: Metrics.spacingLg

                        ListView {
                            id: detailSessionList

                            objectName: "courseDetailSessions"
                            Layout.fillWidth: true
                            Layout.preferredHeight: ListItemStyle.preferredHeight
                            clip: true
                            spacing: ListItemStyle.spacing
                            model: courseDetailDialog.sessions

                            delegate: Rectangle {
                                width: detailSessionList.width
                                height: ListItemStyle.height
                                radius: ListItemStyle.radius
                                color: Theme.surfaceAlt
                                border.width: ListItemStyle.borderWidth
                                border.color: Theme.border

                                Text {
                                    anchors.left: parent.left
                                    anchors.leftMargin: ListItemStyle.padding
                                    anchors.right: parent.right
                                    anchors.rightMargin: ListItemStyle.padding
                                    anchors.verticalCenter: parent.verticalCenter
                                    // 与学期页列表项、时间段表单保持同一展示格式
                                    text: qsTr("%1 第 %2-%3 节 · %4").arg(modelData.dayName).arg(modelData.startSlot).arg(modelData.endSlot).arg(modelData.weeksDisplay) + (modelData.location.length > 0 ? " · " + modelData.location : "") + (modelData.teacher.length > 0 ? " · " + modelData.teacher : "")
                                    elide: Text.ElideRight
                                    font.pixelSize: Typography.fontBodyLarge
                                    color: Theme.textStrong
                                }
                            }

                            Label {
                                anchors.centerIn: parent
                                visible: courseDetailDialog.sessions.length === 0
                                text: qsTr("未设置上课时间")
                                color: Theme.textMuted
                            }
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Metrics.spacingLg

            Label {
                Layout.fillWidth: true
                text: qsTr("需要调整？点击“编辑”进入课程编辑器")
                color: Theme.textMuted
                font.pixelSize: Typography.fontSmall
                elide: Text.ElideRight
            }

            Button {
                id: courseDetailCloseButton

                objectName: "courseDetailCloseButton"
                text: qsTr("关闭")
            }

            Button {
                id: courseDetailEditButton

                objectName: "courseDetailEditButton"
                text: qsTr("编辑")
            }
        }
    }
}
