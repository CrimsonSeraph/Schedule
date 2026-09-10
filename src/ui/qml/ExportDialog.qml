import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

// 导出对话框：选择格式与目标目录，导出后**必须**展示实际写入路径。
//
// **目录选择对话框在 UI 层**：本文件提供 FolderDialog；C++ 侧（app 层 UiConnector）连接：
//  - exportChooseDirButton.clicked -> 打开 exportDirDialog（初始目录取 exportDirField.text）
//  - exportDirDialog.accepted      -> 读取 selectedFolder 写回 exportDirField
//  - exportConfirmButton.clicked   -> 读取 formatSelector.currentIndex 与 exportDirField.text，
//                                     调用 schedule.importExport.export_schedule(...)
//  - exportCancelButton.clicked    -> 关闭
// 导出完成后界面通过绑定 `lastExportSummary` / `lastExportPath` 展示**实际路径**。
Dialog {
    id: exportDialog

    objectName: "exportDialog"

    title: qsTr("导出课表")
    modal: true
    closePolicy: Popup.CloseOnEscape
    width: Math.min(620, parent ? parent.width - 40 : 620)
    height: Math.min(460, parent ? parent.height - 40 : 460)
    anchors.centerIn: parent

    contentItem: ColumnLayout {
        spacing: 10

        GroupBox {
            Layout.fillWidth: true
            title: qsTr("导出设置")

            GridLayout {
                anchors.fill: parent
                columns: 2
                columnSpacing: 8
                rowSpacing: 8

                Label { text: qsTr("文件格式") }
                ComboBox {
                    id: exportFormatSelector

                    objectName: "exportFormatSelector"
                    Layout.fillWidth: true
                    model: schedule.importExport.formatNames
                    currentIndex: 0
                }

                Label { text: qsTr("目标目录") }
                TextField {
                    id: exportDirField

                    objectName: "exportDirField"
                    Layout.fillWidth: true
                    text: schedule.importExport.defaultExportDir
                    placeholderText: qsTr("默认：文档/Schedule")
                }

                Item { Layout.fillWidth: true }

                RowLayout {
                    Layout.columnSpan: 2
                    spacing: 8

                    Button {
                        id: exportChooseDirButton

                        objectName: "exportChooseDirButton"
                        text: qsTr("选择目录…")
                    }

                    Button {
                        id: exportResetDirButton

                        objectName: "exportResetDirButton"
                        text: qsTr("使用默认目录")
                    }
                }
            }
        }

        GroupBox {
            Layout.fillWidth: true
            Layout.fillHeight: true
            title: qsTr("导出结果")

            ColumnLayout {
                anchors.fill: parent
                spacing: 6

                Label {
                    Layout.fillWidth: true
                    text: schedule.importExport.lastExportSummary.length > 0
                          ? schedule.importExport.lastExportSummary
                          : qsTr("尚未导出。文件名规则：Schedule_<学期>_<yyyyMMdd_HHmmss>.<扩展名>")
                    wrapMode: Text.WordWrap
                    color: schedule.importExport.lastExportPath.length > 0 ? "#2E7D5B" : "#5A6A80"
                }

                Label {
                    Layout.fillWidth: true
                    text: schedule.importExport.lastExportPath
                    wrapMode: Text.WrapAnywhere
                    color: "#1B4FA8"
                    font.bold: true
                }

                Item { Layout.fillHeight: true }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                Layout.fillWidth: true
                text: schedule.importExport.lastError
                color: "#C0392B"
                wrapMode: Text.WordWrap
                font.pixelSize: 11
            }

            Button {
                id: exportCancelButton

                objectName: "exportCancelButton"
                text: qsTr("关闭")
            }

            Button {
                id: exportConfirmButton

                objectName: "exportConfirmButton"
                text: qsTr("导出")
            }
        }

        ProgressBar {
            Layout.fillWidth: true
            from: 0
            to: 100
            value: schedule.importExport.progress
        }
    }

    FolderDialog {
        id: exportDirDialog

        objectName: "exportDirDialog"
        title: qsTr("选择导出目录")
    }
}
