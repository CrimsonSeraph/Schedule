pragma Singleton
import QtQuick

// 列表项专属令牌：学期页课程列表、编辑器 / 详情弹层的时间段列表共用。
//
// 颜色取 Theme（surface* / selection* / border），这里只描述度量。
QtObject {
    // 行高：单行文本 40，带摘要的三行文本 62
    readonly property int height: 40
    readonly property int heightTall: 62

    // 圆角 / 描边
    readonly property int radius: Metrics.radiusSm
    readonly property int radiusTall: Metrics.radiusMd
    readonly property int borderWidth: Metrics.borderWidth

    // 左侧课程色条
    readonly property int colorBarWidth: 6
    readonly property int colorBarRadius: Metrics.radiusXs
    readonly property int colorBarInset: Metrics.spacingMd
    readonly property int colorBarVerticalInset: 16

    // 文本内边距 / 行距
    readonly property int padding: Metrics.spacingLg
    readonly property int textInset: Metrics.spacingXl
    readonly property int textSpacing: Metrics.spacingXs
    readonly property int spacing: Metrics.spacingSm

    // 时间段列表的可视高度（课程编辑器 / 课程详情弹层）
    readonly property int preferredHeight: 132
}
