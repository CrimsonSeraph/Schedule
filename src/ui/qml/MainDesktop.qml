import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 桌面 / 平板主界面：顶部工具栏（周次 + 导航 + 操作）+ 中央页面栈 + 三个对话框。
//
// 交互约定（项目硬性规范）：
//  - 本文件**没有**任何 onClicked / Connections / onXxx 处理器；
//  - 每个交互控件都有稳定的 objectName，由 C++ 侧（app 层 UiConnector）在
//    `engine.load()` 之后显式建立连接；
//  - 界面只通过属性绑定读取 `schedule` / `bridge` 的状态。
ApplicationWindow {
    id: root

    objectName: "mainWindow"
    width: 1180
    height: 760
    minimumWidth: 900
    minimumHeight: 600
    visible: true
    title: qsTr("Schedule - 课表")

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            spacing: 8

            Label {
                text: qsTr("课表")
                font.bold: true
                font.pixelSize: 18
                leftPadding: 8
            }

            Label {
                text: schedule.hasSemester
                      ? qsTr("%1 · 共 %2 周").arg(schedule.semesterName).arg(schedule.totalWeeks)
                      : qsTr("尚未设置学期")
                color: "#5A6A80"
                elide: Text.ElideRight
                Layout.maximumWidth: 260
            }

            ToolSeparator {}

            ToolButton {
                id: prevWeekButton

                objectName: "prevWeekButton"
                text: qsTr("◀ 上一周")
            }

            ComboBox {
                id: weekSelector

                objectName: "weekSelector"
                Layout.preferredWidth: 150
                textRole: "label"
                valueRole: "value"
                model: schedule.weekOptions
                currentIndex: Math.max(0, schedule.selectedWeek - 1)
            }

            ToolButton {
                id: nextWeekButton

                objectName: "nextWeekButton"
                text: qsTr("下一周 ▶")
            }

            ToolButton {
                id: currentWeekButton

                objectName: "currentWeekButton"
                text: schedule.currentWeek > 0 ? qsTr("回到第 %1 周").arg(schedule.currentWeek) : qsTr("回到本周")
            }

            ToolSeparator {}

            ToolButton {
                id: navWeekButton

                objectName: "navWeekButton"
                text: qsTr("周视图")
                checkable: true
                checked: pageStack.currentIndex === 0
            }

            ToolButton {
                id: navDayButton

                objectName: "navDayButton"
                text: qsTr("日视图")
                checkable: true
                checked: pageStack.currentIndex === 1
            }

            ToolButton {
                id: navSemesterButton

                objectName: "navSemesterButton"
                text: qsTr("学期与课程")
                checkable: true
                checked: pageStack.currentIndex === 2
            }

            ToolButton {
                id: navSettingsButton

                objectName: "navSettingsButton"
                text: qsTr("设置")
                checkable: true
                checked: pageStack.currentIndex === 3
            }

            Item { Layout.fillWidth: true }

            Label {
                id: conflictBadge

                objectName: "conflictBadge"
                text: schedule.conflictSummary
                color: schedule.hasBlockingConflicts ? "#C0392B" : "#2E7D5B"
                font.bold: schedule.hasBlockingConflicts
            }

            ToolSeparator {}

            ToolButton {
                id: addCourseButton

                objectName: "addCourseButton"
                text: qsTr("＋ 新建课程")
            }

            ToolButton {
                id: importButton

                objectName: "importButton"
                text: qsTr("导入…")
            }

            ToolButton {
                id: exportButton

                objectName: "exportButton"
                text: qsTr("导出…")
            }
        }
    }

    // 中央页面栈：C++ 侧设置 currentIndex 完成导航
    StackLayout {
        id: pageStack

        objectName: "pageStack"
        anchors.fill: parent
        currentIndex: 0

        WeekView {
            id: weekPage
        }

        DayView {
            id: dayPage
        }

        SemesterPage {
            id: semesterPage
        }

        SettingsPage {
            id: settingsPage
        }
    }

    footer: ToolBar {
        RowLayout {
            anchors.fill: parent
            spacing: 8

            Label {
                id: statusLabel

                objectName: "statusLabel"
                Layout.fillWidth: true
                leftPadding: 8
                elide: Text.ElideRight
                text: schedule.lastError.length > 0
                      ? qsTr("⚠ %1").arg(schedule.lastError)
                      : (schedule.lastInfo.length > 0 ? qsTr("✓ %1").arg(schedule.lastInfo) : qsTr("就绪"))
                color: schedule.lastError.length > 0 ? "#C0392B" : "#5A6A80"
            }

            Label {
                text: qsTr("第 %1 周 · %2").arg(schedule.selectedWeek).arg(schedule.selectedWeekRange)
                color: "#8A97A8"
            }
        }
    }


    // 应用内提醒横幅：C++ 侧（UiConnector）在收到 notificationRequested 后写入
    // bannerTitle / bannerMessage，并在若干秒后清空。系统通知不可用时它是兜底展示。
    Rectangle {
        id: notificationBanner

        objectName: "notificationBanner"
        property string bannerTitle: ""
        property string bannerMessage: ""

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 20
        width: Math.min(parent.width - 48, 460)
        height: notificationBanner.bannerTitle.length > 0 ? 76 : 0
        visible: height > 0
        radius: 10
        color: "#1F2A44"
        opacity: 0.97

        Column {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 4

            Text {
                width: parent.width
                text: notificationBanner.bannerTitle
                color: "white"
                font.bold: true
                font.pixelSize: 14
                elide: Text.ElideRight
            }

            Text {
                width: parent.width
                text: notificationBanner.bannerMessage
                color: "#C9D6EA"
                font.pixelSize: 12
                elide: Text.ElideRight
            }
        }
    }

    // 各页共用的对话框
    CourseEditor {
        id: courseEditor
    }

    ImportWizard {
        id: importWizard
    }

    ExportDialog {
        id: exportDialog
    }
}
