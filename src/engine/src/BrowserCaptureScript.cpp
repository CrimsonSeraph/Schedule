#include "engine/BrowserCaptureScript.h"

namespace Schedule {

    QString browser_capture_script() {
        // 说明：这是一段**页面内**脚本，变量与函数都刻意做成自包含的 IIFE，
        // 避免与教务系统页面自身的全局变量（如 table0 / index / activity）冲突。
        static const QString script = QStringLiteral(R"JS(
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

    function installButton() {
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
    }

    installButton();
    // 直接把当前页面原文回传给应用；同时留一份供页面内按钮复用
    window[PAYLOAD_KEY] = collectPage();
    return window[PAYLOAD_KEY];
})();
)JS");

        return script;
    }

} // namespace Schedule
