import QtQuick
import QtQuick.Controls

// 统一按钮样式：primary（蓝底白字）、secondary（扁平蓝字）、danger（扁平红字）。
//
// 交互仍由 C++ 侧按 objectName 连接 clicked()；本组件只负责视觉与高度。
Button {
    id: appButton

    property bool primary: false
    property bool danger: false

    implicitHeight: Metrics.controlHeight
    leftPadding: Metrics.spacingXl
    rightPadding: Metrics.spacingXl
    topPadding: Metrics.spacingSm
    bottomPadding: Metrics.spacingSm
    font.pixelSize: Typography.fontBodyLarge

    background: Rectangle {
        radius: Metrics.radiusMd
        color: appButton.primary
               ? (appButton.down ? Theme.accentStrong : Theme.accent)
               : (appButton.down ? Theme.surfaceAlt : "transparent")
    }

    contentItem: Text {
        text: appButton.text
        color: appButton.danger ? Theme.danger
               : (appButton.primary ? "#FFFFFF" : Theme.accent)
        font: appButton.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
