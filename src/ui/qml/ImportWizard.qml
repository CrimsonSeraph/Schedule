import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

// 导入向导：选择文件 → 预览（格式 / 新增数 / 重复数 / 冲突 / 提示）→ 选择合并策略 → 应用。
//
// **文件选择对话框在 UI 层**：本文件提供 FileDialog，仅负责让用户选文件；
// 解析、冲突检测与合并由 data 层完成，C++ 侧（app 层 UiConnector）连接：
//  - importChooseFileButton.clicked  -> 打开 importFileDialog（初始目录取
//    schedule.importExport.lastImportDir）
//  - importFileDialog.accepted       -> 读取 selectedFile -> importExport.preview_import(url)
//  - importApplyButton.clicked       -> importExport.apply_import(策略下标) 并关闭
//  - importCancelButton.clicked      -> importExport.cancel_import() 并关闭
Dialog {
    id: importWizard

    objectName: "importWizard"

    title: qsTr("导入课表")
    modal: true
    closePolicy: Popup.CloseOnEscape
    width: Math.min(680, parent ? parent.width - 40 : 680)
    height: Math.min(620, parent ? parent.height - 40 : 620)
    anchors.centerIn: parent

    // C++ 侧在打开对话框前把文件对话框的初始目录写到这里
    property string initialDirectory: schedule.importExport.lastImportDir

    contentItem: ColumnLayout {
        spacing: 10

        // ------------------------------------------------------------------ 选择文件
        GroupBox {
            Layout.fillWidth: true
            title: qsTr("第 1 步：选择文件")

            RowLayout {
                anchors.fill: parent
                spacing: 8

                TextField {
                    id: importFileField

                    objectName: "importFileField"
                    Layout.fillWidth: true
                    readOnly: true
                    placeholderText: qsTr("支持 JSON / CSV / ICS")
                }

                Button {
                    id: importChooseFileButton

                    objectName: "importChooseFileButton"
                    text: qsTr("浏览…")
                }
            }
        }

        // -------------------------------------------------------------------- 预览
        GroupBox {
            Layout.fillWidth: true
            Layout.fillHeight: true
            title: qsTr("第 2 步：预览与冲突检查")

            ColumnLayout {
                anchors.fill: parent
                spacing: 6

                Label {
                    Layout.fillWidth: true
                    text: schedule.importExport.previewSummary
                    wrapMode: Text.WordWrap
                    color: schedule.importExport.previewConflictCount > 0 ? "#C0392B" : "#2E7D5B"
                    font.bold: true
                }

                Label {
                    Layout.fillWidth: true
                    visible: schedule.importExport.previewWarnings.length > 0
                    text: qsTr("提示：") + schedule.importExport.previewWarnings.join("\n提示：")
                    wrapMode: Text.WordWrap
                    color: "#B7791F"
                }

                ScrollView {
                    id: conflictScroll

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true

                    // 关闭横向滚动条并让内容宽度直接跟随 ScrollView 宽度，
                    // 避免 contentWidth 与 availableWidth 互相依赖造成绑定循环
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    Column {
                        width: conflictScroll.width
                        spacing: 4

                        Repeater {
                            model: schedule.importExport.previewConflicts

                            delegate: Label {
                                width: parent.width
                                text: "• [" + modelData.typeName + "] " + modelData.message
                                wrapMode: Text.WordWrap
                                color: modelData.blocking ? "#C0392B" : "#B7791F"
                                font.pixelSize: 12
                            }
                        }

                        Label {
                            width: parent.width
                            visible: schedule.importExport.previewConflicts.length === 0
                            text: schedule.importExport.hasPendingPreview
                                  ? qsTr("没有发现新引入的冲突。")
                                  : qsTr("选择文件后将在此显示预览结果。")
                            color: "#8A97A8"
                            font.pixelSize: 12
                        }
                    }
                }
            }
        }

        // ------------------------------------------------------------------ 策略
        GroupBox {
            Layout.fillWidth: true
            title: qsTr("第 3 步：合并策略")

            RowLayout {
                anchors.fill: parent
                spacing: 8

                ComboBox {
                    id: importStrategySelector

                    objectName: "importStrategySelector"
                    Layout.fillWidth: true
                    model: schedule.importExport.strategyNames
                    currentIndex: 0
                }

                Button {
                    id: importApplyButton

                    objectName: "importApplyButton"
                    text: qsTr("开始导入")
                    enabled: schedule.importExport.hasPendingPreview
                }

                Button {
                    id: importCancelButton

                    objectName: "importCancelButton"
                    text: qsTr("取消")
                }
            }
        }

        Label {
            Layout.fillWidth: true
            text: schedule.importExport.lastImportSummary
            color: "#5A6A80"
            wrapMode: Text.WordWrap
        }

        ProgressBar {
            Layout.fillWidth: true
            from: 0
            to: 100
            value: schedule.importExport.progress
        }
    }

    // 文件选择对话框：UI 层职责，数据层只接收路径
    FileDialog {
        id: importFileDialog

        objectName: "importFileDialog"
        title: qsTr("选择要导入的课表文件")
        fileMode: FileDialog.OpenFile
        nameFilters: [
            qsTr("全部支持的课表文件 (*.json *.csv *.ics)"),
            qsTr("课表 JSON (*.json)"),
            qsTr("表格 CSV (*.csv *.txt)"),
            qsTr("日历 ICS (*.ics *.ical)"),
            qsTr("全部文件 (*)")
        ]
        currentFolder: importWizard.initialDirectory.length > 0
                       ? "file:///" + importWizard.initialDirectory.replace(/\\/g, "/")
                       : ""
    }
}
