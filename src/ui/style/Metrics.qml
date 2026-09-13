pragma Singleton
import QtQuick

// 通用设计令牌：间距刻度 / 圆角 / 描边 / 跨组件共用的尺寸。
//
// 分工约定：
//  - 颜色 → Theme，字号 → Typography，断点与派生函数 → Responsive；
//  - 课卡专属度量 → CourseCardStyle，列表项专属度量 → ListItemStyle；
//  - 本文件只放「通用值」，页面里不要再出现字面量。
QtObject {
    // -------------------------------------------------------------- 间距刻度
    readonly property int spacing2xs: 1
    readonly property int spacingXs: 2
    readonly property int spacingSm: 4
    readonly property int spacingMd: 6
    readonly property int spacingLg: 8
    readonly property int spacingXl: 10
    readonly property int spacing2xl: 12
    readonly property int spacing3xl: 14
    readonly property int spacing4xl: 24

    // ---------------------------------------------------------- 圆角 / 描边
    readonly property int radiusXs: 3
    readonly property int radiusSm: 4
    readonly property int radiusMd: 6
    readonly property int radiusLg: 10
    readonly property int borderWidth: 1

    // ------------------------------------------------------ 头部 / 工具栏
    // 工具栏里的学期回显最长宽度：再长就省略，避免把周次导航挤出去
    readonly property int headerEchoMaxWidth: 260
    // 周次下拉框宽度：紧凑形态（折叠次要操作）/ 完整形态
    readonly property int weekSelectorWidthCompact: 108
    readonly property int weekSelectorWidth: 150

    // -------------------------------------------------------------- 周视图
    readonly property int weekHeaderHeight: 38
    readonly property int weekSlotHeight: 64
    readonly property int weekSlotMinHeight: 34
    readonly property int weekSlotColumnWidth: 76

    // ---------------------------------------------------------- 侧栏 / 面板
    readonly property int sidePanelWidth: 320
    readonly property int sidePanelMinWidth: 260
    // 学期页窄屏（上下分栏）时表单区的高度上限 / 下限
    readonly property int stackedPanelHeight: 300
    readonly property int stackedPanelMinHeight: 200

    // ---------------------------------------------------------- 通知横幅
    readonly property int bannerHeight: 76
    readonly property int bannerMinWidth: 160
    readonly property int bannerMaxWidth: 460
    readonly property int bannerHorizontalGutter: 32
    readonly property int bannerBottomMargin: 20
    readonly property int bannerBottomMarginShort: 8
    readonly property real bannerOpacity: 0.97
}
