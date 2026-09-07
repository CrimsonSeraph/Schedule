import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MyApp 1.0

ApplicationWindow {
    id: root
    width: 960
    height: 640
    visible: true
    title: qsTr("Schedule - Desktop")

    // 桥接对象：读取 version 属性、调用测试槽
    AppBridge {
        id: bridge
    }

    Connections {
        target: bridge
        function onTestSignal(msg) {
            console.log("[QML] testSignal:", msg)
        }
    }

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

            onClicked: bridge.testButtonClicked()
        }
    }
}
