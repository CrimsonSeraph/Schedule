import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 周视图：左侧节次栏 + 7 个星期列，课程卡片按“起始节次 × 节次高度”绝对定位。
//
// 数据来源：
//  - `schedule.weekModel` 始终是“整周”的排布（不随 `selectedDay` 变化）；
//  - `schedule.timeSlots` 提供节次标题与作息时间。
//
// 交互约定：所有课程卡片由 CourseCard 提供 "sessionCardClick" 热区，
// C++ 侧（app 层 UiConnector）扫描并连接，QML 不写 onClicked / Connections。
Item {
    id: weekView

    objectName: "weekView"

    // 每一节的高度（像素）
    property int slotHeight: 64

    // 左侧节次栏宽度
    property int slotColumnWidth: 76

    // 星期表头高度
    property int headerHeight: 38

    readonly property int dayCount: 7
    readonly property var periods: schedule.timeSlots
    readonly property real dayWidth: Math.max(88, (width - slotColumnWidth) / dayCount)

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // -------------------------------------------------------------- 星期表头
        Row {
            Layout.fillWidth: true
            spacing: 0

            Item {
                width: weekView.slotColumnWidth
                height: weekView.headerHeight
            }

            Repeater {
                model: weekView.dayCount

                delegate: Rectangle {
                    id: dayHeader

                    property int dayIndex: index + 1

                    width: weekView.dayWidth
                    height: weekView.headerHeight
                    color: (dayHeader.dayIndex === schedule.selectedDay) ? "#D6E4FF" : "#EEF3FB"
                    border.width: 1
                    border.color: "#DCE3ED"

                    Column {
                        anchors.centerIn: parent
                        spacing: 0

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: schedule.day_name(dayHeader.dayIndex)
                            font.bold: true
                            font.pixelSize: 13
                            color: (dayHeader.dayIndex === schedule.selectedDay) ? "#1B4FA8" : "#33415C"
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: schedule.week_date_text(schedule.selectedWeek, dayHeader.dayIndex)
                            font.pixelSize: 10
                            color: "#6B7A90"
                        }
                    }
                }
            }
        }

        // ---------------------------------------------------------------- 课表主体
        ScrollView {
            id: gridScroll

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            // 关闭横向滚动条并让内容宽度直接跟随 ScrollView 宽度，
            // 避免 contentWidth 与 availableWidth 互相依赖造成绑定循环
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            Item {
                width: gridScroll.width
                height: Math.max(1, weekView.periods.length) * weekView.slotHeight

                Row {
                    anchors.fill: parent
                    spacing: 0

                    // 左侧节次栏
                    Column {
                        width: weekView.slotColumnWidth
                        spacing: 0

                        Repeater {
                            model: weekView.periods

                            delegate: Rectangle {
                                width: weekView.slotColumnWidth
                                height: weekView.slotHeight
                                color: "#F7F9FC"
                                border.width: 1
                                border.color: "#E3E9F2"

                                Column {
                                    anchors.centerIn: parent
                                    spacing: 1

                                    Text {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: modelData.label
                                        font.pixelSize: 11
                                        font.bold: true
                                        color: "#33415C"
                                    }

                                    Text {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        visible: modelData.start.length > 0
                                        text: modelData.start
                                        font.pixelSize: 9
                                        color: "#7A8798"
                                    }

                                    Text {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        visible: modelData.end.length > 0
                                        text: modelData.end
                                        font.pixelSize: 9
                                        color: "#7A8798"
                                    }
                                }
                            }
                        }
                    }

                    // 7 个星期列
                    Repeater {
                        model: weekView.dayCount

                        delegate: Item {
                            id: dayColumn

                            property int dayIndex: index + 1

                            width: weekView.dayWidth
                            height: parent.height

                            Rectangle {
                                anchors.fill: parent
                                color: (dayColumn.dayIndex % 2 === 0) ? "#FBFCFE" : "#FFFFFF"
                                border.width: 1
                                border.color: "#EDF1F7"
                            }

                            // 节次分隔线
                            Repeater {
                                model: weekView.periods.length

                                delegate: Rectangle {
                                    width: parent.width
                                    height: 1
                                    y: index * weekView.slotHeight
                                    color: "#EDF1F7"
                                }
                            }

                            // 本列的课程卡片（整周模型 + 按星期过滤显示）
                            Repeater {
                                model: schedule.weekModel

                                delegate: CourseCard {
                                    visible: model.dayOfWeek === dayColumn.dayIndex
                                    x: 2
                                    y: (model.startSlot - 1) * weekView.slotHeight + 2
                                    width: dayColumn.width - 4
                                    height: Math.max(30, model.rowSpan * weekView.slotHeight - 4)

                                    courseId: model.courseId
                                    courseName: model.courseName
                                    teacher: model.teacher
                                    location: model.location
                                    cardColor: model.color
                                    timeText: model.startTime + "-" + model.endTime
                                    weeksText: model.weeksDisplay
                                    compact: height < 58
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
