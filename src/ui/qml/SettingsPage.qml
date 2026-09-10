import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

// 设置页：导入 / 导出目录、作息时间（节次）设置、数据库维护与关于信息。
//
// 交互约定：按钮不带 onClicked，文件 / 目录选择对话框也不带 onAccepted；
// C++ 侧（app 层 UiConnector）显式连接并读写下列具名控件。
Item {
    id: settingsPage

    objectName: "settingsPage"

    ScrollView {
        id: settingsScroll

        anchors.fill: parent
        clip: true

        // 关闭横向滚动条并让内容宽度直接跟随 ScrollView 宽度，
        // 避免 contentWidth 与 availableWidth 互相依赖造成绑定循环
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            width: settingsScroll.width
            spacing: 14

            Label {
                Layout.margins: 12
                text: qsTr("设置")
                font.bold: true
                font.pixelSize: 18
                color: "#1F2A44"
            }

            // ------------------------------------------------------------ 目录设置
            GroupBox {
                Layout.fillWidth: true
                Layout.margins: 12
                title: qsTr("导入 / 导出目录")

                GridLayout {
                    anchors.fill: parent
                    columns: 3
                    columnSpacing: 8
                    rowSpacing: 8

                    Label { text: qsTr("默认导入目录") }

                    TextField {
                        id: importDirField

                        objectName: "importDirField"
                        Layout.fillWidth: true
                        placeholderText: schedule.importExport.defaultImportDir
                        text: schedule.importExport.defaultImportDir
                    }

                    Button {
                        id: chooseImportDirButton

                        objectName: "chooseImportDirButton"
                        text: qsTr("选择…")
                    }

                    Label { text: qsTr("默认导出目录") }

                    TextField {
                        id: exportDirField

                        objectName: "exportDirField"
                        Layout.fillWidth: true
                        placeholderText: schedule.importExport.defaultExportDir
                        text: schedule.importExport.defaultExportDir
                    }

                    Button {
                        id: chooseExportDirButton

                        objectName: "chooseExportDirButton"
                        text: qsTr("选择…")
                    }

                    Item { Layout.fillWidth: true }

                    RowLayout {
                        Layout.columnSpan: 2
                        spacing: 8

                        Button {
                            id: saveDirsButton

                            objectName: "saveDirsButton"
                            text: qsTr("保存目录")
                        }

                        Button {
                            id: resetDirsButton

                            objectName: "resetDirsButton"
                            text: qsTr("恢复默认目录")
                        }
                    }
                }
            }

            // ------------------------------------------------------------ 作息时间
            GroupBox {
                Layout.fillWidth: true
                Layout.margins: 12
                title: qsTr("作息时间（节次）")

                GridLayout {
                    anchors.fill: parent
                    columns: 3
                    columnSpacing: 8
                    rowSpacing: 8

                    Label { text: qsTr("选择节次") }

                    ComboBox {
                        id: slotSelector

                        objectName: "slotSelector"
                        Layout.fillWidth: true
                        textRole: "label"
                        valueRole: "index"
                        model: schedule.timeSlots
                    }

                    Item { Layout.fillWidth: true }

                    Label { text: qsTr("名称") }
                    TextField {
                        id: slotLabelField

                        objectName: "slotLabelField"
                        Layout.columnSpan: 2
                        Layout.fillWidth: true
                        placeholderText: qsTr("第 1 节")
                    }

                    Label { text: qsTr("开始时间") }
                    TextField {
                        id: slotStartField

                        objectName: "slotStartField"
                        Layout.fillWidth: true
                        placeholderText: "08:00"
                    }
                    TextField {
                        id: slotEndField

                        objectName: "slotEndField"
                        Layout.fillWidth: true
                        placeholderText: "08:45"
                    }

                    Item { Layout.fillWidth: true }

                    RowLayout {
                        Layout.columnSpan: 2
                        spacing: 8

                        Button {
                            id: saveSlotButton

                            objectName: "saveSlotButton"
                            text: qsTr("保存该节次")
                        }

                        Button {
                            id: resetSlotsButton

                            objectName: "resetSlotsButton"
                            text: qsTr("恢复默认作息")
                        }
                    }
                }
            }

            // ------------------------------------------------------------ 数据维护
            GroupBox {
                Layout.fillWidth: true
                Layout.margins: 12
                title: qsTr("数据")

                GridLayout {
                    anchors.fill: parent
                    columns: 2
                    columnSpacing: 8
                    rowSpacing: 8

                    Label { text: qsTr("数据文件") }
                    Label {
                        Layout.fillWidth: true
                        text: schedule.databasePath
                        color: "#5A6A80"
                        wrapMode: Text.WrapAnywhere
                    }

                    Label { text: qsTr("课程数量") }
                    Label { text: String(schedule.courseCount) }

                    Item { Layout.fillWidth: true }

                    RowLayout {
                        Layout.columnSpan: 2
                        spacing: 8

                        Button {
                            id: reloadButton

                            objectName: "reloadButton"
                            text: qsTr("从数据库重新加载")
                        }

                        Button {
                            id: saveNowButton

                            objectName: "saveNowButton"
                            text: qsTr("立即保存")
                        }

                        Button {
                            id: testButton

                            objectName: "testButton"
                            text: qsTr("自检（输出测试信息）")
                        }
                    }
                }
            }

            Label {
                Layout.margins: 12
                Layout.fillWidth: true
                text: qsTr("Schedule v%1 · 本地课表应用（不做云同步 / 账号系统）\n数据仅保存在本机，导入导出可指定任意目录。")
                          .arg(schedule.version)
                color: "#8A97A8"
                wrapMode: Text.WordWrap
            }

            Item { Layout.fillHeight: true }
        }
    }

    // 目录选择对话框：仅负责把用户选中的目录写回上方文本框，实际保存在 C++ 侧完成
    FolderDialog {
        id: importDirDialog

        objectName: "importDirDialog"
        title: qsTr("选择默认导入目录")
    }

    FolderDialog {
        id: exportDirDialog

        objectName: "exportDirDialog"
        title: qsTr("选择默认导出目录")
    }
}
