import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 待办页：列表 + 新增 / 编辑 / 完成 / 删除。
//
// 交互仍由 C++（UiConnector::connect_todo_page）按 objectName 连接；
// 列表热区 todoListItemClick 与学期页课程列表同一套机制。
Item {
    id: todoPage

    objectName: "todoPage"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Metrics.spacingLg
        spacing: Metrics.spacingMd

        RowLayout {
            Layout.fillWidth: true
            spacing: Metrics.spacingMd

            Label {
                Layout.fillWidth: true
                text: qsTr("%1 项待办 · %2 项已完成")
                    .arg(schedule.openTodoCount)
                    .arg(schedule.todoCount - schedule.openTodoCount)
                color: Theme.textSecondary
                font.pixelSize: Typography.fontSmall
                elide: Text.ElideRight
            }

            AppButton {
                objectName: "todoAddButton"
                text: qsTr("新增")
                primary: true
                implicitHeight: Metrics.controlHeight
            }
        }

        ListView {
            id: todoList

            objectName: "todoList"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: Metrics.spacingSm
            model: schedule.todoModel

            delegate: Rectangle {
                width: ListView.view.width
                height: Math.max(56, rowContent.implicitHeight + Metrics.spacingLg * 2)
                radius: 12
                color: ListView.isCurrentItem ? Theme.surfaceAlt : Theme.surfaceSubtle
                border.width: ListView.isCurrentItem ? Metrics.borderWidth * 2 : Metrics.borderWidth
                border.color: ListView.isCurrentItem ? Theme.accent : Theme.border

                RowLayout {
                    id: rowContent

                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.margins: Metrics.spacingLg
                    spacing: Metrics.spacingMd

                    Rectangle {
                        Layout.alignment: Qt.AlignTop
                        width: 18
                        height: 18
                        radius: 9
                        color: model.done ? Theme.accent : "transparent"
                        border.width: Metrics.borderWidth
                        border.color: model.done ? Theme.accent : Theme.border

                        Text {
                            anchors.centerIn: parent
                            visible: model.done
                            text: "✓"
                            color: "#FFFFFF"
                            font.pixelSize: Typography.fontTiny
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Label {
                            Layout.fillWidth: true
                            text: model.title
                            color: model.done ? Theme.textMuted : Theme.textPrimary
                            font.pixelSize: Typography.fontBody
                            font.strikeout: model.done
                            elide: Text.ElideRight
                        }

                        Label {
                            Layout.fillWidth: true
                            visible: model.dueText.length > 0
                            text: model.dueText
                            color: model.overdue ? Theme.danger : Theme.textSecondary
                            font.pixelSize: Typography.fontSmall
                        }

                        Label {
                            Layout.fillWidth: true
                            visible: model.notes.length > 0
                            text: model.notes
                            color: Theme.textMuted
                            font.pixelSize: Typography.fontSmall
                            elide: Text.ElideRight
                        }
                    }
                }

                MouseArea {
                    objectName: "todoListItemClick"
                    property int itemIndex: index
                    anchors.fill: parent
                }
            }

            Label {
                anchors.centerIn: parent
                visible: todoList.count === 0
                text: qsTr("还没有待办，点「新增」添加一条。")
                color: Theme.textMuted
                font.pixelSize: Typography.fontBody
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Metrics.spacingMd

            Item {
                Layout.fillWidth: true
            }

            AppButton {
                objectName: "todoEditButton"
                text: qsTr("编辑选中")
                implicitHeight: Metrics.controlHeight
            }

            AppButton {
                objectName: "todoToggleButton"
                text: qsTr("完成/取消")
                implicitHeight: Metrics.controlHeight
            }

            AppButton {
                objectName: "todoDeleteButton"
                text: qsTr("删除选中")
                danger: true
                implicitHeight: Metrics.controlHeight
            }
        }
    }

    Dialog {
        id: todoDialog

        objectName: "todoDialog"
        title: qsTr("待办")
        modal: true
        anchors.centerIn: parent
        width: Math.min(400, parent ? parent.width - 2 * Metrics.spacing2xl : 360)

        contentItem: ColumnLayout {
            spacing: Metrics.spacingLg

            Label {
                Layout.fillWidth: true
                text: qsTr("标题")
                color: Theme.textPrimary
            }

            TextField {
                objectName: "todoTitleField"
                Layout.fillWidth: true
                placeholderText: qsTr("如 交数据结构实验报告")
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("截止时间（yyyy-MM-dd HH:mm，可留空）")
                color: Theme.textPrimary
            }

            TextField {
                objectName: "todoDueField"
                Layout.fillWidth: true
                placeholderText: "2026-11-21 18:00"
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("备注")
                color: Theme.textPrimary
            }

            TextField {
                objectName: "todoNotesField"
                Layout.fillWidth: true
                placeholderText: qsTr("可选")
            }

            CheckBox {
                objectName: "todoDoneCheck"
                text: qsTr("已完成")
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Metrics.spacingLg

                Item {
                    Layout.fillWidth: true
                }

                AppButton {
                    objectName: "todoCancelButton"
                    text: qsTr("取消")
                    implicitHeight: Metrics.controlHeight
                }

                AppButton {
                    objectName: "todoSaveButton"
                    text: qsTr("保存")
                    primary: true
                    implicitHeight: Metrics.controlHeight
                }
            }
        }
    }
}
