import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    width: 960
    height: 640
    visible: true
    title: qsTr("Schedule - Desktop")

    // bridge 是 C++ 侧注入的上下文属性（见 src/app/main.cpp），QML 只读取其属性；
    // 按钮点击等信号连接均在 C++ 侧显式建立（QObject::connect），QML 不再隐式连接。
    ColumnLayout {
        anchors.centerIn: parent
        spacing: 24

        Label {
            text: qsTr("当前版本: %1").arg(bridge.version)
            font.pixelSize: 28
            Layout.alignment: Qt.AlignHCenter
        }

        Button {
            id: testButton
            objectName: "testButton"
            text: qsTr("测试")
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 180
            Layout.preferredHeight: 48
        }
    }
}
