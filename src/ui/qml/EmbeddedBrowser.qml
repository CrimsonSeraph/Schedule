import QtQuick

// 内嵌浏览器统一外壳。
//
// **后端由 C++ 侧在编译期决定**（`schedule.importExport.webBrowserBackend`，见根 CMakeLists
// 的 SCHEDULE_BROWSER_BACKEND），QML 只负责按名字挑实现文件：
//
//   - `webview`   → EmbeddedWebView.qml   （Qt WebView，复用系统原生 Web 组件，体积小）
//   - `webengine` → EmbeddedWebEngine.qml （Qt WebEngine，自带 Chromium）
//   - `none`      → EmbeddedFallback.qml  （提示改用系统浏览器 + 文件导入）
//
// 关键实现取舍：这里用 `Loader.source`（字符串）而不是内联 `Component`。
// 内联组件会让 QML 在解析本文件时就要求被引用类型存在；而未被选中的后端文件
// 根本不会被加进 QML 模块，解析必然失败。`Loader.source` 是**延迟**的：
// 只有真正选中的那份文件会被加载，其 `import QtWebView` / `import QtWebEngine` 也才会生效。
//
// 本文件对上层暴露**统一接口**，三种后端各自实现同名成员：
//
//   property url    currentUrl      当前地址
//   property string pageTitle       页面标题
//   property bool   pageLoading     是否正在加载
//   property string lastError       最近一次加载错误（空串表示无错误）
//   function loadUrl(url)           导航到指定地址
//   function reload()               重新加载当前页
//   function runJavaScript(script, callback)  在页面中执行脚本并回传结果
//
// 本文件**不写任何信号处理器**（项目规范）：交互一律由 app/UiConnector 在 C++ 侧连接。
Item {
    id: root

    objectName: "scheduleBrowser"

    // ---------------------------------------------------------------- 统一接口

    /** 编译期选定的后端名。 */
    readonly property string backend: schedule.importExport.webBrowserBackend

    /** 内嵌浏览器是否可用（`false` 时界面应引导用户改用系统浏览器）。 */
    readonly property bool available: schedule.importExport.hasEmbeddedBrowser

    /** 当前地址；由后端回灌。 */
    readonly property url currentUrl: backendLoader.item ? backendLoader.item.currentUrl : ""

    /** 页面标题；由后端回灌。 */
    readonly property string pageTitle: backendLoader.item ? backendLoader.item.pageTitle : ""

    /** 是否正在加载。 */
    readonly property bool pageLoading: backendLoader.item ? backendLoader.item.pageLoading : false

    /** 最近一次加载错误；空串表示无错误。 */
    readonly property string lastError: backendLoader.item ? backendLoader.item.lastError : ""

    /** 最近一次抓取到的页面原文（HTML）。 */
    property string capturedPayload

    /** 抓取完成；成功与否的说明见 webCaptureSummary，正文见 capturedPayload。 */
    signal captureFinished(bool success, string message)

    /** 当前后端的人类可读名称（界面提示用）。 */
    readonly property string backendLabel: root.backend === "webview" ? qsTr("系统原生 WebView")
        : root.backend === "webengine" ? qsTr("Qt WebEngine")
        : qsTr("未启用")

    // ---------------------------------------------------------------- 统一操作

    /** @brief 导航到指定地址；后端不可用或地址为空时记录错误。 */
    function loadUrl(target) {
        const item = backendLoader.item;
        if (!item) {
            root.capturedPayload = "";
            return;
        }
        item.loadUrl(target);
    }

    /** @brief 重新加载当前页。 */
    function reload() {
        const item = backendLoader.item;
        if (item) {
            item.reload();
        }
    }

    /**
     * @brief 在当前页面执行脚本。
     * @param script   要执行的 JavaScript
     * @param callback 形如 function(result) 的回调；后端不可用时以 null 调用
     */
    function runJavaScript(script, callback) {
        const item = backendLoader.item;
        if (!item) {
            if (callback) {
                callback(null);
            }
            return;
        }
        item.runJavaScript(script, callback);
    }

    /**
     * @brief 从**当前页面**抓取课表并回传。
     *
     * 抓取产物是页面原文（HTML），随后交给 C++ 侧走与文件导入完全相同的
     * 「嗅探 → 解析 → 冲突检测 → 预览」流程；应用不读取任何 Cookie。
     */
    function grabTimetable() {
        const item = backendLoader.item;
        if (!item) {
            root.capturedPayload = "";
            root.captureFinished(false, qsTr("内嵌浏览器不可用"));
            return;
        }
        item.grabTimetable(function(payload, message) {
            root.capturedPayload = payload ? payload : "";
            root.captureFinished(payload !== null && payload !== undefined, message);
        });
    }

    Loader {
        id: backendLoader

        objectName: "scheduleBrowserBackend"
        anchors.fill: parent

        // 相对路径基于本文件所在目录解析（同一 QML 模块内）
        source: root.backend === "webview" ? "EmbeddedWebView.qml" : root.backend === "webengine" ? "EmbeddedWebEngine.qml" : "EmbeddedFallback.qml"
    }
}
