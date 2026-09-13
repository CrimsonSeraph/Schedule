pragma Singleton
import QtQuick

// 课卡（CourseCard）专属令牌：高度推导参数 / 紧凑阈值 / 内边距 / 圆角。
//
// 颜色不在这里：卡片背景由数据层给出（`model.color`），文字色取 Theme.cardText*。
QtObject {
    // -------------------------------------------------- 日视图卡片高度推导
    // 高度 = clamp(可用高度 / 目标可见张数, minHeight, maxHeight)，见 Responsive.dayCardHeight()
    readonly property int preferredHeight: 96
    readonly property int minHeight: 60
    readonly property int maxHeight: 112
    readonly property int visibleTarget: 3
    readonly property int selectorBarHeight: 64

    // -------------------------------------------------------- 紧凑程度阈值
    // 低于这些宽高就不再展示地点 / 教师（完整信息（名称+地点+教师+时间+周次）约需 92px 高）
    readonly property int denseWidth: 104
    readonly property int denseHeight: 96
    // 低于这些宽高只保留课程名与时间
    readonly property int tightWidth: 84
    readonly property int tightHeight: 44
    // 周视图按卡片高度显式降级为 compact 的阈值
    readonly property int compactHeight: 58

    // -------------------------------------------- 网格中的位置与最小渲染尺寸
    // 卡片相对星期列 / 节次行的内缩；相邻卡片之间因此有 2 * inset 的间隙
    readonly property int inset: 2
    // 列宽或行高不足时仍保留的最小渲染尺寸
    readonly property int minRenderWidth: 24
    readonly property int minRenderHeight: 24

    // ------------------------------------------------------ 内边距 / 行距
    readonly property int tightPadding: 3
    readonly property int densePadding: Metrics.spacingSm
    readonly property int padding: Metrics.spacingMd
    readonly property int denseSpacing: Metrics.spacing2xs
    readonly property int spacing: Metrics.spacingXs

    // ---------------------------------------------------------- 圆角 / 描边
    readonly property int radius: Metrics.radiusMd
    readonly property int tightRadius: Metrics.radiusSm
    readonly property int borderWidth: Metrics.borderWidth
    // 卡片描边由背景色加深得到，保证深色课卡也有可见边界
    readonly property real borderDarken: 1.35
}
