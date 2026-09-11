import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 日视图：只显示 `schedule.selectedDay` 当天的课程，纵列展示，适合手机与“今天上什么”场景。
//
// 数据来源：`schedule.sessionModel`（已按 `selectedDay` 过滤；为 0 时显示整周）。
//
// 响应式策略：
//  - 卡片高度不再固定 88px：按可用高度与“一屏目标卡片数”推导，并夹在
//    [minCardHeight, maxCardHeight] 之间，低高度屏幕（横屏 / 分屏）不会挤成一团；
//  - 卡片过矮时把 compact 传给 CourseCard，由后者同时参考宽高隐藏次要信息；
//  - 顶部星期选择栏在窄屏下收缩选择器宽度并省略右侧统计文本。
//
// 交互约定：课程卡片由 CourseCard 提供 "sessionCardClick" 热区，C++ 侧显式连接。
Item {
    id: dayView

    objectName: "dayView"

    // 卡片高度的首选值与上下限（像素）。
    // 96px 是“完整信息不裁切”的下限（见 CourseCard.denseHeight），因此首选值取 96。
    property int preferredCardHeight: 96
    property int minCardHeight: 60
    property int maxCardHeight: 112

    // 一屏希望完整看到的卡片数量：据此把卡片高度压到可用高度之内
    property int visibleCardTarget: 3

    // 顶部选择栏占用的高度（含外边距），用于估算列表可用高度
    readonly property int selectorBarHeight: 64

    readonly property int cardHeight: {
        const usable = Math.max(0, dayView.height - dayView.selectorBarHeight);
        const fitted = Math.round(usable / Math.max(1, dayView.visibleCardTarget));
        const preferred = Math.min(dayView.preferredCardHeight, fitted > 0 ? fitted : dayView.preferredCardHeight);
        return Math.max(dayView.minCardHeight, Math.min(dayView.maxCardHeight, preferred));
    }

    // 窄屏（手机竖屏 / 分屏）：压缩选择栏
    readonly property bool narrowBar: dayView.width < 360

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        // ---------------------------------------------------------------- 星期选择
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            Layout.topMargin: 10
            spacing: 8

            Label {
                text: qsTr("查看")
                color: "#33415C"
                // 超窄时省掉提示词，把宽度让给选择器
                visible: !dayView.narrowBar
            }

            ComboBox {
                id: daySelector

                objectName: "daySelector"
                // 宽度随可用宽度收缩，避免在小屏上把统计文本挤出窗口
                Layout.preferredWidth: Math.max(96, Math.min(140, Math.round(dayView.width * 0.34)))
                Layout.fillWidth: dayView.narrowBar
                textRole: "label"
                valueRole: "value"
                model: schedule.dayOptions
                currentIndex: schedule.selectedDay
            }

            Label {
                Layout.fillWidth: true
                text: schedule.selectedDay === 0
                      ? qsTr("整周共 %1 节课").arg(schedule.sessionModel.count)
                      : qsTr("%1 共 %2 节课").arg(schedule.day_name(schedule.selectedDay)).arg(schedule.sessionModel.count)
                color: "#6B7A90"
                horizontalAlignment: Text.AlignRight
                elide: Text.ElideRight
                visible: !dayView.narrowBar
            }
        }

        // ------------------------------------------------------------------ 课程列表
        ScrollView {
            id: dayScroll

            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            Layout.bottomMargin: 12
            clip: true

            // 关闭横向滚动条并让内容宽度直接跟随 ScrollView 宽度，
            // 避免 contentWidth 与 availableWidth 互相依赖造成绑定循环
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            Column {
                width: dayScroll.width
                spacing: 8

                Repeater {
                    model: schedule.sessionModel

                    delegate: CourseCard {
                        width: parent.width
                        height: dayView.cardHeight

                        courseId: model.courseId
                        courseName: model.courseName
                        teacher: model.teacher
                        location: model.location
                        cardColor: model.color
                        timeText: model.dayName + " " + model.startTime + "-" + model.endTime
                                 + qsTr("（第 %1-%2 节）").arg(model.startSlot).arg(model.endSlot)
                        weeksText: model.weeksDisplay
                        // 紧凑程度交给 CourseCard 按卡片实际宽高判定（宽卡片在全信息放不下时自动降级）
                    }
                }

                Label {
                    width: parent.width
                    visible: schedule.sessionModel.count === 0
                    text: qsTr("这一天还没有课程。点击顶部“新建课程”开始添加。")
                    color: "#8A97A8"
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                    topPadding: 24
                }
            }
        }
    }
}
