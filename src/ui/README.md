# ui（QML 界面层）

## 职责

提供全部界面（QML）与静态资源，承载**文件 / 目录选择对话框**，按目标形态区分桌面与移动布局。界面只通过属性绑定读取 `engine` 层注入的桥接对象状态，所有交互由 `app` 层在 C++ 侧显式连接。

明确不负责：业务规则（`core`）、文件解析与落盘（`data`）、QML ↔ C++ 的连接建立（`app`）。

## 依赖

| 依赖                      | 类型   | 说明                                                            |
| ------------------------- | ------ | --------------------------------------------------------------- |
| `ScheduleEngine`          | 项目内 | 提供 `AppBridge` / `ScheduleBridge`（含 `importExport` 子对象） |
| `Qt6::Quick` / `Qt6::Qml` | 外部   | Quick Controls 2 / Layouts                                      |
| `Qt6::QuickDialogs2`      | 外部   | `FileDialog` / `FolderDialog`（`import QtQuick.Dialogs`）       |

- 允许依赖：`engine`
- 禁止依赖：`data`（不得直接调用仓库或导入导出实现）、`core`、`app`

## 产物

- 目标名：`ScheduleUI`（静态库 + QML 模块，URI: `Schedule`，版本 1.0）
- QML 文件以资源方式内嵌，运行时路径前缀为 `qrc:/qt/qml/Schedule/`

## 目录结构

```text
src/ui/
├── CMakeLists.txt
├── README.md
├── qml/
│   ├── MainDesktop.qml    # 桌面 / 平板主界面（1180x760）
│   ├── MainMobile.qml     # 手机竖屏主界面（480x860）
│   ├── WeekView.qml       # 周视图：节次栏 + 7 个星期列
│   ├── DayView.qml        # 日视图：按天列出课程
│   ├── CourseCard.qml     # 课程卡片（周 / 日视图复用）
│   ├── CourseEditor.qml   # 课程编辑对话框（含时间段草稿列表）
│   ├── CourseDetailDialog.qml # 课程详情弹层（点击课卡后展示，可进入编辑器）
│   ├── ImportWizard.qml   # 导入向导（选文件 → 预览 → 策略 → 应用）
│   ├── ExportDialog.qml   # 导出对话框（选格式与目录，展示实际路径）
│   ├── SemesterPage.qml   # 学期设置 + 课程列表 + 冲突列表
│   └── SettingsPage.qml   # 目录设置 / 作息表设置 / 数据维护 / 关于
├── style/                 # 【QML 单例】设计令牌：页面只引用，不写字面量
│   ├── Responsive.qml     # 断点 + 响应式尺寸 + 派生判断与派生函数
│   ├── Metrics.qml        # 间距刻度 / 圆角 / 描边 / 通用组件尺寸
│   ├── Theme.qml          # 颜色令牌（文字 / 主色 / 表面 / 描边 / 选中态 / 课卡 / 横幅）
│   ├── Typography.qml     # 字号令牌
│   ├── CourseCardStyle.qml # 课卡专属：高度推导参数 / 紧凑阈值 / 内边距 / 描边
│   └── ListItemStyle.qml  # 列表项专属：行高 / 色条 / 内边距
└── resources/
    └── assets.qrc         # 静态资源清单（占位）
```

## 公开接口与关键类型

本层不导出 C++ 类型，只导出 QML 类型（同模块内可直接互相引用，无需 `import`）。两个主界面暴露同一套“契约 `objectName`”，因此 `app` 层的连接代码在桌面 / 移动端通用（例外：`addCourseButton` / `importButton` / `exportButton` 三个次要操作按钮只保留在桌面宽屏工具栏中，窄屏与移动端由 `moreMenuButton` + 菜单项承接）：

| 区域 | objectName |
| --- | --- |
| 页面栈 | `pageStack` |
| 导航 | `navWeekButton`、`navDayButton`、`navSemesterButton`、`navSettingsButton` |
| 周次 | `prevWeekButton`、`nextWeekButton`、`currentWeekButton`、`weekSelector` |
| 星期 | `daySelector` |
| 课程 | `addCourseButton`、`pageNewCourseButton`、`editCourseButton`、`deleteCourseButton`、`courseList` |
| 学期 | `semesterNameField`、`semesterStartField`、`semesterWeeksSpin`、`saveSemesterButton` |
| 设置 | `importDirField`、`chooseImportDirButton`、`importDirDialog`、`exportDirField`、`chooseExportDirButton`、`exportDirDialog`、`saveDirsButton`、`resetDirsButton`、`slotSelector`、`slotLabelField`、`slotStartField`、`slotEndField`、`saveSlotButton`、`resetSlotsButton`、`reloadButton`、`saveNowButton`、`testButton` |
| 提醒 | `reminderEnabledCheck`、`reminderMinutesSelector`、`testNotificationButton`、`requestPermissionButton`、`notificationBanner` |
| 适配器 | `adapterSelector`、`adapterScheduleUrlField`、`adapterLoginUrlField`、`adapterCookieField`、`adapterSaveUrlButton`、`adapterImportButton`、`adapterClearSessionButton` |
| 编辑器 | `courseEditor`、`editor*Field`、`sessionDraftModel`、`sessionList`、`sessionRowClick`（`sessionList` 委托内的热区）、`session*`、`sessionAddButton`、`sessionUpdateButton`、`sessionRemoveButton`、`courseSaveButton`、`courseCancelButton` |
| 课程详情 | `courseDetailDialog`、`courseDetailName`、`courseDetailCode`、`courseDetailTeacher`、`courseDetailLocation`、`courseDetailCredits`、`courseDetailNotes`、`courseDetailSessions`、`courseDetailEditButton`、`courseDetailCloseButton` |
| 窄屏折叠菜单 | `moreMenuButton`（触发）、`moreMenu`（菜单本体）、`addCourseMenuItem`、`importMenuItem`、`exportMenuItem` |
| 导入 | `importWizard`、`importFileField`、`importChooseFileButton`、`importFileDialog`、`importStrategySelector`、`importApplyButton`、`importCancelButton` |
| 导出 | `exportDialog`、`exportFormatSelector`、`exportDirField`、`exportChooseDirButton`、`exportResetDirButton`、`exportDirDialog`、`exportConfirmButton`、`exportCancelButton` |
| 动态课卡 | `sessionCardClick`（由 `CourseCard` 提供，含 `courseId` 属性） |
| 课程列表项 | `courseListItemClick`（由 `SemesterPage` 的 `ListView` 委托提供，含 `itemIndex` 属性） |

## 界面与数据绑定

| 界面位置 | 绑定的桥接属性 |
| --- | --- |
| 顶部回显 | `schedule.semesterName`、`schedule.totalWeeks`、`schedule.currentWeek`、`schedule.selectedWeekRange` |
| 周次 / 星期下拉 | `schedule.weekOptions`、`schedule.dayOptions` |
| 周视图网格 | `schedule.weekModel`（整周）、`schedule.timeSlots`（节次标题） |
| 日视图列表 | `schedule.sessionModel`（受 `selectedDay` 过滤） |
| 课程列表 | `schedule.courseModel` |
| 冲突提示 | `schedule.conflictSummary`、`schedule.conflicts`、`schedule.hasBlockingConflicts` |
| 导入向导 | `schedule.importExport.previewSummary` / `previewWarnings` / `previewConflicts` / `lastImportSummary` / `progress` |
| 导出对话框 | `schedule.importExport.defaultExportDir` / `lastExportSummary` / **`lastExportPath`** |
| 状态栏 | `schedule.lastError` / `schedule.lastInfo` |
| 提醒设置 | `reminders.enabled` / `minutesIndex` / `backendName` / `backendStatus` / `nextReminderText` / `todayReminders` / `lastNotificationText` |
| 适配器设置 | `schedule.importExport.adapterOptions` / `adapterSessionStatus` |

## 构建与测试方式

```bash
cmake --preset windows-msvc
cmake --build --preset windows-msvc-debug
./build/windows-msvc/Debug/Schedule.exe
```

- 由根 `CMakeLists.txt` 通过 `add_subdirectory(src/ui)` 引入。
- 界面自检通过 app 层 `-DBUILD_SELFTEST=ON` + `Schedule.exe --selftest` 完成（阶段 7 扩展为导入 / 导出全流程校验），依赖上表中的稳定 `objectName`。
- QML 文件在构建期由 `qt6_add_qml_module()` 编入资源；新增页面必须同步加入 `CMakeLists.txt` 的 `QML_FILES` 列表；新增 `style/` 单例还须逐文件声明 `QT_QML_SINGLETON_TYPE`（详见「样式令牌」一节）。

## 与上下层交互方式

- 向下：只读取 `app` 注入的上下文属性 `schedule`（`ScheduleBridge*`）与 `bridge`（`AppBridge*`），不 `import` C++ 类型。
- 向上：由 `app` 按平台加载主 QML：

```cpp
// 桌面：qrc:/qt/qml/Schedule/qml/MainDesktop.qml
// 移动：qrc:/qt/qml/Schedule/qml/MainMobile.qml
```

- 文件选择：`ImportWizard` 使用 `FileDialog`，`ExportDialog` 与 `SettingsPage` 使用 `FolderDialog`；选中的 `QUrl` 由 C++ 侧读取后交给 `ImportExportBridge`，**数据层只接收路径**。

## 样式令牌（style/ 单例）

颜色、字号、间距、圆角与组件尺寸全部集中在 `style/` 下的 6 个 QML 单例（CMake 中通过 `QT_QML_SINGLETON_TYPE` 注册），页面只引用令牌或 `Responsive.isXxx(...)` / `Responsive.xxx(...)` 判断，不再各写一套魔法数字。单例之间可以互相依赖（`Responsive.dayCardHeight()` 用 `CourseCardStyle`，`CourseCardStyle` / `ListItemStyle` 用 `Metrics`）：

| 单例 | 职责 | 主要内容 |
| --- | --- | --- |
| `Responsive` | 断点与派生 | 窗口 / 学期页 / 表单 / 对话框断点；`isCompactToolbar()`、`isNarrow()`、`isShort()`、`isTiny()`、`isWideSplit()`、`isDialogNarrow()`、`editorColumns()`、`settingsColumns()`、`settingsPairColumns()`、`dialogWidth()`、`dialogHeight()`、`daySelectorWidth()`、`bannerWidth()`、`exportPreviewHeight()`、`importPreviewHeight()`、`dayCardHeight()` |
| `Metrics` | 通用度量 | 间距刻度 `spacing2xs(1) ~ spacing4xl(24)`、圆角 `radiusXs(3) ~ radiusLg(10)`、`borderWidth`、工具栏宽度、周视图网格、侧栏 / 面板、通知横幅 |
| `Theme` | 颜色 | 文字 `textPrimary/Strong/Secondary/Muted/Subtle`、主色 `accent/accentStrong/danger/success/warning`、表面 `surface/surfaceAlt/surfaceSubtle`、描边 `border/divider`、表头、选中态、课卡文字、横幅文字、节次时间 |
| `Typography` | 字号 | `fontTiny(9) / fontCaption(10) / fontSmall(11) / fontBody(12) / fontBodyLarge(13) / fontSubheading(14) / fontHeading(16) / fontTitle(18)` |
| `CourseCardStyle` | 课卡 | 高度推导参数、`dense*` / `tight*` 紧凑阈值、`inset`、内边距、行距、圆角、描边加深系数 |
| `ListItemStyle` | 列表项 | 行高 `height(40) / heightTall(62)`、圆角、色条、文本内边距、`preferredHeight(132)` |

> **注册方式**：`QT_QML_SINGLETON_TYPE` 是**逐文件**属性。一个 `set_source_files_properties()` 里写多组同名属性只有最后一组生效——曾因此让 6 个单例全部指向 `Typography.qml`。新增单例时必须一个文件一条调用（见 `src/ui/CMakeLists.txt`）。

### 断点与阈值

| 常量 | 值 | 用途 |
| --- | --- | --- |
| `compactToolbarWidth` | 1440 | 桌面工具栏折叠次要操作（完整工具栏实测需约 1400px） |
| `narrowWidth` | 800 | 桌面隐藏标题 / 学期回显 |
| `shortHeight` | 520 | 低高度（横屏 / 分屏）压缩纵向占位 |
| `minWindowWidth` / `minWindowHeight` | 640 / 420 | 主窗口最小尺寸 |
| `wideSplitWidth` | 900 | 学期页左右 / 上下分栏切换 |
| `wideFormWidth` | 640 | 设置页表单多列 / 单列切换 |
| `editorWideWidth` / `editorMediumWidth` | 620 / 420 | 课程编辑器 4 / 2 / 1 列 |
| `dialogWideWidth` | 460 | 导出对话框标签并排 / 上下 |
| `tinyWidth` | 360 | 移动端与日视图选择栏的极窄断点 |
| `minDayWidth` | 64 | 周视图最小可读列宽，再窄改为横向滚动 |
| `dayHeaderCompactWidth` / `dayHeaderDateWidth` | 72 / 52 | 周视图表头星期字号降档 / 隐藏日期的列宽 |
| `slotTimeVisibleHeight` | 48 | 节次栏显示上下课时间的最低节次高度 |
| `CourseCardStyle.minHeight` / `preferredHeight` / `maxHeight` | 60 / 96 / 112 | 日视图卡片高度区间 |
| `CourseCardStyle.denseWidth` / `denseHeight` | 104 / 96 | 课卡隐藏地点、教师的阈值 |
| `CourseCardStyle.tightWidth` / `tightHeight` | 84 / 44 | 课卡只留课程名与时间的阈值 |

### 回归测试矩阵

回归验证使用 `custom/tools` 的窗口截图链路（离屏烟测 + `PrintWindow` 抓图 + 空白帧检测），固定跑下面 7 档尺寸；本机屏幕缩放 125%，顺带覆盖高 DPI：

| 尺寸     | 覆盖场景                                      |
| -------- | --------------------------------------------- |
| 320×568  | 最窄手机竖屏：周视图横向滚动 + 提示，课卡紧凑 |
| 360×640  | 小屏手机竖屏：极窄断点边界                    |
| 390×844  | 主流手机竖屏：4 Tab + 折叠菜单                |
| 480×860  | 大屏手机竖屏：横向滚动边界                    |
| 844×390  | 手机横屏 / 低高度窗口：头部压缩、节次高度收缩 |
| 900×600  | 桌面窄窗口：工具栏折叠、学期页上下分栏        |
| 1180×760 | 桌面默认：折叠形态、7 列完整可见              |

## 信号连接约定

- QML **不写** `onClicked` / `Connections` / `onXxx` 等任何信号处理器；所有交互（按钮点击、下拉切换、对话框确认、列表选中）由 `app/UiConnector` 在 C++ 侧按 `objectName` 显式连接。
- 因此每个交互控件都必须有**唯一且稳定**的 `objectName`；新增交互控件时须同步更新 `UiConnector` 与本文档的表格。
- 列表选中由委托内的 `courseListItemClick` 热区驱动：C++ 连接 `clicked()` 后把该委托的 `itemIndex` 写回 `courseList.currentIndex`，高亮仍由 `ListView.isCurrentItem` 负责。
- 周次（`weekSelector`）与星期（`daySelector`）下拉框的 `currentIndex` **不在 QML 中绑定** `schedule.selectedWeek` / `selectedDay`：用户操作会破坏绑定，且 `currentValue` 在 `currentIndexChanged` 触发时尚未更新。改由 `UiConnector` 读 `currentIndex` 下发，并在桥接信号回来时带抑制标志回写。
- 周 / 日视图的课卡由 `Repeater` 动态生成，课卡经 `setParentItem()` 挂进可视树、不进入 `QObject::children()`，因此 `UiConnector` 沿 `QQuickItem::childItems()` 扫描 `sessionCardClick`，并在承载委托的可视父级 `childrenChanged` / 模型 `modelReset` 后重扫。
- 点击课卡先弹出课程详情弹层（`courseDetailDialog`），再由其中的「编辑」（`courseDetailEditButton`）进入 `courseEditor`。
- 学期页的列表项由 `ListView` 动态生成：委托只挂在 `contentItem` 的**可视**子树下，不进入 `QObject::children()`，`findChildren()` 扫不到。因此 `UiConnector` 沿 `QQuickItem::childItems()` 扫描 `courseListItemClick`，并在 `contentItem` 的可视子项变化（`childrenChanged`）时重扫，保证滚动或模型重置后新出现的行同样可点。

## 扩展点与注意事项

- **新增页面**：在 `qml/` 下新增文件 → 注册到 `CMakeLists.txt` → 在 `MainDesktop` / `MainMobile` 的 `pageStack` 中追加 → 在 `UiConnector::connect_navigation()` 中补充导航连接。
- **新增样式**：能落到现有令牌的（颜色 / 字号 / 间距 / 圆角 / 组件尺寸）直接引用对应单例；确实缺令牌时补进 `style/` 的对应文件（通用 → `Metrics`，颜色 → `Theme`，字号 → `Typography`，课卡 → `CourseCardStyle`，列表项 → `ListItemStyle`，断点或派生量 → `Responsive`），**不要**在页面里留字面量。
- **命名约定**：QML 内部 id / 属性用 `camelCase`（符合 Qt 与 Prettier 惯例）， `objectName` 面向 C++ 连接，必须与 `UiConnector` 中的字符串常量保持一致。
- **绑定循环**：`ScrollView` 中不要同时写 `contentWidth: availableWidth` 与依赖 `availableWidth` 的内容宽度，否则 Qt 会报 "Binding loop detected"；本项目统一关闭横向滚动条并让内容宽度直接跟随 `ScrollView.width`。
- **移动端**：触摸目标不小于 48dp；`MainMobile.qml` 不使用悬浮窗口类控件，底部导航与桌面版共用同一批 `objectName`。
- **应用内提醒横幅**：`notificationBanner` 在 `MainDesktop` / `MainMobile` 中各有一份；C++ 侧收到 `NotificationService::notificationRequested` 后写入 `bannerTitle` / `bannerMessage`，并在若干秒后清空（见 `UiConnector::show_banner()`）。这是系统通知不可用时的兜底展示通路。
- **主题色**：当前使用固定浅色配色，全部集中在 `style/Theme.qml`（`#1F2A44` 主文字、`#4C8DFF` 主色、`#C0392B` 告警色）；深色主题留待后续在 `AppSettings::theme()` 基础上扩展，届时只需替换 `Theme` 单例的取值。

## 相关文档

- [根 README](../README.md)
- [阶段路线图](../docs/ROADMAP.md)
- [模块 README 模板](../docs/README_TEMPLATE.md)
- [engine](../engine/README.md) — 提供桥接对象的层
- [data/import_export](../data/import_export/README.md) — 导入导出实现
- [app](../app/README.md) — 建立全部 QML ↔ C++ 连接的应用入口
