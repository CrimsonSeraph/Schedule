import QtQuick
import QtQuick.Controls

// 课程卡片：周视图与日视图中表示一次上课时间段。
// 交互约定：卡片内的 MouseArea 命名为 "sessionCardClick" 并暴露 courseId，
// 由 C++ 侧在 app 层扫描后显式连接到“打开课程编辑器”，QML 不写 onClicked。
Rectangle {
    id: card

    property string courseId: ""
    property string courseName: ""
    property string teacher: ""
    property string location: ""
    property string timeText: ""
    property string weeksText: ""
    property color cardColor: "#4C8DFF"
    property bool compact: false

    radius: 6
    color: cardColor
    border.width: 1
    border.color: Qt.darker(cardColor, 1.35)
    clip: true

    Column {
        id: content

        anchors.fill: parent
        anchors.margins: card.compact ? 4 : 6
        spacing: 2

        Text {
            width: parent.width
            text: card.courseName
            color: "white"
            font.bold: true
            font.pixelSize: card.compact ? 12 : 14
            elide: Text.ElideRight
            maximumLineCount: 2
            wrapMode: Text.WordWrap
        }

        Text {
            width: parent.width
            visible: !card.compact && card.location.length > 0
            text: "📍 " + card.location
            color: "#EAF1FF"
            font.pixelSize: 11
            elide: Text.ElideRight
        }

        Text {
            width: parent.width
            visible: !card.compact && card.teacher.length > 0
            text: "👤 " + card.teacher
            color: "#EAF1FF"
            font.pixelSize: 11
            elide: Text.ElideRight
        }

        Text {
            width: parent.width
            visible: card.timeText.length > 0
            text: card.timeText
            color: "#D8E4FF"
            font.pixelSize: 11
            elide: Text.ElideRight
        }

        Text {
            width: parent.width
            visible: card.weeksText.length > 0
            text: card.weeksText
            color: "#CBD9FF"
            font.pixelSize: 10
            elide: Text.ElideRight
        }
    }

    MouseArea {
        id: cardClick

        objectName: "sessionCardClick"
        anchors.fill: parent
        // C++ 侧在点击时读取本属性，定位到要编辑的课程
        property string courseId: card.courseId
    }
}
