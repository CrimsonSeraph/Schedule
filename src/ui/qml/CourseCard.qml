import QtQuick
import QtQuick.Controls

// 课程卡片：周视图与日视图中表示一次上课时间段。
//
// 响应式策略：
//  - `compact` 仍是调用方可显式指定的覆盖项（周视图按“卡片高度”传入）；
//  - 实际紧凑程度由 `dense` / `tight` 决定，同时参考**宽度与高度**：
//      * dense：卡片偏窄或偏矮 —— 隐藏地点、教师等次要信息；
//      * tight：卡片极窄或极矮 —— 再隐藏周次，标题只留一行。
//
// 交互约定：卡片内的 MouseArea 命名为 "sessionCardClick" 并暴露 courseId，
// 由 C++ 侧在 app 层扫描后显式连接到“打开课程详情弹层”，QML 不写 onClicked。
Rectangle {
    id: card

    property string courseId: ""
    property string courseName: ""
    property string teacher: ""
    property string location: ""
    property string timeText: ""
    property string weeksText: ""
    property color cardColor: Responsive.accent
    property bool compact: false

    // 偏窄 / 偏矮阈值：低于它们就不再展示地点、教师等次要信息。
    // 阈值按“完整信息（课程名 + 地点 + 教师 + 时间 + 周次）实际需要约 92px”标定，
    // 因此矮于 96px 时提前降级，避免最后一行被 clip 裁掉。
    property int denseWidth: Responsive.cardDenseWidth
    property int denseHeight: Responsive.cardDenseHeight

    // 极窄 / 极矮阈值：再低于它们只保留课程名与时间
    property int tightWidth: Responsive.cardTightWidth
    property int tightHeight: Responsive.cardTightHeight

    readonly property bool dense: card.compact || card.width < card.denseWidth || card.height < card.denseHeight
    readonly property bool tight: card.width < card.tightWidth || card.height < card.tightHeight

    radius: card.tight ? 4 : 6
    color: cardColor
    border.width: 1
    border.color: Qt.darker(cardColor, 1.35)
    clip: true

    Column {
        id: content

        anchors.fill: parent
        anchors.margins: card.tight ? 3 : (card.dense ? 4 : 6)
        spacing: card.dense ? 1 : 2

        Text {
            width: parent.width
            text: card.courseName
            color: "white"
            font.bold: true
            font.pixelSize: card.tight ? Responsive.fontSmall : (card.dense ? Responsive.fontBody : Responsive.fontSubheading)
            elide: Text.ElideRight
            // 极矮卡片只留一行标题，避免第二行被裁掉
            maximumLineCount: card.tight ? 1 : 2
            wrapMode: Text.WordWrap
        }

        Text {
            width: parent.width
            visible: !card.dense && card.location.length > 0
            text: "📍 " + card.location
            color: "#EAF1FF"
            font.pixelSize: Responsive.fontSmall
            elide: Text.ElideRight
        }

        Text {
            width: parent.width
            visible: !card.dense && card.teacher.length > 0
            text: "👤 " + card.teacher
            color: "#EAF1FF"
            font.pixelSize: Responsive.fontSmall
            elide: Text.ElideRight
        }

        Text {
            width: parent.width
            visible: card.timeText.length > 0
            text: card.timeText
            color: "#D8E4FF"
            font.pixelSize: card.tight ? Responsive.fontCaption : Responsive.fontSmall
            elide: Text.ElideRight
        }

        Text {
            width: parent.width
            visible: !card.tight && card.weeksText.length > 0
            text: card.weeksText
            color: "#CBD9FF"
            font.pixelSize: Responsive.fontCaption
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
