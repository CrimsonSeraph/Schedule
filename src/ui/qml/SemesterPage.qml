import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 学期页：维护学期元数据（名称 / 起始日 / 总周数）并管理课程列表。
//
// 交互约定：所有按钮与列表都不带 onClicked；C++ 侧（app 层 UiConnector）显式连接。
//  - 学期表单的保存按钮：C++ 读取 semesterNameField / semesterStartField / semesterWeeksSpin，
//    根据 schedule.hasSemester 决定调用 create_semester() 还是 update_semester()；
//  - 课程列表使用 ListView 内建的 currentIndex 选中行为，C++ 读取该属性完成编辑 / 删除。
//
// 响应式策略：
//  - SplitView 方向随宽度切换：够宽时左表单 / 右列表；窄屏（< 900）改为上下排列，
//    表单限高、课程列表占据剩余高度，避免左右都被压到不可用；
//  - 课程列表的操作按钮改用 Flow，窄屏自动换行而不是被挤出视口。
Item {
    id: semesterPage

    objectName: "semesterPage"

    // C++ 在打开编辑器时会把选中课程 id 写回这里（供界面高亮，可选）
    property string highlightedCourseId: ""

    // 宽屏：左右分栏；窄屏：上下分栏
    readonly property bool wideLayout: Responsive.isWideSplit(semesterPage.width)

    SplitView {
        anchors.fill: parent
        orientation: semesterPage.wideLayout ? Qt.Horizontal : Qt.Vertical

        // ---------------------------------------------------------------- 学期表单
        ScrollView {
            // 宽屏时占左侧固定宽度；窄屏时改为限制纵向高度（-1 表示不参与该方向的分配）
            SplitView.preferredWidth: semesterPage.wideLayout ? 320 : -1
            SplitView.minimumWidth: semesterPage.wideLayout ? 260 : -1
            SplitView.preferredHeight: semesterPage.wideLayout ? -1 : 300
            SplitView.minimumHeight: semesterPage.wideLayout ? -1 : 200
            clip: true

            ColumnLayout {
                width: parent.width
                spacing: 10

                Label {
                    Layout.margins: Responsive.margin
                    text: qsTr("学期设置")
                    font.bold: true
                    font.pixelSize: Responsive.fontHeading
                    color: Responsive.textPrimary
                }

                GridLayout {
                    Layout.margins: Responsive.margin
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: 8
                    rowSpacing: 8

                    Label { text: qsTr("学期名称") }
                    TextField {
                        id: semesterNameField

                        objectName: "semesterNameField"
                        Layout.fillWidth: true
                        placeholderText: qsTr("如 2024-2025 学年第一学期")
                    }

                    Label { text: qsTr("起始日期") }
                    TextField {
                        id: semesterStartField

                        objectName: "semesterStartField"
                        Layout.fillWidth: true
                        placeholderText: qsTr("yyyy-MM-dd（第 1 周周一）")
                    }

                    Label { text: qsTr("总周数") }
                    SpinBox {
                        id: semesterWeeksSpin

                        objectName: "semesterWeeksSpin"
                        Layout.fillWidth: true
                        from: 1
                        to: 64
                        value: 20
                        editable: true
                    }
                }

                Button {
                    id: saveSemesterButton

                    objectName: "saveSemesterButton"
                    Layout.margins: Responsive.margin
                    Layout.fillWidth: true
                    text: schedule.hasSemester ? qsTr("保存学期信息") : qsTr("创建学期")
                }

                Label {
                    Layout.margins: Responsive.margin
                    Layout.fillWidth: true
                    text: schedule.hasSemester
                          ? qsTr("当前：%1\n%2 ~ %3（共 %4 周）")
                                .arg(schedule.semesterName)
                                .arg(schedule.semesterStartDate)
                                .arg(schedule.semesterEndDate)
                                .arg(schedule.totalWeeks)
                          : qsTr("尚未创建学期")
                    color: Responsive.textSecondary
                    wrapMode: Text.WordWrap
                }

                Label {
                    Layout.margins: Responsive.margin
                    Layout.fillWidth: true
                    text: qsTr("共 %1 门课程 · %2").arg(schedule.courseCount).arg(schedule.conflictSummary)
                    color: schedule.hasBlockingConflicts ? Responsive.danger : Responsive.success
                    wrapMode: Text.WordWrap
                }

                Item { Layout.fillHeight: true }
            }
        }

        // ---------------------------------------------------------------- 课程列表
        ColumnLayout {
            // 宽屏：占满右侧剩余宽度；窄屏：占满下方剩余高度
            SplitView.fillWidth: semesterPage.wideLayout
            SplitView.fillHeight: !semesterPage.wideLayout
            spacing: 0

            ColumnLayout {
                Layout.fillWidth: true
                Layout.margins: Responsive.margin
                spacing: Responsive.spacing

                Label {
                    text: qsTr("课程列表")
                    font.bold: true
                    font.pixelSize: Responsive.fontHeading
                    color: Responsive.textPrimary
                }

                // 用 Flow 承载操作按钮：窄屏自动换行，不会被挤出视口
                Flow {
                    Layout.fillWidth: true
                    spacing: Responsive.spacing

                    Button {
                        id: pageNewCourseButton

                        objectName: "pageNewCourseButton"
                        text: qsTr("新建课程")
                    }

                    Button {
                        id: editCourseButton

                        objectName: "editCourseButton"
                        text: qsTr("编辑选中")
                    }

                    Button {
                        id: deleteCourseButton

                        objectName: "deleteCourseButton"
                        text: qsTr("删除选中")
                    }
                }
            }

            ListView {
                id: courseList

                objectName: "courseList"
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: Responsive.margin
                clip: true
                spacing: 6
                model: schedule.courseModel

                delegate: Rectangle {
                    width: courseList.width
                    height: 62
                    radius: 6
                    color: ListView.isCurrentItem ? "#E8F0FF" : "#FFFFFF"
                    border.width: 1
                    border.color: ListView.isCurrentItem ? "#9FBEF5" : Responsive.border

                    Rectangle {
                        id: colorBar

                        width: 6
                        height: parent.height - 16
                        anchors.left: parent.left
                        anchors.leftMargin: 6
                        anchors.verticalCenter: parent.verticalCenter
                        radius: 3
                        color: model.color
                    }

                    Column {
                        anchors.left: colorBar.right
                        anchors.leftMargin: 10
                        anchors.right: parent.right
                        anchors.rightMargin: 10
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 2

                        Text {
                            width: parent.width
                            text: model.name + (model.code.length > 0 ? "（" + model.code + "）" : "")
                            font.bold: true
                            font.pixelSize: Responsive.fontSubheading
                            elide: Text.ElideRight
                            color: Responsive.textPrimary
                        }

                        Text {
                            width: parent.width
                            text: model.daySummary.length > 0 ? model.daySummary : qsTr("未设置上课时间")
                            font.pixelSize: Responsive.fontBody
                            elide: Text.ElideRight
                            color: Responsive.textSecondary
                        }

                        Text {
                            width: parent.width
                            text: (model.location.length > 0 ? model.location + " · " : "")
                                  + (model.teacher.length > 0 ? model.teacher + " · " : "")
                                  + model.weekDisplay
                            font.pixelSize: Responsive.fontSmall
                            elide: Text.ElideRight
                            color: Responsive.textMuted
                        }
                    }
                }

                Label {
                    anchors.centerIn: parent
                    visible: schedule.courseCount === 0
                    text: qsTr("还没有课程\n点击“新建课程”开始录入")
                    horizontalAlignment: Text.AlignHCenter
                    color: Responsive.textMuted
                }
            }

            // 冲突列表
            ColumnLayout {
                Layout.fillWidth: true
                Layout.margins: Responsive.margin
                spacing: 4

                Label {
                    text: schedule.conflictSummary
                    font.bold: true
                    color: schedule.hasBlockingConflicts ? Responsive.danger : Responsive.success
                }

                Repeater {
                    model: schedule.conflicts

                    delegate: Label {
                        Layout.fillWidth: true
                        text: "• " + modelData.message
                        color: modelData.blocking ? Responsive.danger : Responsive.warning
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }
    }
}
