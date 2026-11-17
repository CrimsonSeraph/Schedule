import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

// 设置页：导入 / 导出目录、作息时间（节次）设置、数据库维护与关于信息。
//
// 交互约定：按钮不带 onClicked，文件 / 目录选择对话框也不带 onAccepted；
// C++ 侧（app 层 UiConnector）显式连接并读写下列具名控件。
//
// 响应式策略：
//  - 各表单 GridLayout 的列数由页面宽度决定（宽屏 3/2 列、窄屏单列），
//    输入框一律 Layout.fillWidth，窄屏下改为“标签在上、输入框在下”；
//  - 按钮行改用 Flow，宽度不足时自动换行，不再依赖固定像素宽度。
Item {
    id: settingsPage

    objectName: "settingsPage"

    // 宽表单：目录 / 作息表用 3 列，其余用 2 列；窄屏统一降为单列
    readonly property bool wideForm: settingsPage.width >= Responsive.wideFormWidth
    readonly property int groupColumns: Responsive.settingsColumns(settingsPage.width)
    readonly property int pairColumns: Responsive.settingsPairColumns(settingsPage.width)

    // 自检按钮只在 --selftest 下显示：正常运行不暴露调试入口，自动化自检仍能点到它。
    readonly property bool selftestMode: Qt.application.arguments.indexOf("--selftest") >= 0

    ScrollView {
        id: settingsScroll

        anchors.fill: parent
        clip: true

        // 关闭横向滚动条并让内容宽度直接跟随 ScrollView 宽度，
        // 避免 contentWidth 与 availableWidth 互相依赖造成绑定循环
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            width: settingsScroll.width
            spacing: Metrics.spacing3xl

            // 自检按钮只在 --selftest 模式出现，正常界面不显示调试入口；
            // 仍放在页面最顶部，保证自检能在真实窗口里点到它。
            RowLayout {
                Layout.fillWidth: true
                Layout.margins: Metrics.spacing2xl
                spacing: Metrics.spacingLg
                visible: settingsPage.selftestMode

                AppButton {
                    id: testButton

                    objectName: "testButton"
                    text: qsTr("自检（输出测试信息）")
                }
            }

            // ------------------------------------------------------------ 显示设置
            GroupBox {
                Layout.fillWidth: true
                Layout.margins: Metrics.spacing2xl
                title: qsTr("显示")

                GridLayout {
                    anchors.fill: parent
                    columns: settingsPage.pairColumns
                    columnSpacing: Metrics.spacingLg
                    rowSpacing: Metrics.spacingMd

                    CheckBox {
                        id: showWeekendCheck

                        objectName: "showWeekendCheck"
                        Layout.columnSpan: settingsPage.pairColumns
                        text: qsTr("显示周六、周日")
                        checked: schedule.showWeekend
                    }

                    CheckBox {
                        id: showOtherWeeksCheck

                        objectName: "showOtherWeeksCheck"
                        Layout.columnSpan: settingsPage.pairColumns
                        text: qsTr("显示非本周课程")
                        checked: schedule.showOtherWeeks
                    }

                    Label {
                        text: qsTr("主题")
                    }
                    ComboBox {
                        id: themeSelector

                        objectName: "themeSelector"
                        Layout.fillWidth: true
                        model: [qsTr("跟随系统"), qsTr("浅色"), qsTr("深色")]
                        currentIndex: schedule.theme === "light" ? 1 : schedule.theme === "dark" ? 2 : 0
                    }

                    Label {
                        text: qsTr("格子高度")
                    }
                    ComboBox {
                        id: slotDensitySelector

                        objectName: "slotDensitySelector"
                        Layout.fillWidth: true
                        model: [qsTr("紧凑"), qsTr("标准"), qsTr("宽松")]
                        currentIndex: schedule.slotDensity
                    }

                    Label {
                        text: qsTr("卡片圆角")
                    }
                    ComboBox {
                        id: cardRadiusSelector

                        objectName: "cardRadiusSelector"
                        Layout.fillWidth: true
                        model: [qsTr("小"), qsTr("中"), qsTr("大")]
                        currentIndex: schedule.cardRadius
                    }
                }
            }

            // ------------------------------------------------------------ 目录设置
            GroupBox {
                Layout.fillWidth: true
                Layout.margins: Metrics.spacing2xl
                title: qsTr("导入 / 导出目录")

                GridLayout {
                    anchors.fill: parent
                    columns: settingsPage.groupColumns
                    columnSpacing: Metrics.spacingLg
                    rowSpacing: Metrics.spacingLg

                    Label {
                        text: qsTr("默认导入目录")
                    }

                    Label {
                        id: importDirField

                        objectName: "importDirField"
                        Layout.fillWidth: true
                        text: schedule.importExport.defaultImportDir
                        elide: Text.ElideMiddle
                        color: Theme.textPrimary
                        font.pixelSize: Typography.fontBody
                    }

                    AppButton {
                        id: chooseImportDirButton

                        objectName: "chooseImportDirButton"
                        text: qsTr("选择")
                        flat: true
                        implicitHeight: Metrics.controlHeight
                    }

                    Label {
                        text: qsTr("默认导出目录")
                    }

                    Label {
                        id: exportDirField

                        objectName: "exportDirField"
                        Layout.fillWidth: true
                        text: schedule.importExport.defaultExportDir
                        elide: Text.ElideMiddle
                        color: Theme.textPrimary
                        font.pixelSize: Typography.fontBody
                    }

                    AppButton {
                        id: chooseExportDirButton

                        objectName: "chooseExportDirButton"
                        text: qsTr("选择")
                        flat: true
                        implicitHeight: Metrics.controlHeight
                    }

                    Item {
                        Layout.fillWidth: true
                        // 单列排版时不需要占位撑开按钮行
                        visible: settingsPage.wideForm
                    }

                    Flow {
                        Layout.fillWidth: true
                        Layout.columnSpan: settingsPage.wideForm ? 2 : 1
                        spacing: Metrics.spacingLg

                        AppButton {
                            id: saveDirsButton

                            objectName: "saveDirsButton"


                            primary: true
                            text: qsTr("保存目录")
                            implicitHeight: Metrics.controlHeight
                        }

                        AppButton {
                            id: resetDirsButton

                            objectName: "resetDirsButton"
                            text: qsTr("恢复默认目录")
                            implicitHeight: Metrics.controlHeight
                        }
                    }
                }
            }

            // ------------------------------------------------------------ 作息时间
            GroupBox {
                Layout.fillWidth: true
                Layout.margins: Metrics.spacing2xl
                title: qsTr("作息时间（节次）")

                GridLayout {
                    anchors.fill: parent
                    columns: settingsPage.groupColumns
                    columnSpacing: Metrics.spacingLg
                    rowSpacing: Metrics.spacingLg

                    Label {
                        text: qsTr("选择节次")
                    }

                    ComboBox {
                        id: slotSelector

                        objectName: "slotSelector"
                        Layout.fillWidth: true
                        textRole: "label"
                        valueRole: "index"
                        model: schedule.timeSlots
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    Label {
                        text: qsTr("名称")
                    }
                    TextField {
                        id: slotLabelField

                        objectName: "slotLabelField"
                        // 宽表单里名称字段跨到按钮列；单列排版时占满整行
                        Layout.columnSpan: settingsPage.wideForm ? 2 : 1
                        Layout.fillWidth: true
                        placeholderText: qsTr("第 1 节")
                    }

                    Label {
                        text: qsTr("开始时间")
                    }
                    TextField {
                        id: slotStartField

                        objectName: "slotStartField"
                        Layout.fillWidth: true
                        placeholderText: "08:00"
                    }
                    TextField {
                        id: slotEndField

                        objectName: "slotEndField"
                        Layout.fillWidth: true
                        placeholderText: "08:45"
                    }

                    Item {
                        Layout.fillWidth: true
                        visible: settingsPage.wideForm
                    }

                    Flow {
                        Layout.fillWidth: true
                        Layout.columnSpan: settingsPage.wideForm ? 2 : 1
                        spacing: Metrics.spacingLg

                        AppButton {
                            id: saveSlotButton

                            objectName: "saveSlotButton"


                            primary: true
                            text: qsTr("保存该节次")
                        }

                        AppButton {
                            id: resetSlotsButton

                            objectName: "resetSlotsButton"
                            text: qsTr("恢复默认作息")
                        }
                    }
                }
            }

            // ------------------------------------------------------------ 课程提醒
            GroupBox {
                Layout.fillWidth: true
                Layout.margins: Metrics.spacing2xl
                title: qsTr("课程提醒")

                ColumnLayout {
                    anchors.fill: parent
                    spacing: Metrics.spacingLg

                    GridLayout {
                        Layout.fillWidth: true
                        columns: settingsPage.pairColumns
                        columnSpacing: Metrics.spacingLg
                        rowSpacing: Metrics.spacingLg

                        Label {
                            text: qsTr("启用提醒")
                        }
                        CheckBox {
                            id: reminderEnabledCheck

                            objectName: "reminderEnabledCheck"
                            text: qsTr("上课前通过系统通知提醒")
                            checked: reminders.enabled
                        }

                        Label {
                            text: qsTr("提前时间")
                        }
                        ComboBox {
                            id: reminderMinutesSelector

                            objectName: "reminderMinutesSelector"
                            Layout.fillWidth: true
                            textRole: "label"
                            valueRole: "value"
                            model: reminders.minutesOptions
                            currentIndex: reminders.minutesIndex
                        }

                        Label {
                            text: qsTr("通知方式")
                        }
                        Label {
                            Layout.fillWidth: true
                            text: reminders.backendName + " · " + reminders.backendStatus
                            color: Theme.textSecondary
                            wrapMode: Text.WordWrap
                        }

                        Item {
                            Layout.fillWidth: true
                            visible: settingsPage.wideForm
                        }

                        Flow {
                            Layout.fillWidth: true
                            Layout.columnSpan: settingsPage.wideForm ? 2 : 1
                            spacing: Metrics.spacingLg

                            AppButton {
                                id: testNotificationButton

                                objectName: "testNotificationButton"
                                text: qsTr("发送测试通知")
                            }

                            AppButton {
                                id: requestPermissionButton

                                objectName: "requestPermissionButton"
                                text: qsTr("申请通知权限")
                            }
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("下一次：") + reminders.nextReminderText
                        color: Theme.accentStrong
                        wrapMode: Text.WordWrap
                    }

                    Label {
                        Layout.fillWidth: true
                        visible: reminders.todayReminders.length > 0
                        text: qsTr("今日课程")
                        font.bold: true
                        color: Theme.textStrong
                    }

                    Repeater {
                        model: reminders.todayReminders

                        delegate: Label {
                            Layout.fillWidth: true
                            text: "• " + modelData.start + " " + modelData.courseName + " · " + modelData.message
                            color: Theme.textSecondary
                            wrapMode: Text.WordWrap
                            font.pixelSize: Typography.fontBody
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        text: reminders.lastNotificationText
                        color: Theme.textMuted
                        wrapMode: Text.WordWrap
                        font.pixelSize: Typography.fontSmall
                    }
                }
            }

            // ------------------------------------------------------ 教务适配器（可选）
            GroupBox {
                Layout.fillWidth: true
                Layout.margins: Metrics.spacing2xl
                title: qsTr("从教务导入")

                ColumnLayout {
                    anchors.fill: parent
                    spacing: Metrics.spacingLg

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("隐私说明：只在你点击“导入课表”时抓取一次当前页面的内容，不保存密码、不做后台同步；不读取也不保存 Cookie。")
                        color: Theme.warning
                        wrapMode: Text.WordWrap
                        font.pixelSize: Typography.fontSmall
                    }

                    // 与导入向导里的入口列表完全一致（含固定的“打开内置浏览器”）
                    GridLayout {
                        Layout.fillWidth: true
                        columns: settingsPage.pairColumns
                        columnSpacing: Metrics.spacingLg
                        rowSpacing: Metrics.spacingLg

                        Label {
                            text: qsTr("教务入口")
                        }
                        ComboBox {
                            id: settingsAdapterSelector

                            objectName: "settingsAdapterSelector"
                            Layout.fillWidth: true
                            textRole: "name"
                            model: schedule.importExport.browserEntries
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        text: {
                            const entries = schedule.importExport.browserEntries;
                            if (settingsAdapterSelector.currentIndex < 0 || settingsAdapterSelector.currentIndex >= entries.length) {
                                return "";
                            }
                            return entries[settingsAdapterSelector.currentIndex].description;
                        }
                        wrapMode: Text.WordWrap
                        color: Theme.textMuted
                        font.pixelSize: Typography.fontSmall
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("已适配的课表类型：%1").arg(schedule.importExport.adaptedTimetableTypes.join(qsTr("；")))
                        wrapMode: Text.WordWrap
                        color: Theme.textMuted
                        font.pixelSize: Typography.fontSmall
                    }

                    Flow {
                        Layout.fillWidth: true
                        spacing: Metrics.spacingLg

                        AppButton {
                            id: settingsAdapterImportButton

                            objectName: "settingsAdapterImportButton"


                            primary: true
                            text: qsTr("从教务导入…")
                        }

                        AppButton {
                            id: settingsAdapterSystemOpenButton

                            objectName: "settingsAdapterSystemOpenButton"
                            text: qsTr("用系统浏览器打开")
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        visible: schedule.importExport.webCaptureSummary.length > 0
                        text: schedule.importExport.webCaptureSummary
                        wrapMode: Text.WordWrap
                        color: schedule.importExport.hasPendingPreview ? Theme.success : Theme.danger
                        font.pixelSize: Typography.fontSmall
                    }

                    // ------------------------------------------------ 高级选项（默认隐藏）
                    // 接口地址与 Cookie 只服务于“课表接口直接返回 JSON / CSV / ICS”的少数场景，
                    // 对绝大多数用户是噪音，因此默认折叠，勾选后才展开。
                    CheckBox {
                        id: adapterAdvancedCheck

                        objectName: "adapterAdvancedCheck"
                        text: qsTr("显示高级选项（接口地址与登录凭证）")
                    }

                    ColumnLayout {
                        id: adapterAdvancedPanel

                        objectName: "adapterAdvancedPanel"
                        Layout.fillWidth: true
                        visible: adapterAdvancedCheck.checked
                        spacing: Metrics.spacingLg

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("仅当教务系统的课表接口能直接返回 JSON / CSV / ICS 时才需要填写；地址会写入本机设置，Cookie 只驻留内存。")
                            wrapMode: Text.WordWrap
                            color: Theme.textMuted
                            font.pixelSize: Typography.fontSmall
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: settingsPage.pairColumns
                            columnSpacing: Metrics.spacingLg
                            rowSpacing: Metrics.spacingLg

                            Label {
                                text: qsTr("适配器")
                            }
                            ComboBox {
                                id: adapterSelector

                                objectName: "adapterSelector"
                                Layout.fillWidth: true
                                textRole: "name"
                                model: schedule.importExport.adapterOptions
                            }

                            Label {
                                text: qsTr("课表接口地址")
                            }
                            TextField {
                                id: adapterScheduleUrlField

                                objectName: "adapterScheduleUrlField"
                                Layout.fillWidth: true
                                placeholderText: qsTr("http(s) 接口地址或本地文件路径")
                            }

                            Label {
                                text: qsTr("登录页地址")
                            }
                            TextField {
                                id: adapterLoginUrlField

                                objectName: "adapterLoginUrlField"
                                Layout.fillWidth: true
                                placeholderText: qsTr("供内嵌浏览器打开；可留空")
                            }

                            Label {
                                text: qsTr("登录 Cookie")
                            }
                            TextField {
                                id: adapterCookieField

                                objectName: "adapterCookieField"
                                Layout.fillWidth: true
                                placeholderText: qsTr("从浏览器开发者工具复制 Cookie 请求头")
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            text: schedule.importExport.adapterSessionStatus
                            color: Theme.textSecondary
                            wrapMode: Text.WordWrap
                            font.pixelSize: Typography.fontSmall
                        }

                        Flow {
                            Layout.fillWidth: true
                            spacing: Metrics.spacingLg

                            AppButton {
                                id: adapterSaveUrlButton

                                objectName: "adapterSaveUrlButton"


                                primary: true
                                text: qsTr("保存接口地址")
                            }

                            AppButton {
                                id: adapterImportButton

                                objectName: "adapterImportButton"
                                text: qsTr("从适配器导入")
                            }

                            AppButton {
                                id: adapterClearSessionButton

                                objectName: "adapterClearSessionButton"


                                danger: true
                                text: qsTr("清除凭证")
                            }
                        }
                    }
                }
            }

            // ------------------------------------------------------------ 数据维护
            GroupBox {
                Layout.fillWidth: true
                Layout.margins: Metrics.spacing2xl
                title: qsTr("数据")

                GridLayout {
                    anchors.fill: parent
                    columns: settingsPage.pairColumns
                    columnSpacing: Metrics.spacingLg
                    rowSpacing: Metrics.spacingLg

                    Label {
                        text: qsTr("数据文件")
                    }
                    Label {
                        Layout.fillWidth: true
                        text: schedule.databasePath
                        color: Theme.textSecondary
                        elide: Text.ElideMiddle
                        wrapMode: Text.NoWrap
                    }

                    Label {
                        text: qsTr("课程数量")
                    }
                    Label {
                        text: String(schedule.courseCount)
                    }

                    Item {
                        Layout.fillWidth: true
                        visible: settingsPage.wideForm
                    }

                    Flow {
                        Layout.fillWidth: true
                        Layout.columnSpan: settingsPage.wideForm ? 2 : 1
                        spacing: Metrics.spacingLg

                        AppButton {
                            id: reloadButton

                            objectName: "reloadButton"
                            text: qsTr("从数据库重新加载")
                        }

                        AppButton {
                            id: saveNowButton

                            objectName: "saveNowButton"


                            primary: true
                            text: qsTr("立即保存")
                        }
                    }
                }
            }

            // ------------------------------------------------------------ 诊断
            GroupBox {
                Layout.fillWidth: true
                Layout.margins: Metrics.spacing2xl
                title: qsTr("诊断")

                ColumnLayout {
                    anchors.fill: parent
                    spacing: Metrics.spacingLg

                    GridLayout {
                        Layout.fillWidth: true
                        columns: settingsPage.pairColumns
                        columnSpacing: Metrics.spacingLg
                        rowSpacing: Metrics.spacingLg

                        Label {
                            text: qsTr("日志目录")
                        }
                        Label {
                            objectName: "logDirField"
                            Layout.fillWidth: true
                            color: Theme.textSecondary
                            elide: Text.ElideMiddle
                        }

                        Label {
                            text: qsTr("日志级别")
                        }
                        ComboBox {
                            id: logLevelSelector

                            objectName: "logLevelSelector"
                            Layout.fillWidth: true
                            model: [qsTr("调试"), qsTr("信息"), qsTr("警告"), qsTr("错误")]
                            currentIndex: schedule.logLevel
                        }

                        Label {
                            text: qsTr("保留天数")
                        }
                        ComboBox {
                            id: logRetentionSelector

                            objectName: "logRetentionSelector"
                            Layout.fillWidth: true
                            model: [qsTr("1 天"), qsTr("3 天"), qsTr("7 天"), qsTr("30 天")]
                            currentIndex: Math.max(0, [1, 3, 7, 30].indexOf(schedule.logRetentionDays))
                        }
                    }

                    Flow {
                        Layout.fillWidth: true
                        spacing: Metrics.spacingLg

                        AppButton {
                            objectName: "exportLogsButton"
                            text: qsTr("导出日志")
                            primary: true
                        }

                        AppButton {
                            objectName: "shareLogsButton"
                            text: qsTr("分享日志")
                        }
                    }
                }
            }

            Label {
                Layout.margins: Metrics.spacing2xl
                Layout.fillWidth: true
                text: qsTr("Schedule v%1 · 本地课表应用（不做云同步 / 账号系统）\n数据仅保存在本机，导入导出可指定任意目录。").arg(schedule.version)
                color: Theme.textMuted
                wrapMode: Text.WordWrap
            }

            Item {
                Layout.fillHeight: true
            }
        }
    }

    // 目录选择对话框：仅负责把用户选中的目录写回上方文本框，实际保存在 C++ 侧完成
    FolderDialog {
        id: importDirDialog

        objectName: "importDirDialog"
        title: qsTr("选择默认导入目录")
    }

    FolderDialog {
        id: exportDirDialog

        objectName: "exportDirDialog"
        title: qsTr("选择默认导出目录")
    }
}
