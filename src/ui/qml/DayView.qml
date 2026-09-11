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

    // 卡片高度：按可用高度与“一屏目标卡片数”推导，上下限与目标张数都在 Responsive 里
    readonly property int cardHeight: Responsive.dayCardHeight(dayView.height)

    // 窄屏（手机竖屏 / 分屏）：压缩选择栏
    readonly property bool narrowBar: Responsive.isTiny(dayView.width)

    ColumnLayout {
        anchors.fill: parent
        spacing: Responsive.spacing

        // ---------------------------------------------------------------- 星期选择
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Responsive.margin
            Layout.rightMargin: Responsive.margin
            Layout.topMargin: 10
            spacing: Responsive.spacing

            Label {
                text: qsTr("查看")
                color: Responsive.textStrong
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
            Layout.leftMargin: Responsive.margin
            Layout.rightMargin: Responsive.margin
            Layout.bottomMargin: 12
            clip: true

            // 关闭横向滚动条并让内容宽度直接跟随 ScrollView 宽度，
            // 避免 contentWidth 与 availableWidth 互相依赖造成绑定循环
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            Column {
                width: dayScroll.width
                spacing: Responsive.spacing

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
                    color: Responsive.textMuted
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                    topPadding: 24
                }
            }
        }
    }
}
