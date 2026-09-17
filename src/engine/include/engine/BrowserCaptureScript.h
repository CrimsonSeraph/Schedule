#pragma once

#include <QString>

namespace Schedule {

    /**
     * @brief 返回注入内嵌浏览器的**抓取脚本**。
     *
     * 脚本在内嵌浏览器加载页面后由用户触发执行（`EmbeddedBrowser::grabTimetable()`），
     * 职责只有两件：
     *
     *  1. 把**当前页面的 HTML 原文**回传给应用，交给 `ImportManager` 走与文件导入
     *     完全相同的「嗅探 → 解析 → 冲突检测 → 预览」流程；
     *  2. 在页面右下角注入一个悬浮的「抓取课表」按钮（幂等），让用户在页面内就能完成
     *     抓取并获得反馈——移动端上应用自身的对话框按钮可能被网页挤出可视区。
     *
     * ## 为什么必须抓「整页 HTML」而不是重新拼装课程数据
     *
     * 正方教务的课表页面把**逐周位图**放在内嵌脚本的 `new TaskActivity(...)` 参数里，
     * 而页面上渲染出来的表格只会显示折叠后的周次文本：单周课的单元格写作 `第1-11`
     * （真实含义是 1,3,5,7,9,11），双周课写作 `第2-12`。仅凭表格文本无法还原真实周次，
     * 因此抓取必须保留脚本原文，由 `ZhengfangTimetableIo` 按位图解析。
     *
     * 若某些页面完全在客户端拼装表格、HTML 里没有 `TaskActivity`，解析会失败并给出
     * 明确提示，引导用户改用教务系统的「导出」功能后走文件导入——而不是把折叠过的
     * 周次当成真实数据静默写入课表。
     *
     * ## 通信方式
     *
     * 采用 `runJavaScript(script, callback)` 的**拉取**模型，而不是 QWebChannel 推送：
     * Qt WebView（本项目的首选后端）并没有 `webChannel` 属性，只有 Qt WebEngine 支持。
     * 拉取模型在三种后端上行为一致，也不需要把任何 Qt 对象暴露给网页。
     * 脚本同时把结果写入 `window.__scheduleCapturePayload`，便于页面内按钮与
     * 应用侧取到同一份数据。
     *
     * ## 隐私
     *
     * 脚本只读取 DOM 文本，**不读取 Cookie、不读取 localStorage、不发任何网络请求**。
     */
    QString browser_capture_script();

    /**
     * @brief 返回**只注入悬浮按钮**的脚本，不抓取、不回传内容。
     *
     * 由宿主在"页面加载完成"时调用，让用户在页面内随时看得到「抓取课表」按钮；
     * 实际抓取仍走 `browser_capture_script()`（用户点应用侧「导入课表」时执行）。
     *
     * 与 `browser_capture_script()` 共享同一段安装片段，按钮幂等，重复调用安全。
     */
    QString browser_inject_button_script();


} // namespace Schedule
