import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Schedule 1.0

ApplicationWindow {
    id: root
    width: 480
    height: 800
    visible: true
    title: qsTr("Schedule - Mobile")

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
        anchors.fill: parent
        anchors.margins: 24
        spacing: 32

        Item { Layout.fillHeight: true }

        Label {
            text: qsTr("当前版本: %1").arg(bridge.version)
            font.pixelSize: 24
            Layout.alignment: Qt.AlignHCenter
        }

        Button {
            id: testButton
            objectName: "testButton"
            text: qsTr("测试")
            Layout.alignment: Qt.AlignHCenter
            Layout.fillWidth: true
            Layout.preferredHeight: 56

            onClicked: bridge.testButtonClicked()
        }

        Item { Layout.fillHeight: true }
    }
}
