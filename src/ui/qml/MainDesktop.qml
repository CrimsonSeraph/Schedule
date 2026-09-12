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
//
// 响应式约定：
//  - 最小窗口取 Responsive.minWindowWidth / minWindowHeight：小屏、分屏、低高度窗口
//    不再被 900×600 的硬下限挡住；
//  - 断点只依赖窗口宽高（不依赖平台宏），且全部来自 Responsive 单例，
//    横竖屏切换与手动缩放走同一条代码路径：
//      * compactToolbar（宽度 < Responsive.compactToolbarWidth）：次要操作
//        （新建 / 导入 / 导出）折叠进“更多”折叠菜单（moreMenuButton + moreMenu）；
//      * narrow（宽度 < Responsive.narrowWidth）：隐藏标题与学期回显，把宽度让给导航；
//      * shortHeight（高度 < Responsive.shortHeight）：压缩底部状态栏与提醒横幅占位。
//  - 折叠菜单项（addCourseMenuItem / importMenuItem / exportMenuItem）与触发按钮
//    moreMenuButton 是**新增控件**，需要 C++ 侧（app 层 UiConnector）显式连接
//    `clicked()`（打开菜单）与 `triggered()`（菜单动作）；
//    宽屏内联的 addCourseButton / importButton / exportButton 及其既有连接保持不变。
ApplicationWindow {
    id: root

    objectName: "mainWindow"
    width: 1180
    height: 760
    minimumWidth: Responsive.minWindowWidth
    minimumHeight: Responsive.minWindowHeight
    visible: true
    title: qsTr("Schedule - 课表")

    // ------------------------------------------------------------ 响应式断点
    // 全部来自 Responsive 单例：完整工具栏需要约 1400px 才不裁切，
    // 因此 compactToolbarWidth 取 1440，默认窗口 1180 落在折叠形态。
    readonly property bool compactToolbar: Responsive.isCompactToolbar(width)

    // 超窄窗口：隐藏非必要回显，只保留导航骨架
    readonly property bool narrow: Responsive.isNarrow(width)

    // 低高度窗口（横屏 / 分屏）：压缩纵向占位
    readonly property bool shortHeight: Responsive.isShort(height)

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            spacing: root.compactToolbar ? 2 : 8

            Label {
                text: qsTr("课表")
                font.bold: true
                font.pixelSize: Responsive.fontTitle
                leftPadding: 8
                // 超窄窗口下标题让位给导航按钮
                visible: !root.narrow
            }

            Label {
                text: schedule.hasSemester ? qsTr("%1 · 共 %2 周").arg(schedule.semesterName).arg(schedule.totalWeeks) : qsTr("尚未设置学期")
                color: Responsive.textSecondary
                elide: Text.ElideRight
                Layout.maximumWidth: 260
                // 窄窗口下先折叠学期回显：完整信息在“学期与课程”页仍可见
                visible: !root.compactToolbar
            }

            ToolSeparator {
                visible: !root.compactToolbar
            }

            ToolButton {
                id: prevWeekButton

                objectName: "prevWeekButton"
                text: root.compactToolbar ? qsTr("◀") : qsTr("◀ 上一周")
            }

            ComboBox {
                id: weekSelector

                objectName: "weekSelector"
                Layout.preferredWidth: root.compactToolbar ? 108 : 150
                textRole: "label"
                valueRole: "value"
                model: schedule.weekOptions
            }

            ToolButton {
                id: nextWeekButton

                objectName: "nextWeekButton"
                text: root.compactToolbar ? qsTr("▶") : qsTr("下一周 ▶")
            }

            ToolButton {
                id: currentWeekButton

                objectName: "currentWeekButton"
                text: root.compactToolbar ? qsTr("本周") : (schedule.currentWeek > 0 ? qsTr("回到第 %1 周").arg(schedule.currentWeek) : qsTr("回到本周"))
            }

            ToolSeparator {
                visible: !root.compactToolbar
            }

            ToolButton {
                id: navWeekButton

                objectName: "navWeekButton"
                text: root.compactToolbar ? qsTr("周") : qsTr("周视图")
                checkable: true
                checked: pageStack.currentIndex === 0
            }

            ToolButton {
                id: navDayButton

                objectName: "navDayButton"
                text: root.compactToolbar ? qsTr("日") : qsTr("日视图")
                checkable: true
                checked: pageStack.currentIndex === 1
            }

            ToolButton {
                id: navSemesterButton

                objectName: "navSemesterButton"
                text: root.compactToolbar ? qsTr("课程") : qsTr("学期与课程")
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

            Item {
                Layout.fillWidth: true
            }

            Label {
                id: conflictBadge

                objectName: "conflictBadge"
                text: schedule.conflictSummary
                color: schedule.hasBlockingConflicts ? Responsive.danger : Responsive.success
                font.bold: schedule.hasBlockingConflicts
                // 冲突明细在“学期与课程”页与底部状态栏仍有展示，超窄时先隐藏
                visible: !root.narrow
            }

            ToolSeparator {
                visible: !root.compactToolbar
            }

            // 宽屏：次要操作直接内联（objectName 与既有 C++ 连接保持不变）
            ToolButton {
                id: addCourseButton

                objectName: "addCourseButton"
                text: qsTr("＋ 新建课程")
                visible: !root.compactToolbar
            }

            ToolButton {
                id: importButton

                objectName: "importButton"
                text: qsTr("导入…")
                visible: !root.compactToolbar
            }

            ToolButton {
                id: exportButton

                objectName: "exportButton"
                text: qsTr("导出…")
                visible: !root.compactToolbar
            }

            // 窄屏折叠入口：新建 / 导入 / 导出。
            // 该按钮只负责“触发”，菜单的展开由 C++ 侧（UiConnector）读取 objectName 后调用
            // Menu.open() 完成，QML 中不出现任何信号处理器（与 chooseImportDirButton →
            // importDirDialog 的既有写法一致）。菜单项同样是新增交互控件，需要 C++ 侧连接
            // triggered()，清单见文件头注释。
            ToolButton {
                id: moreMenuButton

                objectName: "moreMenuButton"
                text: qsTr("更多 ▾")
                visible: root.compactToolbar
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
            // 低高度窗口压缩节次高度，尽量让整周可见
            slotHeight: root.shortHeight ? 52 : 64
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
            spacing: Responsive.spacing

            Label {
                id: statusLabel

                objectName: "statusLabel"
                Layout.fillWidth: true
                leftPadding: 8
                elide: Text.ElideRight
                text: schedule.lastError.length > 0 ? qsTr("⚠ %1").arg(schedule.lastError) : (schedule.lastInfo.length > 0 ? qsTr("✓ %1").arg(schedule.lastInfo) : qsTr("就绪"))
                color: schedule.lastError.length > 0 ? Responsive.danger : Responsive.textSecondary
            }

            Label {
                text: qsTr("第 %1 周 · %2").arg(schedule.selectedWeek).arg(schedule.selectedWeekRange)
                color: Responsive.textMuted
                // 超窄窗口优先保证状态行不折行
                visible: !root.narrow
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

    // 折叠菜单：窄屏下由 moreMenuButton 触发，C++ 侧（UiConnector）读取 objectName 后
    // 调用 open() 展开，QML 中不需要信号处理器。菜单项需要 C++ 侧连接 triggered()。
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

    // 各页共用的对话框（尺寸策略保持 Math.min 限制，不超出父窗口）
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
