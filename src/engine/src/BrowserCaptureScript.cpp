#include "engine/BrowserCaptureScript.h"

namespace Schedule {

    namespace {

        /**
         * @brief 悬浮按钮的安装片段：自包含 IIFE，幂等，可重复注入。
         *
         * 两个公开脚本都复用它，保证按钮 id / 文案 / 点击行为只有一份定义。
         */
        const QString& install_button_snippet() {
            static const QString snippet = QStringLiteral(R"JS(
(function () {
    var BUTTON_ID = 'schedule-grab-button';
    var TOAST_ID = 'schedule-grab-toast';
    var PAYLOAD_KEY = '__scheduleCapturePayload';

    function collectPage() {
        if (!document || !document.documentElement) {
            return '';
        }
        return document.documentElement.outerHTML || '';
    }

    function showToast(text) {
        var element = document.getElementById(TOAST_ID);
        if (!element) {
            element = document.createElement('div');
            element.id = TOAST_ID;
            element.style.cssText = 'position:fixed;left:50%;bottom:96px;transform:translateX(-50%);' +
                'z-index:2147483647;background:rgba(31,42,68,.92);color:#fff;padding:10px 16px;' +
                'border-radius:6px;font-size:14px;line-height:1.5;max-width:70%;text-align:center;' +
                'pointer-events:none;';
            document.body.appendChild(element);
        }
        element.textContent = text;
        element.style.display = 'block';
        window.setTimeout(function () { element.style.display = 'none'; }, 4000);
    }

    if (!document.body || document.getElementById(BUTTON_ID)) {
        return;
    }
    var button = document.createElement('button');
    button.id = BUTTON_ID;
    button.type = 'button';
    button.textContent = '抓取课表';
    button.style.cssText = 'position:fixed;right:16px;bottom:16px;z-index:2147483647;' +
        'padding:10px 18px;border:0;border-radius:24px;background:#4C8DFF;color:#fff;' +
        'font-size:15px;box-shadow:0 4px 14px rgba(0,0,0,.25);cursor:pointer;';
    button.addEventListener('click', function () {
        window[PAYLOAD_KEY] = collectPage();
        showToast('已抓取课表，请回到应用窗口点「导入课表」');
    });
    document.body.appendChild(button);
})();
)JS");
            return snippet;
        }

    } // namespace

    QString browser_inject_button_script() {
        return install_button_snippet();
    }

    QString browser_capture_script() {
        // 注入（幂等）+ 立即抓取当前页面原文
        static const QString capture = QStringLiteral(R"JS(
(function () {
    if (!document || !document.documentElement) {
        return '';
    }
    window.__scheduleCapturePayload = document.documentElement.outerHTML || '';
    return window.__scheduleCapturePayload;
})();
)JS");
        return install_button_snippet() + capture;
    }

} // namespace Schedule
