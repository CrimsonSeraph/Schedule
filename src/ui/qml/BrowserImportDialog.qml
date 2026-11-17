import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 「从教务导入」：移动端优先的全屏页；桌面端仍是居中对话框。
//
// 结构（自上而下）：
//   顶栏    关闭 / 标题 / 「导入课表」主操作
//   入口行  教务入口下拉 +「打开」
//   地址行  教务系统地址 +「前往」
//   网页    占满剩余高度（不再套 Flickable，避免 WebView 吃掉滚动手势）
//   状态行  当前页面 / 错误 / 抓取摘要
//   底栏    重新加载 / 缩放 / 系统浏览器
//
// 交互仍全部由 C++ 侧（app 层 UiConnector）按 objectName 连接，QML 不写信号处理器：
//  - browserEntrySelector.currentIndexChanged -> 回填 browserUrlField
//  - browserOpenEntryButton / browserGoButton / browserReloadButton
//  - browserSystemOpenButton / browserImportButton / browserCloseButton
//  - browserZoomOutButton / browserZoomInButton / browserZoomResetButton
//  - scheduleBrowser.captureFinished(bool, QString)
Dialog {
    id: browserDialog

    objectName: "browserImportDialog"

    modal: true
    closePolicy: Popup.CloseOnEscape
    // 顶栏 / 底栏自绘：Controls 默认 header 会额外占一行，且全屏时没有位置放关闭按钮。
    header: null
    footer: null
    padding: 0
    // 以 Overlay 为父对象，全屏时才能盖住应用自身的周次导航。
    parent: Overlay.overlay

    readonly property real parentW: parent ? parent.width : 480
    readonly property real parentH: parent ? parent.height : 800
    // Android / iOS 上横竖屏都全屏显示网页；桌面端窄窗口同样全屏
    readonly property bool fullScreenMode: Qt.platform.os === "android" || Qt.platform.os === "ios" || parentW < 600

    // 横屏高度紧张：默认收起入口 / 地址栏，把高度让给网页；顶栏“地址”可展开
    property bool addressVisible: false
    readonly property bool compactHeight: height < 620
    readonly property int horizontalInset: Metrics.spacing2xl

    width: fullScreenMode ? parentW : Math.min(Responsive.browserDialogWidth, parentW - 40)
    height: fullScreenMode ? parentH : Math.min(Responsive.browserDialogHeight, parentH - 40)
    x: fullScreenMode ? 0 : (parentW - width) / 2
    y: fullScreenMode ? 0 : (parentH - height) / 2

    background: Rectangle {
        color: Theme.surface
        radius: browserDialog.fullScreenMode ? 0 : Metrics.radiusLg
        border.width: browserDialog.fullScreenMode ? 0 : Metrics.borderWidth
        border.color: Theme.border
    }

    contentItem: ColumnLayout {
        // Dialog 的 contentItem 默认按隐式高度布局，横屏时底部会留出一大块空白；
        // 显式撑满可用区域，让网页区拿到全部剩余高度。
        width: browserDialog.availableWidth
        height: browserDialog.availableHeight
        spacing: 0

        // ---------------------------------------------------------------- 顶栏
        Rectangle {
            Layout.fillWidth: true
            // 全屏时窗口是 edge-to-edge，顶栏内容要让开系统状态栏；
            // SafeArea 在桌面平台为 0，不影响对话框形态。
            Layout.preferredHeight: Metrics.appBarHeight + SafeArea.margins.top
            // 顶栏用主题蓝：状态栏图标是浅色，白色顶栏会让时间/信号“消失”。
            color: Theme.accent

            RowLayout {
                anchors.fill: parent
                anchors.topMargin: SafeArea.margins.top
                anchors.leftMargin: browserDialog.horizontalInset
                anchors.rightMargin: browserDialog.horizontalInset
                spacing: Metrics.spacingLg

                ToolButton {
                    id: browserCloseButton

                    objectName: "browserCloseButton"
                    text: qsTr("关闭")
                    flat: true
                    Layout.preferredHeight: Metrics.toolButtonSize
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("关闭")

                    contentItem: Text {
                        text: browserCloseButton.text
                        color: "#FFFFFF"
                        font.pixelSize: Typography.fontSmall
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("从教务导入")
                        font.pixelSize: Typography.fontHeading
                        font.bold: true
                        color: "#FFFFFF"
                        elide: Text.ElideRight
                    }

                    Label {
                        Layout.fillWidth: true
                        visible: !browserDialog.compactHeight
                        text: qsTr("登录后停在课表页")
                        font.pixelSize: Typography.fontSmall
                        color: Theme.cardTextTertiary
                        elide: Text.ElideRight
                    }
                }

                ToolButton {
                    id: browserCompactReloadButton

                    objectName: "browserCompactReloadButton"
                    text: qsTr("刷新")
                    visible: browserDialog.compactHeight
                    flat: true
                    Layout.preferredHeight: Metrics.toolButtonSize
                    contentItem: Text {
                        text: browserCompactReloadButton.text
                        color: "#FFFFFF"
                        font.pixelSize: Typography.fontSmall
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                ToolButton {
                    id: browserAddressButton

                    objectName: "browserAddressButton"
                    text: qsTr("地址")
                    visible: browserDialog.compactHeight
                    flat: true
                    Layout.preferredHeight: Metrics.toolButtonSize
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("展开 / 收起地址栏")

                    contentItem: Text {
                        text: browserAddressButton.text
                        color: "#FFFFFF"
                        font.pixelSize: Typography.fontSmall
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                ToolButton {
                    id: browserRotateButton

                    objectName: "browserRotateButton"
                    text: qsTr("横屏")
                    flat: true
                    Layout.preferredHeight: Metrics.toolButtonSize
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("切换横屏 / 竖屏")

                    contentItem: Text {
                        text: browserRotateButton.text
                        color: "#FFFFFF"
                        font.pixelSize: Typography.fontSmall
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                Button {
                    id: browserImportButton

                    objectName: "browserImportButton"
                    text: qsTr("导入课表")
                    enabled: schedule.importExport.hasEmbeddedBrowser
                    Layout.preferredHeight: Metrics.controlHeight
                    font.pixelSize: Typography.fontBodyLarge

                    contentItem: Text {
                        text: browserImportButton.text
                        color: browserImportButton.enabled ? Theme.accentStrong : Theme.textMuted
                        font: browserImportButton.font
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }

                    background: Rectangle {
                        radius: Metrics.radiusLg
                        color: browserImportButton.enabled ? "#FFFFFF" : Theme.surfaceAlt
                        opacity: browserImportButton.down ? 0.85 : 1.0
                    }
                }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: Metrics.borderWidth
                color: Qt.rgba(1, 1, 1, 0.24)
            }
        }

        // ------------------------------------------------------------ 入口 / 地址
        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: browserDialog.horizontalInset
            Layout.rightMargin: browserDialog.horizontalInset
            Layout.topMargin: Metrics.spacingMd
            spacing: Metrics.spacingMd
            // 横屏默认收起，避免入口/地址挤占网页高度
            visible: !browserDialog.compactHeight || browserDialog.addressVisible

            RowLayout {
                Layout.fillWidth: true
                spacing: Metrics.spacingLg

                ComboBox {
                    id: browserEntrySelector

                    objectName: "browserEntrySelector"
                    Layout.fillWidth: true
                    textRole: "name"
                    model: schedule.importExport.browserEntries
                    font.pixelSize: Typography.fontBody
                }

                Button {
                    id: browserOpenEntryButton

                    objectName: "browserOpenEntryButton"
                    text: qsTr("打开")
                    Layout.preferredHeight: Metrics.controlHeight
                    font.pixelSize: Typography.fontBodyLarge
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Metrics.spacingLg

                TextField {
                    id: browserUrlField

                    objectName: "browserUrlField"
                    Layout.fillWidth: true
                    placeholderText: qsTr("教务系统地址（可直接粘贴课表页网址）")
                    font.pixelSize: Typography.fontBody
                    selectByMouse: true
                }

                ToolButton {
                    id: browserGoButton

                    objectName: "browserGoButton"
                    text: qsTr("前往")
                    flat: true
                    Layout.preferredHeight: Metrics.controlHeight
                    font.pixelSize: Typography.fontBodyLarge
                }
            }

            Label {
                Layout.fillWidth: true
                visible: !schedule.importExport.hasEmbeddedBrowser
                text: qsTr("当前构建未启用内嵌浏览器：用系统浏览器打开并另存课表页面，再用「文件导入」选择该文件。")
                wrapMode: Text.WordWrap
                color: Theme.warning
                font.pixelSize: Typography.fontSmall
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("只读取当前页面 HTML，不保存密码或 Cookie")
                color: Theme.textMuted
                font.pixelSize: Typography.fontTiny
                elide: Text.ElideRight
            }
        }

        // ---------------------------------------------------------------- 网页
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: browserDialog.horizontalInset
            Layout.rightMargin: browserDialog.horizontalInset
            Layout.topMargin: Metrics.spacingMd
            Layout.minimumHeight: 120
            visible: schedule.importExport.hasEmbeddedBrowser
            color: Theme.surfaceAlt
            border.width: Metrics.borderWidth
            border.color: Theme.border
            radius: Metrics.radiusSm
            clip: true

            EmbeddedBrowser {
                id: scheduleBrowser

                anchors.fill: parent
                clip: true
                layer.enabled: true
            }
        }

        // ---------------------------------------------------------------- 状态行
        Label {
            Layout.fillWidth: true
            visible: !browserDialog.compactHeight
            Layout.leftMargin: browserDialog.horizontalInset
            Layout.rightMargin: browserDialog.horizontalInset
            Layout.topMargin: Metrics.spacingSm
            text: schedule.importExport.webCaptureSummary.length > 0
                  ? schedule.importExport.webCaptureSummary
                  : (scheduleBrowser.pageLoading
                     ? qsTr("正在加载页面…")
                     : (scheduleBrowser.lastError.length > 0
                        ? scheduleBrowser.lastError
                        : (scheduleBrowser.pageTitle.length > 0
                           ? qsTr("当前页面：%1").arg(scheduleBrowser.pageTitle)
                           : qsTr("尚未打开任何页面"))))
            color: schedule.importExport.webCaptureSummary.length > 0
                   ? (schedule.importExport.hasPendingPreview ? Theme.success : Theme.danger)
                   : (scheduleBrowser.lastError.length > 0 ? Theme.danger : Theme.textMuted)
            font.pixelSize: Typography.fontSmall
            elide: Text.ElideRight
        }

        // ---------------------------------------------------------------- 底栏
        Rectangle {
            Layout.fillWidth: true
            // 横屏高度紧张：收起底栏，把刷新放到顶栏，网页区拿到全部剩余高度
            visible: !browserDialog.compactHeight
            Layout.preferredHeight: Metrics.bottomBarHeight
            color: Theme.surface

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: Metrics.borderWidth
                color: Theme.divider
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: browserDialog.horizontalInset
                anchors.rightMargin: browserDialog.horizontalInset
                spacing: Metrics.spacingSm

                ToolButton {
                    id: browserReloadButton

                    objectName: "browserReloadButton"
                    text: qsTr("刷新")
                    flat: true
                    Layout.preferredHeight: Metrics.toolButtonSize
                    font.pixelSize: Typography.fontSmall
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("重新加载")
                }

                ToolButton {
                    objectName: "browserZoomOutButton"
                    text: "−"
                    flat: true
                    Layout.preferredWidth: Metrics.toolButtonSize
                    Layout.preferredHeight: Metrics.toolButtonSize
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("缩小网页")
                }

                Label {
                    text: Math.round(scheduleBrowser.zoomFactor * 100) + "%"
                    color: Theme.textMuted
                    font.pixelSize: Typography.fontSmall
                    horizontalAlignment: Text.AlignHCenter
                    Layout.preferredWidth: Metrics.zoomLabelWidth
                }

                ToolButton {
                    id: browserZoomInButton

                    objectName: "browserZoomInButton"
                    text: "+"
                    flat: true
                    Layout.preferredWidth: Metrics.toolButtonSize
                    Layout.preferredHeight: Metrics.toolButtonSize
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("放大网页")
                }

                ToolButton {
                    objectName: "browserZoomResetButton"
                    text: qsTr("1:1")
                    flat: true
                    Layout.preferredHeight: Metrics.toolButtonSize
                    font.pixelSize: Typography.fontSmall
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("重置缩放")
                }

                Item {
                    Layout.fillWidth: true
                }

                ToolButton {
                    id: browserSystemOpenButton

                    objectName: "browserSystemOpenButton"
                    text: qsTr("系统浏览器")
                    flat: true
                    Layout.preferredHeight: Metrics.toolButtonSize
                    font.pixelSize: Typography.fontSmall
                    ToolTip.visible: hovered
                    ToolTip.text: schedule.importExport.hasEmbeddedBrowser
                                  ? qsTr("仅用于在系统浏览器中查看页面")
                                  : qsTr("内嵌浏览器不可用时的查看通道")
                }
            }
        }
    }
}
