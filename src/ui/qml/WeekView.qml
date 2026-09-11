import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 周视图：左侧节次栏 + 7 个星期列，课程卡片按“起始节次 × 节次高度”绝对定位。
//
// 数据来源：
//  - `schedule.weekModel` 始终是“整周”的排布（不随 `selectedDay` 变化）；
//  - `schedule.timeSlots` 提供节次标题与作息时间。
//
// 响应式策略：
//  - 列宽完全由可用宽度推导（不再强制最小 88px，避免右侧星期列被静默裁掉）；
//  - 宽度不足以放下可读列宽（< minDayWidth）时启用**横向滚动**，星期表头跟随
//    Flickable.contentX 同步位移，并给出“切换到日视图”的提示；
//  - `slotHeight` 是调用方给出的**首选**节次高度，实际渲染高度 `effectiveSlotHeight`
//    会按可用高度收缩：低高度窗口 / 横屏手机不会把整周挤出屏幕，下限由纵向滚动兜底。
//
// 交互约定：所有课程卡片由 CourseCard 提供 "sessionCardClick" 热区，
// C++ 侧（app 层 UiConnector）扫描并连接，QML 不写 onClicked / Connections。
Item {
    id: weekView

    objectName: "weekView"

    // 每一节的首选高度（像素）；实际高度见 effectiveSlotHeight
    property int slotHeight: 64

    // 每一节的最小高度：再矮就不再压缩，改用纵向滚动承载
    property int minSlotHeight: 34

    // 左侧节次栏宽度
    property int slotColumnWidth: 76

    // 星期表头高度
    property int headerHeight: 38

    // 单个星期列的最小可读宽度：再窄就横向滚动，不再继续压缩（常量见 Responsive）
    property int minDayWidth: Responsive.minDayWidth

    readonly property int dayCount: 7
    readonly property var periods: schedule.timeSlots

    // 实际节次高度：按可用高度收缩，让“整周”尽量一屏可见
    readonly property int effectiveSlotHeight: {
        const count = Math.max(1, weekView.periods.length);
        const usable = Math.max(0, weekView.height - weekView.headerHeight);
        const fitted = Math.floor(usable / count);
        const wanted = fitted > 0 ? Math.min(weekView.slotHeight, fitted) : weekView.slotHeight;
        return Math.max(weekView.minSlotHeight, wanted);
    }

    // 按可用宽度平分的列宽；小于 minDayWidth 时改为横向滚动
    readonly property real fittedDayWidth: Math.max(1, (weekView.width - weekView.slotColumnWidth) / weekView.dayCount)
    readonly property bool horizontallyScrollable: weekView.fittedDayWidth < weekView.minDayWidth
    readonly property real dayWidth: weekView.horizontallyScrollable ? weekView.minDayWidth : weekView.fittedDayWidth

    // 网格内容宽度：节次栏 + 7 列
    readonly property real gridContentWidth: weekView.slotColumnWidth + weekView.dayWidth * weekView.dayCount

    // 网格横向滚动偏移：星期表头据此同步位移，保证表头与列始终对齐
    readonly property real gridOffsetX: gridFlick.contentX

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // 横向滚动提示：纯展示文本，不带任何交互（不需要 objectName 与 C++ 连接）
        Label {
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10
            Layout.topMargin: 6
            visible: weekView.horizontallyScrollable
            text: qsTr("屏幕较窄：左右滑动查看整周，或切换到“日视图”")
            color: Responsive.warning
            font.pixelSize: Responsive.fontSmall
            elide: Text.ElideRight
        }

        // -------------------------------------------------------------- 星期表头
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: weekView.headerHeight
            clip: true

            Row {
                // 与网格横向滚动同步；x 为负表示内容向左滚出视口
                x: -weekView.gridOffsetX
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
                                font.pixelSize: weekView.dayWidth < 72 ? 11 : 13
                                color: (dayHeader.dayIndex === schedule.selectedDay) ? Responsive.accentStrong : Responsive.textStrong
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                // 列太窄时隐藏日期，只留星期，避免文字互相挤压
                                visible: weekView.dayWidth >= 52
                                text: schedule.week_date_text(schedule.selectedWeek, dayHeader.dayIndex)
                                font.pixelSize: Responsive.fontCaption
                                color: "#6B7A90"
                            }
                        }
                    }
                }
            }
        }

        // ---------------------------------------------------------------- 课表主体
        // 这里需要“列宽不足时横向滚动”，因此直接用 Flickable 显式声明 contentWidth /
        // contentHeight：横向偏移 contentX 可直接读取（星期表头据此同步），也避免
        // ScrollView 中 contentWidth 与 availableWidth 互相依赖造成的绑定循环。
        Flickable {
            id: gridFlick

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            // 内容宽度取“视口宽度”与“网格所需宽度”的较大者：
            //  - 列宽足够：铺满视口，不出现横向滚动；
            //  - 列宽不足：按最小可读列宽撑开，交给 Flickable 横向滚动。
            contentWidth: Math.max(gridFlick.width, weekView.gridContentWidth)
            contentHeight: Math.max(1, weekView.periods.length) * weekView.effectiveSlotHeight
            boundsBehavior: Flickable.StopAtBounds

            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
            }

            ScrollBar.horizontal: ScrollBar {
                policy: weekView.horizontallyScrollable ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
            }

            Row {
                width: gridFlick.contentWidth
                height: gridFlick.contentHeight
                spacing: 0

                // 左侧节次栏
                Column {
                    width: weekView.slotColumnWidth
                    spacing: 0

                    Repeater {
                        model: weekView.periods

                        delegate: Rectangle {
                            width: weekView.slotColumnWidth
                            height: weekView.effectiveSlotHeight
                            color: "#F7F9FC"
                            border.width: 1
                            border.color: Responsive.border

                            Column {
                                anchors.centerIn: parent
                                spacing: 1

                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: modelData.label
                                    font.pixelSize: Responsive.fontSmall
                                    font.bold: true
                                    color: Responsive.textStrong
                                }

                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    // 节次高度被压缩时省略上下课时间，优先保住节次名
                                    visible: weekView.effectiveSlotHeight >= 48 && modelData.start.length > 0
                                    text: modelData.start
                                    font.pixelSize: 9
                                    color: "#7A8798"
                                }

                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    visible: weekView.effectiveSlotHeight >= 48 && modelData.end.length > 0
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
                                y: index * weekView.effectiveSlotHeight
                                color: "#EDF1F7"
                            }
                        }

                        // 本列的课程卡片（整周模型 + 按星期过滤显示）
                        Repeater {
                            model: schedule.weekModel

                            delegate: CourseCard {
                                visible: model.dayOfWeek === dayColumn.dayIndex
                                x: 2
                                y: (model.startSlot - 1) * weekView.effectiveSlotHeight + 2
                                width: Math.max(24, dayColumn.width - 4)
                                height: Math.max(24, model.rowSpan * weekView.effectiveSlotHeight - 4)

                                courseId: model.courseId
                                courseName: model.courseName
                                teacher: model.teacher
                                location: model.location
                                cardColor: model.color
                                timeText: model.startTime + "-" + model.endTime
                                weeksText: model.weeksDisplay
                                // 显式覆盖仅在极矮卡片时生效，其余交给 CourseCard 的尺寸自适应
                                compact: height < 58
                            }
                        }
                    }
                }
            }
        }
    }
}
