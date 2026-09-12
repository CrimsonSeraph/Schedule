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
//
// 响应式策略：
//  - 整个内容套一层 ScrollView：低高度屏幕（横屏 / 分屏）可以整体滚动；
//  - 导出设置 GridLayout 在窄屏降为单列，输入框保持 fillWidth；
//  - 目录按钮行与底部按钮行改用 Flow，窄屏自动换行；
//  - 对话框尺寸继续用 Math.min 限制在父窗口内。
Dialog {
    id: exportDialog

    objectName: "exportDialog"

    title: qsTr("导出课表")
    modal: true
    closePolicy: Popup.CloseOnEscape
    width: Math.min(620, parent ? parent.width - 40 : 620)
    height: Math.min(460, parent ? parent.height - 40 : 460)
    anchors.centerIn: parent

    // 窄表单：标签在上、输入框在下
    readonly property bool narrowForm: Responsive.isDialogNarrow(exportDialog.width)

    contentItem: ScrollView {
        id: exportScroll

        clip: true

        // 关闭横向滚动条并让内容宽度直接跟随 ScrollView 宽度，
        // 避免 contentWidth 与 availableWidth 互相依赖造成绑定循环
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            width: exportScroll.width
            spacing: 10

            GroupBox {
                Layout.fillWidth: true
                title: qsTr("导出设置")

                GridLayout {
                    anchors.fill: parent
                    columns: exportDialog.narrowForm ? 1 : 2
                    columnSpacing: 8
                    rowSpacing: 8

                    Label {
                        text: qsTr("文件格式")
                    }
                    ComboBox {
                        id: exportFormatSelector

                        objectName: "exportFormatSelector"
                        Layout.fillWidth: true
                        model: schedule.importExport.formatNames
                        currentIndex: 0
                    }

                    Label {
                        text: qsTr("目标目录")
                    }
                    TextField {
                        id: exportDirField

                        objectName: "exportDirField"
                        Layout.fillWidth: true
                        text: schedule.importExport.defaultExportDir
                        placeholderText: qsTr("默认：文档/Schedule")
                    }

                    // 按钮行：Flow 承载，窄屏自动换行
                    Flow {
                        Layout.fillWidth: true
                        Layout.columnSpan: exportDialog.narrowForm ? 1 : 2
                        spacing: Responsive.spacing

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
                // 外层是 ScrollView（高度不定），这里给出有限的预览高度
                Layout.preferredHeight: Math.max(120, Math.min(220, Math.round(exportDialog.height * 0.35)))
                title: qsTr("导出结果")

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 6

                    Label {
                        Layout.fillWidth: true
                        text: schedule.importExport.lastExportSummary.length > 0 ? schedule.importExport.lastExportSummary : qsTr("尚未导出。文件名规则：Schedule_<学期>_<yyyyMMdd_HHmmss>.<扩展名>")
                        wrapMode: Text.WordWrap
                        color: schedule.importExport.lastExportPath.length > 0 ? Responsive.success : Responsive.textSecondary
                    }

                    Label {
                        Layout.fillWidth: true
                        text: schedule.importExport.lastExportPath
                        wrapMode: Text.WrapAnywhere
                        color: Responsive.accentStrong
                        font.bold: true
                    }

                    Item {
                        Layout.fillHeight: true
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Responsive.spacing

                Label {
                    Layout.fillWidth: true
                    text: schedule.importExport.lastError
                    color: Responsive.danger
                    wrapMode: Text.WordWrap
                    font.pixelSize: Responsive.fontSmall
                }

                // Flow：窄屏时“导出 / 关闭”自动换行，不会被挤出对话框
                Flow {
                    Layout.fillWidth: true
                    layoutDirection: Qt.RightToLeft
                    spacing: Responsive.spacing

                    Button {
                        id: exportConfirmButton

                        objectName: "exportConfirmButton"
                        text: qsTr("导出")
                    }

                    Button {
                        id: exportCancelButton

                        objectName: "exportCancelButton"
                        text: qsTr("关闭")
                    }
                }
            }

            ProgressBar {
                Layout.fillWidth: true
                from: 0
                to: 100
                value: schedule.importExport.progress
            }
        }
    }

    FolderDialog {
        id: exportDirDialog

        objectName: "exportDirDialog"
        title: qsTr("选择导出目录")
    }
}
