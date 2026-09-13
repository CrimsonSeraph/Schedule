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
//
// 样式：尺寸阈值 / 内边距 / 圆角取 CourseCardStyle，文字色取 Theme.cardText*。
Rectangle {
    id: card

    property string courseId: ""
    property string courseName: ""
    property string teacher: ""
    property string location: ""
    property string timeText: ""
    property string weeksText: ""
    property color cardColor: Theme.accent
    property bool compact: false

    // 偏窄 / 偏矮阈值：低于它们就不再展示地点、教师等次要信息。
    // 阈值按“完整信息（课程名 + 地点 + 教师 + 时间 + 周次）实际需要约 92px”标定，
    // 因此矮于 denseHeight 时提前降级，避免最后一行被 clip 裁掉。
    property int denseWidth: CourseCardStyle.denseWidth
    property int denseHeight: CourseCardStyle.denseHeight

    // 极窄 / 极矮阈值：再低于它们只保留课程名与时间
    property int tightWidth: CourseCardStyle.tightWidth
    property int tightHeight: CourseCardStyle.tightHeight

    readonly property bool dense: card.compact || card.width < card.denseWidth || card.height < card.denseHeight
    readonly property bool tight: card.width < card.tightWidth || card.height < card.tightHeight

    radius: card.tight ? CourseCardStyle.tightRadius : CourseCardStyle.radius
    color: cardColor
    border.width: CourseCardStyle.borderWidth
    border.color: Qt.darker(cardColor, CourseCardStyle.borderDarken)
    clip: true

    Column {
        id: content

        anchors.fill: parent
        anchors.margins: card.tight ? CourseCardStyle.tightPadding : (card.dense ? CourseCardStyle.densePadding : CourseCardStyle.padding)
        spacing: card.dense ? CourseCardStyle.denseSpacing : CourseCardStyle.spacing

        Text {
            width: parent.width
            text: card.courseName
            color: Theme.cardTextPrimary
            font.bold: true
            font.pixelSize: card.tight ? Typography.fontSmall : (card.dense ? Typography.fontBody : Typography.fontSubheading)
            elide: Text.ElideRight
            // 极矮卡片只留一行标题，避免第二行被裁掉
            maximumLineCount: card.tight ? 1 : 2
            wrapMode: Text.WordWrap
        }

        Text {
            width: parent.width
            visible: !card.dense && card.location.length > 0
            text: "📍 " + card.location
            color: Theme.cardTextSecondary
            font.pixelSize: Typography.fontSmall
            elide: Text.ElideRight
        }

        Text {
            width: parent.width
            visible: !card.dense && card.teacher.length > 0
            text: "👤 " + card.teacher
            color: Theme.cardTextSecondary
            font.pixelSize: Typography.fontSmall
            elide: Text.ElideRight
        }

        Text {
            width: parent.width
            visible: card.timeText.length > 0
            text: card.timeText
            color: Theme.cardTextTertiary
            font.pixelSize: card.tight ? Typography.fontCaption : Typography.fontSmall
            elide: Text.ElideRight
        }

        Text {
            width: parent.width
            visible: !card.tight && card.weeksText.length > 0
            text: card.weeksText
            color: Theme.cardTextMuted
            font.pixelSize: Typography.fontCaption
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
