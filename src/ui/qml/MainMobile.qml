import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// 移动端主界面：页面级标题栏 + 周次条 + 页面栈 + 状态条 + 底部导航 + 底部「更多」弹层。
//
// 交互约定与桌面版一致：QML 不含任何信号处理器，全部交互由 C++ 侧 UiConnector 显式连接。
//
// 导航约定：底部导航 4 个主 Tab（周 / 日 / 课程 / 设置）+「更多」；
// 「更多」是底部弹层，新建 / 导入 / 导出仍复用原来的 objectName，C++ 连接不变。
//
// 响应式约定：
//  - currentTab 跟随 pageStack.currentIndex：周 / 日显示周次条，课程 / 设置只显示页面标题；
//  - shortHeight（横屏 / 分屏）隐藏标题行与状态条，把高度让给内容；
//  - 周视图在移动端允许 7 列压到 Responsive.mobileMinDayWidth，优先整周可见。

ApplicationWindow {
    id: root

    objectName: "mainWindow"
    width: 480
    height: 860
    visible: true
    color: Theme.surface
    title: qsTr("Schedule - 课表")

    // Material 控件跟随项目主题色：默认 accent 是粉/橙，和蓝白设计冲突；
    // primary 同时决定顶部 ToolBar 的底色，与内嵌浏览器页的蓝色顶栏保持一致。
    Material.primary: Theme.accent
    Material.accent: Theme.accent
    Material.theme: Theme.dark ? Material.Dark : Material.Light

    // 主题：schedule.darkMode 解析 system / light / dark，驱动 Theme 的两套色板
    Binding {
        target: Theme
        property: "dark"
        value: schedule.darkMode
    }

    // ------------------------------------------------------------ 响应式断点
    readonly property bool narrow: Responsive.isTiny(width)
    readonly property bool shortHeight: Responsive.isShort(height)
    readonly property bool landscape: width > height

    // 当前页签：0 周 / 1 日 / 2 课程 / 3 设置
    // pageStack 在下方声明，创建前先按 0 处理，创建后自动跟随 currentIndex。
    readonly property int currentTab: pageStack ? pageStack.currentIndex : 0

    // ---------------------------------------------------------------- 头部
    // 用窗口 header 而不是内容区的 ColumnLayout：Android 上窗口 header 会铺到状态栏下方，
    // 状态栏的浅色图标才能落在蓝色背景上；内容区从状态栏下方开始，放这里会出现白条。
    header: Rectangle {
        id: headerBar

        color: Theme.accent
        implicitHeight: SafeArea.margins.top + (root.shortHeight ? 0
            : (root.currentTab <= 1 ? Metrics.appBarHeight + Metrics.weekBarHeight + Metrics.spacingSm
                                        + Metrics.weekSliderHeight + Metrics.spacingMd
                                    : Metrics.appBarHeight))

        ColumnLayout {
            id: headerColumn

            anchors.fill: parent
            spacing: 0

            // 状态栏占位：用布局内的 spacer 而不是 anchors.topMargin，
            // 避免 Android 窗口 header 的绘制与命中区域出现上下偏移。
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: SafeArea.margins.top
            }

            // 页面标题：课表页显示学期名，课程 / 设置显示页面名
            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: Metrics.appBarHeight
                spacing: Metrics.spacingMd
                visible: !root.shortHeight

                Label {
                    Layout.fillWidth: true
                    leftPadding: Metrics.spacingLg
                    verticalAlignment: Text.AlignVCenter
                    text: root.currentTab === 2 ? qsTr("课程")
                          : root.currentTab === 3 ? qsTr("设置")
                          : (schedule.hasSemester ? schedule.semesterName : qsTr("尚未设置学期"))
                    font.pixelSize: Typography.fontHeading
                    font.bold: true
                    color: "#FFFFFF"
                    elide: Text.ElideRight
                }

                Label {
                    // 冲突摘要只属于课表页，设置 / 课程页不显示
                    visible: root.currentTab <= 1
                    rightPadding: Metrics.spacingLg
                    verticalAlignment: Text.AlignVCenter
                    text: schedule.conflictSummary
                    color: schedule.hasBlockingConflicts ? "#FFD7D2" : "#D8F5E5"
                    font.pixelSize: Typography.fontSmall
                }
            }

            // 周次条：仅周 / 日页签
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: Metrics.spacingSm
                Layout.rightMargin: Metrics.spacingSm
                Layout.bottomMargin: Metrics.spacingSm
                Layout.preferredHeight: Metrics.weekBarHeight
                spacing: Metrics.spacingSm
                visible: root.currentTab <= 1 && !root.shortHeight

                ToolButton {
                    id: prevWeekButton

                    objectName: "prevWeekButton"
                    text: "‹"
                    flat: true
                    Layout.preferredWidth: Metrics.toolButtonSize
                    Layout.preferredHeight: Metrics.controlHeight
                    background: Item { }

                    contentItem: Text {
                        text: prevWeekButton.text
                        color: "#FFFFFF"
                        font.pixelSize: Typography.fontTitle
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                ComboBox {
                    id: weekSelector

                    objectName: "weekSelector"
                    Layout.fillWidth: true
                    Layout.preferredHeight: Metrics.controlHeight
                    textRole: "label"
                    valueRole: "value"
                    model: schedule.weekOptions
                    currentIndex: Math.max(0, schedule.selectedWeek - 1)
                    font.pixelSize: Typography.fontBody

                    background: Rectangle {
                        radius: height / 2
                        color: Qt.rgba(1, 1, 1, 0.16)
                        border.width: Metrics.borderWidth
                        border.color: Qt.rgba(1, 1, 1, 0.35)
                    }

                    contentItem: Text {
                        leftPadding: Metrics.spacingLg
                        rightPadding: weekSelector.indicator.width + Metrics.spacingLg
                        text: weekSelector.displayText
                        color: "#FFFFFF"
                        font: weekSelector.font
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }

                    indicator: Text {
                        x: weekSelector.width - width - Metrics.spacingLg
                        y: (weekSelector.height - height) / 2
                        // 用常见三角形，避免 ▾ 在部分 Android 字体里显示成方框
                        text: "▼"
                        color: "#FFFFFF"
                        font.pixelSize: Typography.fontTiny
                    }
                }

                ToolButton {
                    id: nextWeekButton

                    objectName: "nextWeekButton"
                    text: "›"
                    flat: true
                    Layout.preferredWidth: Metrics.toolButtonSize
                    Layout.preferredHeight: Metrics.controlHeight
                    background: Item { }

                    contentItem: Text {
                        text: nextWeekButton.text
                        color: "#FFFFFF"
                        font.pixelSize: Typography.fontTitle
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                ToolButton {
                    id: currentWeekButton

                    objectName: "currentWeekButton"
                    text: root.narrow ? qsTr("周") : qsTr("本周")
                    flat: true
                    Layout.preferredHeight: Metrics.controlHeight
                    background: Item { }

                    contentItem: Text {
                        text: currentWeekButton.text
                        color: "#FFFFFF"
                        font.pixelSize: Typography.fontBodyLarge
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }

            // 周数滑杆：左右拖动快速切换周次，与周条、上一周/下一周保持同步
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: Metrics.spacingXl
                Layout.rightMargin: Metrics.spacingXl
                Layout.bottomMargin: Metrics.spacingMd
                Layout.preferredHeight: Metrics.weekSliderHeight
                visible: root.currentTab <= 1 && !root.shortHeight

                Slider {
                    id: weekSlider

                    objectName: "weekSlider"
                    Layout.fillWidth: true
                    from: 1
                    to: Math.max(1, schedule.totalWeeks)
                    stepSize: 1
                    snapMode: Slider.SnapAlways
                    value: schedule.selectedWeek

                    background: Rectangle {
                        x: weekSlider.leftPadding
                        y: weekSlider.topPadding + weekSlider.availableHeight / 2 - height / 2
                        width: weekSlider.availableWidth
                        height: 2
                        radius: 1
                        color: Qt.rgba(1, 1, 1, 0.35)

                        Rectangle {
                            width: weekSlider.visualPosition * parent.width
                            height: parent.height
                            radius: 1
                            color: "#FFFFFF"
                        }
                    }

                    handle: Rectangle {
                        x: weekSlider.leftPadding + weekSlider.visualPosition * (weekSlider.availableWidth - width)
                        y: weekSlider.topPadding + weekSlider.availableHeight / 2 - height / 2
                        width: 14
                        height: 14
                        radius: 7
                        color: "#FFFFFF"
                    }
                }
            }
        }
    }

    // ---------------------------------------------------------------- 页面栈
    StackLayout {
        id: pageStack

        objectName: "pageStack"
        anchors.fill: parent
        currentIndex: 0

        WeekView {
            id: weekPage

            // 显示设置“格子高度”：紧凑 / 标准 / 宽松；横屏或分屏仍优先紧凑
            slotHeight: (root.shortHeight || root.landscape) ? Responsive.mobileSlotHeightCompact
                        : (schedule.slotDensity === 0 ? Responsive.mobileSlotHeightDense
                           : schedule.slotDensity === 2 ? Responsive.mobileSlotHeightLoose
                           : Responsive.mobileSlotHeight)
            slotColumnWidth: root.narrow ? Responsive.mobileSlotColumnWidthNarrow : Responsive.mobileSlotColumnWidth
            // 手机竖屏优先整周可见：允许 7 列压到 40dp，日期自动隐藏、课卡进入 tight 模式
            minDayWidth: Responsive.mobileMinDayWidth
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

    // ---------------------------------------------------------------- 底部
    footer: ColumnLayout {
        spacing: 0

        // 状态条：空闲时不占高度；有错误 / 提示时才出现
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? Metrics.statusBarHeight : 0
            visible: !root.shortHeight && (schedule.lastError.length > 0 || schedule.lastInfo.length > 0)
            color: Theme.surfaceAlt

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: Metrics.borderWidth
                color: Theme.border
            }

            Label {
                id: statusLabel

                objectName: "statusLabel"
                anchors.fill: parent
                anchors.leftMargin: Metrics.spacingLg
                anchors.rightMargin: Metrics.spacingLg
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
                font.pixelSize: Typography.fontSmall
                text: schedule.lastError.length > 0 ? qsTr("⚠ %1").arg(schedule.lastError) : qsTr("✓ %1").arg(schedule.lastInfo)
                color: schedule.lastError.length > 0 ? Theme.danger : Theme.textSecondary
            }
        }

        // 底部导航：4 个主 Tab +「更多」
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: Metrics.navBarHeight
            color: Theme.surface

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: Metrics.borderWidth
                color: Theme.border
            }

            RowLayout {
                anchors.fill: parent
                spacing: 0

                NavButton {
                    objectName: "navWeekButton"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    label: qsTr("周")
                    selected: root.currentTab === 0
                }

                NavButton {
                    objectName: "navDayButton"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    label: qsTr("日")
                    selected: root.currentTab === 1
                }

                NavButton {
                    objectName: "navSemesterButton"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    label: qsTr("课程")
                    selected: root.currentTab === 2
                }

                NavButton {
                    objectName: "navSettingsButton"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    label: qsTr("设置")
                    selected: root.currentTab === 3
                }

                NavButton {
                    objectName: "moreMenuButton"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    label: qsTr("更多")
                    selected: false
                }
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
        anchors.bottomMargin: root.shortHeight ? Metrics.bannerBottomMarginShort : Metrics.bannerBottomMargin
        width: Responsive.bannerWidth(parent.width)
        height: notificationBanner.bannerTitle.length > 0 ? Metrics.bannerHeight : 0
        visible: height > 0
        radius: Metrics.radiusLg
        color: Theme.bannerBg
        opacity: Metrics.bannerOpacity

        Column {
            anchors.fill: parent
            anchors.margins: Metrics.spacing2xl
            spacing: Metrics.spacingSm

            Text {
                width: parent.width
                text: notificationBanner.bannerTitle
                color: Theme.bannerTitleText
                font.bold: true
                font.pixelSize: Typography.fontSubheading
                elide: Text.ElideRight
            }

            Text {
                width: parent.width
                text: notificationBanner.bannerMessage
                color: Theme.bannerSubText
                font.pixelSize: Typography.fontBody
                elide: Text.ElideRight
            }
        }
    }

    // 「更多」底部弹层：C++ 侧（UiConnector）读取 objectName 后调用 open()，
    // 菜单项仍用 MenuItem 的 triggered()，原有连接不变。
    Popup {
        id: moreMenu

        objectName: "moreMenu"
        parent: Overlay.overlay
        modal: true
        dim: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        width: parent ? parent.width : 480
        height: contentColumn.implicitHeight + Metrics.spacing2xl
        x: 0
        y: parent ? parent.height - height : 0
        padding: 0

        background: Rectangle {
            color: Theme.surface
            radius: Metrics.radiusLg
            border.width: Metrics.borderWidth
            border.color: Theme.border
        }

        contentItem: ColumnLayout {
            id: contentColumn
            spacing: 0

            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: Metrics.spacingMd
                width: 36
                height: 4
                radius: 2
                color: Theme.divider
            }

            Label {
                Layout.fillWidth: true
                Layout.leftMargin: Metrics.spacing2xl
                Layout.rightMargin: Metrics.spacing2xl
                Layout.topMargin: Metrics.spacingMd
                Layout.bottomMargin: Metrics.spacingSm
                text: qsTr("更多")
                font.pixelSize: Typography.fontSubheading
                font.bold: true
                color: Theme.textPrimary
            }

            MenuItem {
                objectName: "addCourseMenuItem"
                Layout.fillWidth: true
                implicitHeight: 52
                text: qsTr("＋ 新建课程")
            }

            MenuItem {
                objectName: "importMenuItem"
                Layout.fillWidth: true
                implicitHeight: 52
                text: qsTr("导入…")
            }

            MenuItem {
                objectName: "exportMenuItem"
                Layout.fillWidth: true
                implicitHeight: 52
                text: qsTr("导出…")
            }
        }
    }

    CourseEditor {
        id: courseEditor
    }

    CourseDetailDialog {
        id: courseDetailDialog
        sheetMode: true
    }

    ImportWizard {
        id: importWizard
    }

    BrowserImportDialog {
        id: browserImportDialog
    }

    ExportDialog {
        id: exportDialog
    }
}
