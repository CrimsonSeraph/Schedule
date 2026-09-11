pragma Singleton

import QtQuick

// 全项目统一的响应式断点与设计常量（QML 单例，模块内直接以 `Responsive` 引用）。
//
// 使用约定：
//  - **断点值只在这里定义**，页面里通过 `Responsive.isXxx(...)` 或常量判断，
//    避免各页面各写一套魔法数字；
//  - 间距 / 字号 / 卡片尺寸 / 常用颜色同样集中在此，页面不要再写字面量；
//  - 本文件只放常量与纯函数，不引用 `schedule` / `reminders` 等上下文属性。
//
// 在 CMake 中通过 QT_QML_SINGLETON_TYPE 注册为单例（见 src/ui/CMakeLists.txt）。
QtObject {
    // ---------------------------------------------------------------- 窗口断点
    // 主窗口：完整工具栏需要的宽度（实测 header.implicitWidth ≈ 1401）
    readonly property int compactToolbarWidth: 1440

    // 主窗口：隐藏标题 / 学期回显等次要信息的宽度
    readonly property int narrowWidth: 800

    // 主窗口：开始压缩纵向占位的高度（横屏 / 分屏）
    readonly property int shortHeight: 520

    // 桌面窗口最小尺寸（低于此值工具栏与页面都无法保证可用）
    readonly property int minWindowWidth: 640
    readonly property int minWindowHeight: 420

    // 学期页：左右 / 上下分栏切换
    readonly property int wideSplitWidth: 900

    // 设置页：表单多列 / 单列切换
    readonly property int wideFormWidth: 640

    // 课程编辑器：4 列 / 2 列 / 1 列断点
    readonly property int editorWideWidth: 620
    readonly property int editorMediumWidth: 420

    // 导出对话框：标签在上 / 并排 的断点
    readonly property int dialogWideWidth: 460

    // 日视图 / 移动端：极端窄屏（选择栏收起、文案精简）
    readonly property int tinyWidth: 360

    // 周视图：单个星期列的最小可读宽度（再窄改为横向滚动）
    readonly property int minDayWidth: 64

    // ---------------------------------------------------------------- 间距
    readonly property int margin: 12
    readonly property int spacing: 8
    readonly property int sectionSpacing: 14

    // ---------------------------------------------------------------- 字号
    readonly property int fontTitle: 18
    readonly property int fontHeading: 16
    readonly property int fontSubheading: 14
    readonly property int fontBody: 12
    readonly property int fontSmall: 11
    readonly property int fontCaption: 10

    // ---------------------------------------------------------------- 卡片
    // 日视图：卡片高度按“列表可用高度 / 目标可见张数”推导后再夹紧
    readonly property int cardPreferredHeight: 96
    readonly property int cardMinHeight: 60
    readonly property int cardMaxHeight: 112
    readonly property int cardVisibleTarget: 3
    readonly property int cardSelectorBarHeight: 64

    // 课卡：低于这些宽高就不再展示地点 / 教师（完整信息约需 92px 高）
    readonly property int cardDenseWidth: 104
    readonly property int cardDenseHeight: 96
    // 课卡：低于这些宽高只保留课程名与时间
    readonly property int cardTightWidth: 84
    readonly property int cardTightHeight: 44

    // ---------------------------------------------------------------- 常用颜色
    readonly property color textPrimary: "#1F2A44"
    readonly property color textStrong: "#33415C"
    readonly property color textSecondary: "#5A6A80"
    readonly property color textMuted: "#8A97A8"
    readonly property color accent: "#4C8DFF"
    readonly property color accentStrong: "#1B4FA8"
    readonly property color danger: "#C0392B"
    readonly property color success: "#2E7D5B"
    readonly property color warning: "#B7791F"
    readonly property color border: "#E3E9F2"

    // ---------------------------------------------------------------- 派生判断
    /** @return 主窗口是否折叠次要操作。 */
    function isCompactToolbar(windowWidth) {
        return windowWidth < compactToolbarWidth;
    }

    /** @return 主窗口是否进入超窄形态。 */
    function isNarrow(windowWidth) {
        return windowWidth < narrowWidth;
    }

    /** @return 窗口高度是否不足以同时容纳头部、内容与底部。 */
    function isShort(windowHeight) {
        return windowHeight < shortHeight;
    }

    /** @return 是否为极端窄屏（移动端竖屏小尺寸 / 日视图选择栏）。 */
    function isTiny(width) {
        return width < tinyWidth;
    }

    /** @return 学期页是否左右分栏。 */
    function isWideSplit(width) {
        return width >= wideSplitWidth;
    }

    /** @return 对话框是否需要按窄表单排版。 */
    function isDialogNarrow(dialogWidth) {
        return dialogWidth < dialogWideWidth;
    }

    /** @return 课程编辑器表单列数（4 / 2 / 1）。 */
    function editorColumns(formWidth) {
        return formWidth >= editorWideWidth ? 4 : (formWidth >= editorMediumWidth ? 2 : 1);
    }

    /** @return 设置页“标签 + 输入框 + 按钮”表单列数（3 / 1）。 */
    function settingsColumns(formWidth) {
        return formWidth >= wideFormWidth ? 3 : 1;
    }

    /** @return 设置页“标签 + 输入框”表单列数（2 / 1）。 */
    function settingsPairColumns(formWidth) {
        return formWidth >= wideFormWidth ? 2 : 1;
    }

    /** @return 日视图卡片高度：按可用高度推导并夹在 [cardMinHeight, cardMaxHeight]。 */
    function dayCardHeight(pageHeight) {
        const usable = Math.max(0, pageHeight - cardSelectorBarHeight);
        const fitted = Math.round(usable / Math.max(1, cardVisibleTarget));
        const preferred = Math.min(cardPreferredHeight, fitted > 0 ? fitted : cardPreferredHeight);
        return Math.max(cardMinHeight, Math.min(cardMaxHeight, preferred));
    }
}
