import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 内嵌浏览器后端：不可用时的兜底占位。
//
// 触发条件：构建时未找到可用的 Qt WebView 后端插件，也没有 Qt WebEngine
// （典型是官方 MinGW 套件）。此时**不影响构建与应用启动**：界面仍然完整可用，
// 只是把「在应用内抓取」换成「用系统浏览器打开 + 另存文件后导入」。
//
// 这里不提供任何交互控件（按钮由外层的 BrowserImportDialog 统一提供），
// 避免同一个动作出现两套入口。
Item {
    id: backend

    property url currentUrl: ""
    property string pageTitle: ""
    property bool pageLoading: false
    property string lastError: ""

    function loadUrl(target) {
        // 兜底后端无法承载页面内容，只记录原因，由外层引导用户改用系统浏览器
        backend.lastError = qsTr("当前构建未包含内嵌浏览器，请改用「用系统浏览器打开」后另存课表文件导入");
        backend.currentUrl = target;
    }

    function reload() {
    }

    function runJavaScript(script, callback) {
        backend.lastError = qsTr("当前构建未包含内嵌浏览器，无法从页面抓取");
        if (callback) {
            callback(null);
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Metrics.spacing2xl
        spacing: Metrics.spacingLg

        Label {
            Layout.fillWidth: true
            text: qsTr("当前构建未启用内嵌浏览器")
            font.pixelSize: Typography.fontSubheading
            font.bold: true
            color: Theme.textStrong
        }

        Label {
            Layout.fillWidth: true
            text: qsTr("本机 Qt 缺少可用的 WebView 后端插件，也没有 Qt WebEngine，因此无法在应用内直接打开教务页面。")
            wrapMode: Text.WordWrap
            color: Theme.textSecondary
            font.pixelSize: Typography.fontBody
        }

        Label {
            Layout.fillWidth: true
            text: qsTr("仍然可以导入课表：点「用系统浏览器打开」登录教务系统，停留在课表页面后把该页面另存为 .xls / .html 文件，再用「文件导入」选择它。")
            wrapMode: Text.WordWrap
            color: Theme.textSecondary
            font.pixelSize: Typography.fontBody
        }

        Label {
            Layout.fillWidth: true
            text: qsTr("若希望启用内嵌浏览器，请安装带 WebView 后端插件或 Qt WebEngine 的 Qt 套件后重新构建（CMake 会按 WebView → WebEngine 的顺序自动选择）。")
            wrapMode: Text.WordWrap
            color: Theme.textMuted
            font.pixelSize: Typography.fontSmall
        }

        Item {
            Layout.fillHeight: true
        }
    }
}
