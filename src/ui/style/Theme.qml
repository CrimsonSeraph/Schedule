pragma Singleton
import QtQuick

// 颜色令牌：浅色 / 深色两套。
//
// `dark` 由 MainDesktop / MainMobile 中的 Binding 绑定到 schedule.darkMode；
// 所有颜色都是只读绑定，切换主题时自动重算，QML 无需手工刷新。
QtObject {
    property bool dark: false

    // 文本
    readonly property color textPrimary: dark ? "#E8EDF5" : "#1F2A44"
    readonly property color textStrong: dark ? "#D5DDEA" : "#33415C"
    readonly property color textSecondary: dark ? "#A8B4C6" : "#5A6A80"
    readonly property color textMuted: dark ? "#7C8AA0" : "#8A97A8"
    readonly property color textSubtle: dark ? "#93A1B5" : "#6B7A90"

    // 主色与状态
    readonly property color accent: "#4C8DFF"
    readonly property color accentStrong: dark ? "#9CC4FF" : "#1B4FA8"
    readonly property color danger: dark ? "#FF8A80" : "#C0392B"
    readonly property color success: dark ? "#6FCF97" : "#2E7D5B"
    readonly property color warning: dark ? "#FFC46B" : "#B7791F"

    // 表面与描边
    readonly property color surface: dark ? "#151A23" : "#FFFFFF"
    readonly property color surfaceAlt: dark ? "#1B2230" : "#F7F9FC"
    readonly property color surfaceSubtle: dark ? "#1F2735" : "#FBFCFE"
    readonly property color border: dark ? "#2C3648" : "#E3E9F2"
    readonly property color divider: dark ? "#26303F" : "#EDF1F7"

    // 表头
    readonly property color headerBg: dark ? "#1D2635" : "#EEF3FB"
    readonly property color headerBgSelected: dark ? "#24344F" : "#D6E4FF"
    readonly property color headerBorder: dark ? "#2C3648" : "#DCE3ED"

    // 选中态
    readonly property color selectionBg: dark ? "#1F3350" : "#E8F0FF"
    readonly property color selectionBorder: dark ? "#4C8DFF" : "#9FBEF5"

    // 课卡文字（卡片背景由 cardColor 动态给）
    readonly property color cardTextPrimary: "#FFFFFF"
    readonly property color cardTextSecondary: "#EAF1FF"
    readonly property color cardTextTertiary: "#D8E4FF"
    readonly property color cardTextMuted: "#CBD9FF"

    // 通知横幅
    readonly property color bannerBg: dark ? "#2B3446" : "#1F2A44"
    readonly property color bannerTitleText: "#FFFFFF"
    readonly property color bannerSubText: "#C9D6EA"

    // 节次
    readonly property color slotTimeText: dark ? "#8B99AD" : "#7A8798"
}
