import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 日视图：只显示 `schedule.selectedDay` 当天的课程，纵列展示，适合手机与“今天上什么”场景。
//
// 数据来源：`schedule.sessionModel`（已按 `selectedDay` 过滤；为 0 时显示整周）。
// 交互约定：课程卡片由 CourseCard 提供 "sessionCardClick" 热区，C++ 侧显式连接。
Item {
    id: dayView

    objectName: "dayView"

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
            }

            ComboBox {
                id: daySelector

                objectName: "daySelector"
                Layout.preferredWidth: 140
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
                        height: 88

                        courseId: model.courseId
                        courseName: model.courseName
                        teacher: model.teacher
                        location: model.location
                        cardColor: model.color
                        timeText: model.dayName + " " + model.startTime + "-" + model.endTime
                                 + qsTr("（第 %1-%2 节）").arg(model.startSlot).arg(model.endSlot)
                        weeksText: model.weeksDisplay
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
