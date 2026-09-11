import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 移动端主界面：紧凑头部（周次切换）+ 中央页面栈 + 底部导航 + 三个对话框。
//
// 交互约定与桌面版一致：QML 不含任何信号处理器，全部交互由 C++ 侧 UiConnector 显式连接。
//
// 响应式约定：
//  - 底部导航精简为 4 个主 Tab（周 / 日 / 课程 / 设置），新建、导入、导出折叠进
//    “更多”折叠菜单，避免 7 个按钮在 320dp 宽的屏幕上互相挤压；
//  - 断点只依赖窗口宽高且来自 Responsive 单例，竖屏 / 横屏切换与折叠屏展开走同一条路径：
//      * narrow（宽度 < Responsive.tinyWidth）：极端窄屏，状态行与回显进一步精简；
//      * shortHeight（高度 < Responsive.shortHeight，例如横屏 844×390）：隐藏学期回显与状态行；
//      * landscape：横屏时节次高度进一步压缩，保证整周可见。
//  - 折叠菜单项（addCourseMenuItem / importMenuItem / exportMenuItem）与触发按钮
//    moreMenuButton 是**新增控件**，需要 C++ 侧（app 层 UiConnector）显式连接
//    `triggered()` 与 `clicked()`（打开菜单）；桌面版使用同一批 objectName，
//    因此两端只需要一套连接代码。
//  - 原本内联在底部导航的 addCourseButton / importButton / exportButton 已随“4 个主 Tab”
//    一起移出本文件；课程编辑入口仍然可用（学期页的 pageNewCourseButton 连接不变），
//    导入 / 导出由上面的菜单项承接。
ApplicationWindow {
    id: root

    objectName: "mainWindow"
    width: 480
    height: 860
    visible: true
    title: qsTr("Schedule - 课表")

    // ------------------------------------------------------------ 响应式断点
    // 全部来自 Responsive 单例
    // 极端窄屏（小屏手机竖屏）
    readonly property bool narrow: Responsive.isTiny(width)

    // 低高度（手机横屏 / 分屏）
    readonly property bool shortHeight: Responsive.isShort(height)

    // 横屏：宽大于高
    readonly property bool landscape: width > height

    header: ToolBar {
        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                // 低高度时省掉这一行，把垂直空间让给课表本体
                visible: !root.shortHeight

                Label {
                    text: schedule.hasSemester ? schedule.semesterName : qsTr("尚未设置学期")
                    font.bold: true
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                    leftPadding: 8
                }

                Label {
                    text: schedule.conflictSummary
                    color: schedule.hasBlockingConflicts ? Responsive.danger : Responsive.success
                    font.pixelSize: Responsive.fontSmall
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
                    text: root.narrow ? qsTr("周") : qsTr("本周")
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

            // 手机屏幕窄，压缩节次高度让整周尽量可见；横屏 / 低高度时再压一档
            slotHeight: (root.shortHeight || root.landscape) ? 48 : 56
            slotColumnWidth: root.narrow ? 44 : 56
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
            font.pixelSize: Responsive.fontSmall
            // 低高度屏幕优先保证课表可见，状态信息仍可从错误提示能力之外的页面获取
            visible: !root.shortHeight
            text: schedule.lastError.length > 0
                  ? qsTr("⚠ %1").arg(schedule.lastError)
                  : (schedule.lastInfo.length > 0 ? qsTr("✓ %1").arg(schedule.lastInfo) : qsTr("就绪"))
            color: schedule.lastError.length > 0 ? Responsive.danger : Responsive.textSecondary
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

            // 折叠入口：新建 / 导入 / 导出。按钮只负责触发，菜单的展开由 C++ 侧
            // （UiConnector）读取 objectName 后调用 Menu.open() 完成，QML 中不出现任何
            // 信号处理器；菜单项是新增交互控件，需要 C++ 侧连接 triggered()。
            ToolButton {
                id: moreMenuButton

                objectName: "moreMenuButton"
                Layout.fillWidth: true
                text: qsTr("更多")
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
        anchors.bottomMargin: root.shortHeight ? 8 : 20
        // 两侧留白随窗口收缩，且保证宽度不会因为极窄窗口变成负数
        width: Math.max(160, Math.min(parent.width - 32, 460))
        height: notificationBanner.bannerTitle.length > 0 ? 76 : 0
        visible: height > 0
        radius: 10
        color: Responsive.textPrimary
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
                font.pixelSize: Responsive.fontSubheading
                elide: Text.ElideRight
            }

            Text {
                width: parent.width
                text: notificationBanner.bannerMessage
                color: "#C9D6EA"
                font.pixelSize: Responsive.fontBody
                elide: Text.ElideRight
            }
        }
    }

    // 折叠菜单：由 moreMenuButton 触发，C++ 侧（UiConnector）读取 objectName 后调用
    // open() 展开，QML 中不需要信号处理器；菜单项需要 C++ 侧连接 triggered()。
    Menu {
        id: moreMenu

        objectName: "moreMenu"
        title: qsTr("更多")

        MenuItem {
            objectName: "addCourseMenuItem"
            text: qsTr("＋ 新建课程")
        }

        MenuItem {
            objectName: "importMenuItem"
            text: qsTr("导入…")
        }

        MenuItem {
            objectName: "exportMenuItem"
            text: qsTr("导出…")
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
