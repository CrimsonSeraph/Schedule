import QtQuick
import QtWebEngineQuick

// 内嵌浏览器后端：Qt WebEngine。
//
// 用于没有可用 Qt WebView 后端的平台（典型是 Linux 桌面），自带 Chromium，
// 但会显著增大发行包体积，所以只在 WebView 不可用时才由根 CMakeLists 选中。
//
// 与 EmbeddedWebView.qml 实现同一套统一接口，且同样不写任何信号处理器。
Item {
    id: backend

    property url currentUrl: ""
    property string pageTitle: ""
    property bool pageLoading: false
    property string lastError: ""

    function loadUrl(target) {
        const text = String(target || "");
        if (text.length === 0) {
            backend.lastError = qsTr("地址为空");
            return;
        }
        backend.lastError = "";
        view.url = text;
    }

    function reload() {
        view.reload();
    }

    function runJavaScript(script, callback) {
        view.runJavaScript(script, callback);
    }

    /**
     * @brief 抓取当前页面的 HTML 原文（与 WebView 后端行为一致）。
     */
    function grabTimetable(callback) {
        view.runJavaScript("document.documentElement.outerHTML", function(result) {
            const html = result === undefined || result === null ? "" : String(result);
            if (html.length === 0) {
                callback(null, qsTr("页面内容为空，请确认已打开课表页面"));
                return;
            }
            callback(html, qsTr("已从当前页面抓取 %1 个字符").arg(html.length));
        });
    }

    WebEngineView {
        id: view

        objectName: "scheduleWebEngineView"
        anchors.fill: parent
    }

    Binding {
        target: backend
        property: "pageTitle"
        value: view.title
    }

    Binding {
        target: backend
        property: "pageLoading"
        value: view.loading
    }

    Binding {
        target: backend
        property: "currentUrl"
        value: view.url
    }
}
