pragma Singleton
import QtQuick

// 颜色令牌
QtObject {
    // 文本
    readonly property color textPrimary: "#1F2A44"
    readonly property color textStrong: "#33415C"
    readonly property color textSecondary: "#5A6A80"
    readonly property color textMuted: "#8A97A8"
    readonly property color textSubtle: "#6B7A90"

    // 主色与状态
    readonly property color accent: "#4C8DFF"
    readonly property color accentStrong: "#1B4FA8"
    readonly property color danger: "#C0392B"
    readonly property color success: "#2E7D5B"
    readonly property color warning: "#B7791F"

    // 表面与描边
    readonly property color surface: "#FFFFFF"
    readonly property color surfaceAlt: "#F7F9FC"
    readonly property color surfaceSubtle: "#FBFCFE"
    readonly property color border: "#E3E9F2"
    readonly property color divider: "#EDF1F7"

    // 表头
    readonly property color headerBg: "#EEF3FB"
    readonly property color headerBgSelected: "#D6E4FF"
    readonly property color headerBorder: "#DCE3ED"

    // 选中态
    readonly property color selectionBg: "#E8F0FF"
    readonly property color selectionBorder: "#9FBEF5"

    // 课卡文字（卡片背景由 cardColor 动态给）
    readonly property color cardTextPrimary: "#FFFFFF"
    readonly property color cardTextSecondary: "#EAF1FF"
    readonly property color cardTextTertiary: "#D8E4FF"
    readonly property color cardTextMuted: "#CBD9FF"

    // 通知横幅
    readonly property color bannerTitleText: "#FFFFFF"
    readonly property color bannerSubText: "#C9D6EA"

    // 节次
    readonly property color slotTimeText: "#7A8798"
}
