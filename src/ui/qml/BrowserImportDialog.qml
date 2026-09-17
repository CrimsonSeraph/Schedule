import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 「从教务导入」对话框：在内嵌浏览器里打开教务系统，停在课表页后一键抓取并导入。
//
// **入口列表（第 1 项固定，其后为已适配的教务网站）由 C++ 侧组装**
// （`schedule.importExport.browserEntries`）：本文件只做展示与取用，不判断哪些学校可用。
//
// C++ 侧（app 层 UiConnector）连接的全部交互：
//  - browserEntrySelector.currentIndexChanged -> 把选中入口的地址回填 browserUrlField
//  - browserOpenEntryButton.clicked           -> 用 browserUrlField 的内容导航
//  - browserGoButton.clicked                  -> 同「打开此入口」（地址栏手动输入时用）
//  - browserReloadButton.clicked              -> 重新加载当前页
//  - browserSystemOpenButton.clicked          -> 交给系统默认浏览器打开
//  - browserImportButton.clicked              -> 从当前页面抓取课表并生成导入预览
//  - browserCloseButton.clicked               -> 关闭
//  - scheduleBrowser.captureFinished(bool, QString) -> 把抓取到的页面原文交给 ImportManager
//
// **隐私**：应用不读取也不保存密码与 Cookie；内嵌浏览器只承载用户自己的登录会话，
// 抓取时只带走**当前页面的 HTML**，应用退出即消失。
//
// 响应式策略：工具栏与按钮行用 Flow 自动换行；网页区域取剩余高度并保底
// `Responsive.browserViewMinHeight`，低于该高度时提示用户放大窗口。
Dialog {
    id: browserDialog

    objectName: "browserImportDialog"

    title: qsTr("从教务导入")
    modal: true
    closePolicy: Popup.CloseOnEscape
    width: Responsive.dialogWidth(Responsive.browserDialogWidth, parent ? parent.width : -1)
    height: Responsive.dialogHeight(Responsive.browserDialogHeight, parent ? parent.height : -1)
    anchors.centerIn: parent

    /** 当前选中的入口是否为「打开内置浏览器」（没有预设地址，需用户自己填 / 导航）。 */
    readonly property bool manualEntry: browserEntrySelector.currentIndex <= 0

    contentItem: ScrollView {
        id: contentScroll
        clip: true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ScrollBar.vertical.policy: ScrollBar.AsNeeded
        rightPadding: Metrics.spacingSm

        ColumnLayout {
            width: contentScroll.availableWidth
            spacing: Metrics.spacingLg

            // 入口选择
            GroupBox {
                Layout.fillWidth: true
                Layout.margins: Metrics.spacingLg
                title: qsTr("第 1 步：选择教务入口")

                ColumnLayout {
                    anchors.fill: parent
                    spacing: Metrics.spacingLg

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Metrics.spacingLg

                        ComboBox {
                            id: browserEntrySelector

                            objectName: "browserEntrySelector"
                            Layout.fillWidth: true
                            Layout.preferredWidth: 320
                            textRole: "name"
                            model: schedule.importExport.browserEntries
                        }

                        Button {
                            id: browserOpenEntryButton

                            objectName: "browserOpenEntryButton"
                            text: qsTr("打开此入口")
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        visible: browserEntrySelector.currentIndex >= 0
                        text: {
                            const entries = schedule.importExport.browserEntries;
                            if (browserEntrySelector.currentIndex < 0 || browserEntrySelector.currentIndex >= entries.length) {
                                return "";
                            }
                            return entries[browserEntrySelector.currentIndex].description;
                        }
                        wrapMode: Text.WordWrap
                        color: Theme.textMuted
                        font.pixelSize: Typography.fontSmall
                    }
                }
            }

            // 浏览器
            GroupBox {
                Layout.fillWidth: true
                Layout.margins: Metrics.spacingLg
                title: qsTr("第 2 步：在教务系统中打开课表页面")
                Layout.preferredHeight: schedule.importExport.hasEmbeddedBrowser ? (Responsive.isTiny(browserDialog.width) ? Responsive.browserViewMinHeightCompact + Metrics.spacing4xl : Responsive.browserViewMinHeight + Metrics.spacing6xl) : implicitHeight

                ColumnLayout {
                    anchors.fill: parent
                    spacing: Metrics.spacingLg

                    Flow {
                        Layout.fillWidth: true
                        spacing: Metrics.spacingLg

                        TextField {
                            id: browserUrlField

                            objectName: "browserUrlField"
                            width: browserDialog.width > 640 ? Math.max(220, browserDialog.width - 320) : browserDialog.width - Metrics.spacing6xl
                            placeholderText: qsTr("教务系统地址（可直接粘贴课表页网址）")
                        }

                        Button {
                            id: browserGoButton

                            objectName: "browserGoButton"
                            text: qsTr("前往")
                        }

                        Button {
                            id: browserReloadButton

                            objectName: "browserReloadButton"
                            text: qsTr("重新加载")
                        }

                        Button {
                            id: browserSystemOpenButton

                            objectName: "browserSystemOpenButton"
                            text: qsTr("用系统浏览器打开")
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        visible: !schedule.importExport.hasEmbeddedBrowser
                        text: qsTr("当前构建未启用内嵌浏览器：可先用系统浏览器打开并另存课表页面，再用「文件导入」选择该文件。")
                        wrapMode: Text.WordWrap
                        color: Theme.warning
                        font.pixelSize: Typography.fontSmall
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        visible: schedule.importExport.hasEmbeddedBrowser
                        color: Theme.surfaceAlt
                        border.color: Theme.border
                        border.width: Metrics.borderWidth
                        radius: Metrics.radiusSm
                        clip: true

                        EmbeddedBrowser {
                            id: scheduleBrowser
                            anchors.fill: parent
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        text: scheduleBrowser.lastError.length > 0 ? scheduleBrowser.lastError : (scheduleBrowser.pageTitle.length > 0 ? qsTr("当前页面：%1").arg(scheduleBrowser.pageTitle) : qsTr("尚未打开任何页面"))
                        wrapMode: Text.WordWrap
                        color: scheduleBrowser.lastError.length > 0 ? Theme.danger : Theme.textSecondary
                        font.pixelSize: Typography.fontSmall
                    }
                }
            }

            // 抓取与关闭
            GroupBox {
                Layout.fillWidth: true
                Layout.margins: Metrics.spacingLg
                title: qsTr("第 3 步：抓取并导入")

                ColumnLayout {
                    anchors.fill: parent
                    spacing: Metrics.spacingLg

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("隐私说明：抓取只读取**当前页面的内容**，不读取也不保存密码或 Cookie；不做后台同步。")
                        wrapMode: Text.WordWrap
                        color: Theme.warning
                        font.pixelSize: Typography.fontSmall
                    }

                    Flow {
                        Layout.fillWidth: true
                        spacing: Metrics.spacingLg

                        Button {
                            id: browserImportButton

                            objectName: "browserImportButton"
                            text: qsTr("导入课表")
                            enabled: schedule.importExport.hasEmbeddedBrowser
                        }

                        Button {
                            id: browserCloseButton

                            objectName: "browserCloseButton"
                            text: qsTr("关闭")
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        visible: schedule.importExport.webCaptureSummary.length > 0
                        text: schedule.importExport.webCaptureSummary
                        wrapMode: Text.WordWrap
                        color: schedule.importExport.hasPendingPreview ? Theme.success : Theme.danger
                        font.pixelSize: Typography.fontSmall
                    }
                }
            }
        }
    }
}
