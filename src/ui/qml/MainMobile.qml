import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 移动端主界面：紧凑头部（周次切换）+ 中央页面栈 + 底部导航 + 三个对话框。
//
// 交互约定与桌面版一致：QML 不含任何信号处理器，全部交互由 C++ 侧 UiConnector 显式连接。
// 底部导航按钮与桌面版使用**相同的 objectName**，因此 C++ 连接代码可以复用。
ApplicationWindow {
    id: root

    objectName: "mainWindow"
    width: 480
    height: 860
    visible: true
    title: qsTr("Schedule - 课表")

    header: ToolBar {
        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                Label {
                    text: schedule.hasSemester ? schedule.semesterName : qsTr("尚未设置学期")
                    font.bold: true
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                    leftPadding: 8
                }

                Label {
                    text: schedule.conflictSummary
                    color: schedule.hasBlockingConflicts ? "#C0392B" : "#2E7D5B"
                    font.pixelSize: 11
                    rightPadding: 8
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 4

                ToolButton {
                    id: prevWeekButton

                    objectName: "prevWeekButton"
                    text: qsTr("◀")
                }

                ComboBox {
                    id: weekSelector

                    objectName: "weekSelector"
                    Layout.fillWidth: true
                    textRole: "label"
                    valueRole: "value"
                    model: schedule.weekOptions
                    currentIndex: Math.max(0, schedule.selectedWeek - 1)
                }

                ToolButton {
                    id: nextWeekButton

                    objectName: "nextWeekButton"
                    text: qsTr("▶")
                }

                ToolButton {
                    id: currentWeekButton

                    objectName: "currentWeekButton"
                    text: qsTr("本周")
                }
            }
        }
    }

    StackLayout {
        id: pageStack

        objectName: "pageStack"
        anchors.fill: parent
        currentIndex: 0

        WeekView {
            id: weekPage

            // 手机屏幕窄，压缩节次高度让整周尽量可见
            slotHeight: 56
            slotColumnWidth: 56
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

    footer: ColumnLayout {
        spacing: 0

        Label {
            id: statusLabel

            objectName: "statusLabel"
            Layout.fillWidth: true
            Layout.leftMargin: 8
            Layout.rightMargin: 8
            elide: Text.ElideRight
            font.pixelSize: 11
            text: schedule.lastError.length > 0
                  ? qsTr("⚠ %1").arg(schedule.lastError)
                  : (schedule.lastInfo.length > 0 ? qsTr("✓ %1").arg(schedule.lastInfo) : qsTr("就绪"))
            color: schedule.lastError.length > 0 ? "#C0392B" : "#5A6A80"
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 0

            ToolButton {
                id: navWeekButton

                objectName: "navWeekButton"
                Layout.fillWidth: true
                text: qsTr("周")
            }

            ToolButton {
                id: navDayButton

                objectName: "navDayButton"
                Layout.fillWidth: true
                text: qsTr("日")
            }

            ToolButton {
                id: navSemesterButton

                objectName: "navSemesterButton"
                Layout.fillWidth: true
                text: qsTr("课程")
            }

            ToolButton {
                id: navSettingsButton

                objectName: "navSettingsButton"
                Layout.fillWidth: true
                text: qsTr("设置")
            }

            ToolButton {
                id: addCourseButton

                objectName: "addCourseButton"
                Layout.fillWidth: true
                text: qsTr("新建")
            }

            ToolButton {
                id: importButton

                objectName: "importButton"
                Layout.fillWidth: true
                text: qsTr("导入")
            }

            ToolButton {
                id: exportButton

                objectName: "exportButton"
                Layout.fillWidth: true
                text: qsTr("导出")
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
