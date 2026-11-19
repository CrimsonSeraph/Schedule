pragma Singleton
import QtQuick

// 字号令牌
QtObject {
    // 字体缩放：MainMobile / MainDesktop 把 schedule.fontScale 绑定到这里，
    // 所有 font* 令牌按比例放大，设置页的“字体大小”即可全局生效。
    property real scale: 1.0

    // 字号（基准值 × 缩放）
    readonly property int fontTiny: Math.round(9 * scale)
    readonly property int fontCaption: Math.round(10 * scale)
    readonly property int fontSmall: Math.round(11 * scale)
    readonly property int fontBody: Math.round(12 * scale)
    readonly property int fontBodyLarge: Math.round(13 * scale)
    readonly property int fontSubheading: Math.round(14 * scale)
    readonly property int fontHeading: Math.round(16 * scale)
    readonly property int fontTitle: Math.round(18 * scale)
}
