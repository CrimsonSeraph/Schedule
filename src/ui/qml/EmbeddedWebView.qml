import QtQuick
import QtWebView

// 内嵌浏览器后端：Qt WebView。
//
// Qt WebView 是「外壳 + 平台后端」结构：Android 用系统 WebView、iOS/macOS 用 WKWebView、
// 桌面用 <Qt>/plugins/webview 下的插件（官方 Windows 包为 qtwebview_webengine）。
// 因此**只有确认后端插件存在时**根 CMakeLists 才会选中本文件（见 SCHEDULE_HAS_WEBVIEW_BACKEND）。
//
// 本文件实现 EmbeddedBrowser 约定的统一接口，且不写任何信号处理器。
Item {
    id: backend

    property url currentUrl: ""
    property string pageTitle: ""
    property bool pageLoading: false
    property string lastError: ""
    property real zoomFactor: 1.0

    property real appliedZoomFactor: 0

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

    function applyZoom() {
        const z = backend.zoomFactor;
        if (Math.abs(z - backend.appliedZoomFactor) < 0.001) {
            return;
        }
        backend.appliedZoomFactor = z;
        const js = "(function(){" + "  var z = " + z.toFixed(3) + ";" + "  var h = document.documentElement;" + "  if (!h) return;" + "  h.style.zoom = z;" + "})();";
        view.runJavaScript(js, null);
    }

    WebView {
        id: view

        objectName: "scheduleWebView"
        anchors.fill: parent
        url: ""
        onLoadingChanged: {
            if (!loading) {
                backend.appliedZoomFactor = 0;
                backend.applyZoom();
            }
        }
    }

    onZoomFactorChanged: backend.applyZoom()

    // 页面元信息通过声明式绑定回灌给统一接口（不写信号处理器）。
    // title / loading 在未加载时为默认值，绑定天然处理了「尚未导航」的情形。
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
