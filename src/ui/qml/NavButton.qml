import QtQuick
import QtQuick.Controls

// 底部导航按钮：文字 + 选中指示条。
// 交互由 C++ 侧按 objectName 连接 clicked()，本文件不写信号处理器。
ToolButton {
    id: navButton

    property string label: ""
    property bool selected: false

    flat: true
    // 去掉 Material 默认的按钮底色，导航只保留文字与选中指示条
    background: Item { }

    contentItem: Column {
        anchors.centerIn: parent
        spacing: Metrics.spacingXs

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: navButton.label
            color: navButton.selected ? Theme.accent : Theme.textMuted
            font.pixelSize: navButton.selected ? Typography.fontBodyLarge : Typography.fontBody
            font.bold: navButton.selected
        }

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 18
            height: 2
            radius: 1
            color: Theme.accent
            visible: navButton.selected
        }
    }
}
