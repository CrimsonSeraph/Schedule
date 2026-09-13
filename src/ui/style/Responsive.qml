pragma Singleton
import QtQuick

// 断点、与断点相关的尺寸，以及派生判断（QML 单例，模块内直接以 `Responsive` 引用）。
//
// 使用约定：
//  - **断点值只在这里定义**，页面里通过 `Responsive.isXxx(...)` 或常量判断，
//    避免各页面各写一套魔法数字；
//  - 纯数值令牌分工：颜色 → Theme，字号 → Typography，间距 / 圆角 → Metrics，
//    课卡 → CourseCardStyle，列表项 → ListItemStyle；
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

    // 周视图：表头星期字号降一档的列宽，以及日期 / 节次时间的显示阈值
    readonly property int dayHeaderCompactWidth: 72
    readonly property int dayHeaderDateWidth: 52
    readonly property int slotTimeVisibleHeight: 48

    // ---------------------------------------------------------------- 响应式尺寸
    // 日视图星期选择器：宽度随可用宽度收缩，再夹在上下限之间
    readonly property int daySelectorMinWidth: 96
    readonly property int daySelectorMaxWidth: 140
    readonly property real daySelectorWidthRatio: 0.34

    // 周视图节次高度：桌面低高度窗口（横屏 / 分屏）压缩一档
    readonly property int desktopSlotHeightCompact: 52
    // 周视图节次高度 / 节次栏宽度：移动端竖屏与横屏（低高度）、极窄屏
    readonly property int mobileSlotHeight: 56
    readonly property int mobileSlotHeightCompact: 48
    readonly property int mobileSlotColumnWidth: 56
    readonly property int mobileSlotColumnWidthNarrow: 44

    // ---------------------------------------------------------------- 对话框
    // 尺寸限制在父窗口内时四周保留的边距
    readonly property int dialogEdgeInset: 40
    // 各对话框的首选尺寸（实际取 min(首选, 父窗口 - dialogEdgeInset)）
    readonly property int detailDialogWidth: 560
    readonly property int detailDialogHeight: 560
    readonly property int editorDialogWidth: 720
    readonly property int editorDialogHeight: 640
    readonly property int exportDialogWidth: 620
    readonly property int exportDialogHeight: 460
    readonly property int importDialogWidth: 680
    readonly property int importDialogHeight: 620

    // 对话框内的“结果 / 预览”区高度：按对话框高度比例取值并夹在上下限之间
    readonly property int exportPreviewMinHeight: 120
    readonly property int exportPreviewMaxHeight: 220
    readonly property real exportPreviewHeightRatio: 0.35
    readonly property int importPreviewMinHeight: 140
    readonly property int importPreviewMaxHeight: 260
    readonly property real importPreviewHeightRatio: 0.32
    readonly property int importPreviewLayoutMinimum: 120

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

    /**
     * @return 对话框宽度：首选宽度夹在「父窗口宽度 - dialogEdgeInset」内。
     *         parentWidth < 0 表示当前没有父窗口，直接返回首选宽度。
     */
    function dialogWidth(preferredWidth, parentWidth) {
        return parentWidth >= 0 ? Math.min(preferredWidth, parentWidth - dialogEdgeInset) : preferredWidth;
    }

    /** @return 对话框高度，规则同 dialogWidth()。 */
    function dialogHeight(preferredHeight, parentHeight) {
        return parentHeight >= 0 ? Math.min(preferredHeight, parentHeight - dialogEdgeInset) : preferredHeight;
    }

    /** @return 日视图星期选择器宽度：随可用宽度收缩并夹在 [Min, Max]。 */
    function daySelectorWidth(pageWidth) {
        return Math.max(daySelectorMinWidth, Math.min(daySelectorMaxWidth, Math.round(pageWidth * daySelectorWidthRatio)));
    }

    /** @return 应用内提醒横幅宽度：两侧至少留出 bannerHorizontalGutter。 */
    function bannerWidth(parentWidth) {
        return Math.max(Metrics.bannerMinWidth, Math.min(parentWidth - Metrics.bannerHorizontalGutter, Metrics.bannerMaxWidth));
    }

    /** @return 导出对话框「导出结果」区高度：按对话框高度推导并夹在上下限之间。 */
    function exportPreviewHeight(dialogHeight) {
        return Math.max(exportPreviewMinHeight, Math.min(exportPreviewMaxHeight, Math.round(dialogHeight * exportPreviewHeightRatio)));
    }

    /** @return 导入向导「预览与冲突检查」区高度：按对话框高度推导并夹在上下限之间。 */
    function importPreviewHeight(dialogHeight) {
        return Math.max(importPreviewMinHeight, Math.min(importPreviewMaxHeight, Math.round(dialogHeight * importPreviewHeightRatio)));
    }

    /** @return 日视图卡片高度：按可用高度推导并夹在 [minHeight, maxHeight]。 */
    function dayCardHeight(pageHeight) {
        const usable = Math.max(0, pageHeight - CourseCardStyle.selectorBarHeight);
        const fitted = Math.round(usable / Math.max(1, CourseCardStyle.visibleTarget));
        const preferred = Math.min(CourseCardStyle.preferredHeight, fitted > 0 ? fitted : CourseCardStyle.preferredHeight);
        return Math.max(CourseCardStyle.minHeight, Math.min(CourseCardStyle.maxHeight, preferred));
    }
}
