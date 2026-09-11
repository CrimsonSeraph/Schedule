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

    ScrollView {
        id: settingsScroll

        anchors.fill: parent
        clip: true

        // 关闭横向滚动条并让内容宽度直接跟随 ScrollView 宽度，
        // 避免 contentWidth 与 availableWidth 互相依赖造成绑定循环
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            width: settingsScroll.width
            spacing: Responsive.sectionSpacing

            // 标题行同时承载“自检”按钮：放在页面最顶部，保证任何窗口高度下都可见，
            // 便于 --selftest 通过真实鼠标点击验证 C++ 侧的连接链路。
            RowLayout {
                Layout.fillWidth: true
                Layout.margins: Responsive.margin
                spacing: Responsive.spacing

                Label {
                    Layout.fillWidth: true
                    text: qsTr("设置")
                    font.bold: true
                    font.pixelSize: Responsive.fontTitle
                    color: Responsive.textPrimary
                    // 窄屏时省略标题而不是把“自检”按钮挤出视口
                    elide: Text.ElideRight
                }

                Button {
                    id: testButton

                    objectName: "testButton"
                    text: qsTr("自检（输出测试信息）")
                }
            }

            // ------------------------------------------------------------ 目录设置
            GroupBox {
                Layout.fillWidth: true
                Layout.margins: Responsive.margin
                title: qsTr("导入 / 导出目录")

                GridLayout {
                    anchors.fill: parent
                    columns: settingsPage.groupColumns
                    columnSpacing: 8
                    rowSpacing: 8

                    Label { text: qsTr("默认导入目录") }

                    TextField {
                        id: importDirField

                        objectName: "importDirField"
                        Layout.fillWidth: true
                        placeholderText: schedule.importExport.defaultImportDir
                        text: schedule.importExport.defaultImportDir
                    }

                    Button {
                        id: chooseImportDirButton

                        objectName: "chooseImportDirButton"
                        text: qsTr("选择…")
                    }

                    Label { text: qsTr("默认导出目录") }

                    TextField {
                        id: exportDirField

                        objectName: "exportDirField"
                        Layout.fillWidth: true
                        placeholderText: schedule.importExport.defaultExportDir
                        text: schedule.importExport.defaultExportDir
                    }

                    Button {
                        id: chooseExportDirButton

                        objectName: "chooseExportDirButton"
                        text: qsTr("选择…")
                    }

                    Item {
                        Layout.fillWidth: true
                        // 单列排版时不需要占位撑开按钮行
                        visible: settingsPage.wideForm
                    }

                    Flow {
                        Layout.fillWidth: true
                        Layout.columnSpan: settingsPage.wideForm ? 2 : 1
                        spacing: Responsive.spacing

                        Button {
                            id: saveDirsButton

                            objectName: "saveDirsButton"
                            text: qsTr("保存目录")
                        }

                        Button {
                            id: resetDirsButton

                            objectName: "resetDirsButton"
                            text: qsTr("恢复默认目录")
                        }
                    }
                }
            }

            // ------------------------------------------------------------ 作息时间
            GroupBox {
                Layout.fillWidth: true
                Layout.margins: Responsive.margin
                title: qsTr("作息时间（节次）")

                GridLayout {
                    anchors.fill: parent
                    columns: settingsPage.groupColumns
                    columnSpacing: 8
                    rowSpacing: 8

                    Label { text: qsTr("选择节次") }

                    ComboBox {
                        id: slotSelector

                        objectName: "slotSelector"
                        Layout.fillWidth: true
                        textRole: "label"
                        valueRole: "index"
                        model: schedule.timeSlots
                    }

                    Item { Layout.fillWidth: true }

                    Label { text: qsTr("名称") }
                    TextField {
                        id: slotLabelField

                        objectName: "slotLabelField"
                        // 宽表单里名称字段跨到按钮列；单列排版时占满整行
                        Layout.columnSpan: settingsPage.wideForm ? 2 : 1
                        Layout.fillWidth: true
                        placeholderText: qsTr("第 1 节")
                    }

                    Label { text: qsTr("开始时间") }
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
                        spacing: Responsive.spacing

                        Button {
                            id: saveSlotButton

                            objectName: "saveSlotButton"
                            text: qsTr("保存该节次")
                        }

                        Button {
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
                Layout.margins: Responsive.margin
                title: qsTr("课程提醒")

                ColumnLayout {
                    anchors.fill: parent
                    spacing: Responsive.spacing

                    GridLayout {
                        Layout.fillWidth: true
                        columns: settingsPage.pairColumns
                        columnSpacing: 8
                        rowSpacing: 8

                        Label { text: qsTr("启用提醒") }
                        CheckBox {
                            id: reminderEnabledCheck

                            objectName: "reminderEnabledCheck"
                            text: qsTr("上课前通过系统通知提醒")
                            checked: reminders.enabled
                        }

                        Label { text: qsTr("提前时间") }
                        ComboBox {
                            id: reminderMinutesSelector

                            objectName: "reminderMinutesSelector"
                            Layout.fillWidth: true
                            textRole: "label"
                            valueRole: "value"
                            model: reminders.minutesOptions
                            currentIndex: reminders.minutesIndex
                        }

                        Label { text: qsTr("通知方式") }
                        Label {
                            Layout.fillWidth: true
                            text: reminders.backendName + " · " + reminders.backendStatus
                            color: Responsive.textSecondary
                            wrapMode: Text.WordWrap
                        }

                        Item {
                            Layout.fillWidth: true
                            visible: settingsPage.wideForm
                        }

                        Flow {
                            Layout.fillWidth: true
                            Layout.columnSpan: settingsPage.wideForm ? 2 : 1
                            spacing: Responsive.spacing

                            Button {
                                id: testNotificationButton

                                objectName: "testNotificationButton"
                                text: qsTr("发送测试通知")
                            }

                            Button {
                                id: requestPermissionButton

                                objectName: "requestPermissionButton"
                                text: qsTr("申请通知权限")
                            }
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("下一次：") + reminders.nextReminderText
                        color: Responsive.accentStrong
                        wrapMode: Text.WordWrap
                    }

                    Label {
                        Layout.fillWidth: true
                        visible: reminders.todayReminders.length > 0
                        text: qsTr("今日课程")
                        font.bold: true
                        color: Responsive.textStrong
                    }

                    Repeater {
                        model: reminders.todayReminders

                        delegate: Label {
                            Layout.fillWidth: true
                            text: "• " + modelData.start + " " + modelData.courseName + " · " + modelData.message
                            color: Responsive.textSecondary
                            wrapMode: Text.WordWrap
                            font.pixelSize: Responsive.fontBody
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        text: reminders.lastNotificationText
                        color: Responsive.textMuted
                        wrapMode: Text.WordWrap
                        font.pixelSize: Responsive.fontSmall
                    }
                }
            }

            // ------------------------------------------------------ 教务适配器（可选）
            GroupBox {
                Layout.fillWidth: true
                Layout.margins: Responsive.margin
                title: qsTr("教务适配器（可选 · 实验性）")

                ColumnLayout {
                    anchors.fill: parent
                    spacing: Responsive.spacing

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("隐私说明：适配器仅在你点击“导入”时主动触发一次，不保存密码、不做后台同步；")
                              + qsTr("登录 Cookie 只驻留内存，可随时清除。")
                        color: Responsive.warning
                        wrapMode: Text.WordWrap
                        font.pixelSize: Responsive.fontSmall
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: settingsPage.pairColumns
                        columnSpacing: 8
                        rowSpacing: 8

                        Label { text: qsTr("适配器") }
                        ComboBox {
                            id: adapterSelector

                            objectName: "adapterSelector"
                            Layout.fillWidth: true
                            textRole: "name"
                            model: schedule.importExport.adapterOptions
                        }

                        Label { text: qsTr("课表接口地址") }
                        TextField {
                            id: adapterScheduleUrlField

                            objectName: "adapterScheduleUrlField"
                            Layout.fillWidth: true
                            placeholderText: qsTr("http(s) 接口地址或本地文件路径")
                        }

                        Label { text: qsTr("登录页地址") }
                        TextField {
                            id: adapterLoginUrlField

                            objectName: "adapterLoginUrlField"
                            Layout.fillWidth: true
                            placeholderText: qsTr("供 WebView 打开；可留空")
                        }

                        Label { text: qsTr("登录 Cookie") }
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
                        color: Responsive.textSecondary
                        wrapMode: Text.WordWrap
                        font.pixelSize: Responsive.fontSmall
                    }

                    Flow {
                        Layout.fillWidth: true
                        spacing: Responsive.spacing

                        Button {
                            id: adapterSaveUrlButton

                            objectName: "adapterSaveUrlButton"
                            text: qsTr("保存接口地址")
                        }

                        Button {
                            id: adapterImportButton

                            objectName: "adapterImportButton"
                            text: qsTr("从适配器导入")
                        }

                        Button {
                            id: adapterClearSessionButton

                            objectName: "adapterClearSessionButton"
                            text: qsTr("清除凭证")
                        }
                    }
                }
            }

            // ------------------------------------------------------------ 数据维护
            GroupBox {
                Layout.fillWidth: true
                Layout.margins: Responsive.margin
                title: qsTr("数据")

                GridLayout {
                    anchors.fill: parent
                    columns: settingsPage.pairColumns
                    columnSpacing: 8
                    rowSpacing: 8

                    Label { text: qsTr("数据文件") }
                    Label {
                        Layout.fillWidth: true
                        text: schedule.databasePath
                        color: Responsive.textSecondary
                        wrapMode: Text.WrapAnywhere
                    }

                    Label { text: qsTr("课程数量") }
                    Label { text: String(schedule.courseCount) }

                    Item {
                        Layout.fillWidth: true
                        visible: settingsPage.wideForm
                    }

                    Flow {
                        Layout.fillWidth: true
                        Layout.columnSpan: settingsPage.wideForm ? 2 : 1
                        spacing: Responsive.spacing

                        Button {
                            id: reloadButton

                            objectName: "reloadButton"
                            text: qsTr("从数据库重新加载")
                        }

                        Button {
                            id: saveNowButton

                            objectName: "saveNowButton"
                            text: qsTr("立即保存")
                        }
                    }
                }
            }

            Label {
                Layout.margins: Responsive.margin
                Layout.fillWidth: true
                text: qsTr("Schedule v%1 · 本地课表应用（不做云同步 / 账号系统）\n数据仅保存在本机，导入导出可指定任意目录。")
                          .arg(schedule.version)
                color: Responsive.textMuted
                wrapMode: Text.WordWrap
            }

            Item { Layout.fillHeight: true }
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
