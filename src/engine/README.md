# engine（引擎 / QML 桥接层）

## 职责

把 C++ 侧的服务以 `QObject` 形式暴露给 QML（属性 / 槽 / 信号），是 UI 层与 `core` + `data` 之间的桥梁：

- 持有并刷新 `QAbstractListModel` 派生模型（课程列表、当前周排布）；
- 把课表状态以只读 `Q_PROPERTY` 暴露给 QML；
- 提供课程增删改查、学期设置、节次设置、周次切换等槽函数，并在数据变化后自动落库；
- 编排导入导出（预览 → 策略 → 落库）并把进度与结果转发为信号；
- 提供本地课程提醒：`NotificationService` 周期性计算待提醒课程，并交给可插拔的 `INotificationBackend` 投递系统通知。

明确不负责：业务规则判定（`core`）、文件解析与落盘（`data`）、界面绘制与文件对话框（`ui`）。

## 依赖

| 依赖                     | 类型   | 说明                                                                    |
| ------------------------ | ------ | ----------------------------------------------------------------------- |
| `ScheduleCore`           | 项目内 | 领域模型、`ScheduleService`、`ConflictDetector`、`WeekCalculator`       |
| `ScheduleData`           | 项目内 | `IScheduleRepository`、`ImportManager` / `ExportManager`、`AppSettings` |
| `Qt6::Core` / `Qt6::Qml` | 外部   | `QObject`、`QAbstractListModel`、元对象系统（需 `AUTOMOC`）             |

- 允许依赖：`core`、`data`
- 禁止依赖：`ui`、`app`

## 产物

- 目标名：`ScheduleEngine`（静态库，PUBLIC 链接 ScheduleCore / ScheduleData / Qt6::Core / Qt6::Qml）
- 公开头文件目录：`include/`（引用方式 `#include "engine/ScheduleBridge.h"`）

## 目录结构

```text
src/engine/
├── CMakeLists.txt
├── README.md
├── include/engine/
│   ├── AppBridge.h            # 版本信息 + 自检用“测试”按钮槽
│   ├── AndroidNotificationBackend.h  # Android 本地通知后端（仅 Android 构建）
│   ├── CourseListModel.h      # 课程列表模型
│   ├── INotificationBackend.h # 通知后端接口 + 空后端
│   ├── ImportExportBridge.h   # 导入导出桥接
│   ├── NotificationService.h  # 本地提醒服务
│   ├── ScheduleBridge.h       # 课表主桥接
│   └── SessionListModel.h     # 当前周排布模型
└── src/                       # 与 include/ 同构的实现文件
```

## 公开接口与关键类型

### `ScheduleBridge`（课表主桥接）

**注入方式**（`src/app/main.cpp`）：

```cpp
Schedule::ScheduleBridge schedule_bridge(&schedule_service, repository.get(), &app_settings);
schedule_bridge.initialize();                     // 先从数据库载入，再加载 QML
engine.rootContext()->setContextProperty("schedule", &schedule_bridge);
```

**只读属性**（全部带 `NOTIFY`，QML 只做绑定，不写命令式赋值）：

| 属性 | 类型 | 说明 |
| --- | --- | --- |
| `courseModel` | `QAbstractItemModel*` | 课程列表模型（`CONSTANT`） |
| `sessionModel` | `QAbstractItemModel*` | 当前周排布模型（`CONSTANT`） |
| `importExport` | `ImportExportBridge*` | 导入导出桥接（`CONSTANT`） |
| `hasSemester` / `semesterName` / `semesterStartDate` / `semesterEndDate` / `totalWeeks` | bool / string / int | 学期元数据 |
| `currentWeek` | int | 今天所在周次（0 表示不在学期内） |
| `selectedWeek` / `selectedDay` / `selectedWeekRange` | int / int / string | 当前查看的周与星期（`selectedDay == 0` 表示整周） |
| `todayText` | string | 今天日期文本 |
| `courseCount` | int | 课程数量 |
| `conflictCount` / `hasBlockingConflicts` / `conflictSummary` / `conflicts` | int / bool / string / list | 冲突信息（`conflicts` 为 map 列表） |
| `timeSlots` | list | 作息表（`index` / `label` / `start` / `end` / `duration`） |
| `weekOptions` / `dayOptions` | list | 下拉框选项（`value` / `label`） |
| `databasePath` / `version` | string | 展示用信息 |
| `lastError` / `lastInfo` | string | 最近一次错误 / 提示 |

**槽**（由 C++ 侧从 QML 控件信号显式调用）：

| 槽 | 说明 |
| --- | --- |
| `initialize()` | 从仓库载入当前学期并初始化模型（返回 `bool`，非槽） |
| `reload_from_repository()` / `save_to_repository()` | 手动重新加载 / 保存 |
| `create_semester(name, startDate, totalWeeks)` | 新建学期（自动生成默认作息表） |
| `update_semester(name, startDate, totalWeeks)` | 修改当前学期元数据 |
| `reset_time_slots_to_default()` / `save_time_slot(index, label, start, end)` | 节次设置 |
| `select_week(week)` / `go_to_current_week()` / `previous_week()` / `next_week()` | 周次切换 |
| `select_day(dayOfWeek)` | 切换星期（0 = 整周） |
| `save_course(map)` / `remove_course(id)` | 课程增删改（`map` 键见 `CourseListModel` 文档） |
| `refresh_conflicts()` | 手动重算冲突 |

**信号**：`semesterChanged` / `currentWeekChanged` / `selectedWeekChanged` / `selectedDayChanged` / `coursesChanged` / `timeSlotsChanged` / `conflictsChanged` / `errorOccurred(message)` / `infoMessage(message)`。

### `CourseListModel`（课程列表）

`QAbstractListModel`，数据由 `ScheduleBridge` 在 `courses_changed` 后整体刷新。角色：`courseId`、`name`、`code`、`teacher`、`location`、`color`、`credits`、`notes`、 `sessionCount`、`weekExpression`、`weekDisplay`、`daySummary`。另提供 `get(row)` / `find(courseId)` / `index_of(courseId)` 供编辑器回填表单。

### `SessionListModel`（当前周排布）

`QAbstractListModel`，按“周（+ 可选星期过滤）”提供排布数据。角色：`courseId`、`courseName`、`sessionId`、`dayOfWeek`、`dayName`、`startSlot`、 `endSlot`、`slotCount`、`weeks`、`weeksDisplay`、`teacher`、`location`、`color`、 `startTime`、`endTime`、`date`、`week`、`rowSpan`。方法：`set_week(int)`、`set_day_filter(int)`、`refresh()`、`count()`、`get(row)`。

### `ImportExportBridge`（导入导出）

| 属性 | 说明 |
| --- | --- |
| `defaultImportDir` / `defaultExportDir` | 默认目录（未设置时 `Documents/Schedule`） |
| `lastImportDir` / `lastExportDir` | 最近使用的目录（用于对话框初始位置，**由 C++ 读取后设置到对话框**） |
| `hasPendingPreview` / `previewSummary` / `previewFormatName` | 预览状态 |
| `previewNewCount` / `previewDuplicateCount` / `previewConflictCount` | 预览统计 |
| `previewWarnings` / `previewConflicts` | 预览提示与冲突明细 |
| `strategyNames` / `formatNames` | 下拉框选项文本（`CONSTANT`） |
| `lastExportPath` / `lastExportSummary` / `lastImportSummary` / `lastError` / `progress` | 结果与进度 |

槽：`preview_import(QUrl)`、`apply_import(strategyIndex)`、`cancel_import()`、 `export_schedule(formatIndex, QUrl)`、`set_default_import_dir(QUrl)`、 `set_default_export_dir(QUrl)`、`reset_default_directories()`、`refresh_directories()`。

信号：`importPreviewReady(bool, QString)`、`importFinished(bool, QString)`、 `exportFinished(bool, QString, QString)`、`progressChanged(int, QString)`、 `errorOccurred(QString)`、`directoriesChanged()`、`previewChanged()`、 `importFinishedChanged()`、`exportFinishedChanged()`。

> **文件对话框在 UI 层**：QML 使用 `QtQuick.Dialogs` 的 `FileDialog` / `FolderDialog`，C++ 侧连接其 `accepted` 信号后读取 `selectedFile` / `selectedFolder` 再调用上述槽。

### `NotificationService`（本地提醒）

| 属性                             | 说明                                                                |
| -------------------------------- | ------------------------------------------------------------------- |
| `enabled`                        | 是否启用提醒（持久化在设置中）                                      |
| `minutesBefore` / `minutesIndex` | 提前分钟数（5 / 10 / 15）及其下标（供 ComboBox 绑定）               |
| `backendName` / `backendStatus`  | 当前通知后端名称与权限状态                                          |
| `nextReminderText`               | 下一次提醒的可读文本                                                |
| `todayReminders`                 | 今天的课程提醒（`title` / `message` / `start` / `remindAt` 等字段） |
| `minutesOptions`                 | 提前分钟数下拉项                                                    |
| `lastNotificationText`           | 最近一次通知文本                                                    |

槽：`set_enabled(bool)`、`set_minutes_before(int)`、`refresh()`、`check_now()`、 `show_test_notification()`、`request_permission()`。

信号：`settingsChanged()`、`backendChanged()`、`scheduleChanged()`、 `notificationRequested(title, message)`、`reminderDue(courseId, courseName, minutesBefore)`。

工作方式与平台差异：

- 每 **30 秒**轮询一次（`poll_interval_ms()`），用 `core::ReminderScheduler` 计算到点提醒；
- 同一条提醒（课程 + 时间段 + 周次 + 日期）只通知一次；跨天自动清空去重集合；
- 应用前后台切换时补检查一次（通过 QtCore 的 `QEvent::ApplicationStateChange` 事件过滤器， **不引入 `Qt6::Gui` 依赖**），覆盖移动端进程被冻结导致的漏提醒；
- 无论系统通知是否可用，都会发出 `notificationRequested`，由 QML 应用内横幅兜底展示。

### 通知后端 `INotificationBackend`

| 后端 | 位置 | 说明 |
| --- | --- | --- |
| `NullNotificationBackend` | engine | 默认后端，不做系统投递，仅应用内横幅 |
| `AndroidNotificationBackend` | engine | Android `NotificationManager` 本地通知（`QJniObject`，仅 Android 构建，需真机验证） |
| `TrayNotificationBackend` | **app** | 桌面系统托盘气泡（`QSystemTrayIcon` 属 Qt Widgets，故放在组装层） |

注册方式（见 `src/app/main.cpp`）：

```cpp
Schedule::NotificationService notification_service(&schedule_service, &app_settings);
Schedule::TrayNotificationBackend tray_backend;
notification_service.set_backend(&tray_backend);   // 不可用时自动回退为空后端
notification_service.request_permission();
notification_service.start();
engine.rootContext()->setContextProperty("reminders", &notification_service);
```

### `AppBridge`

早期占位对象，保留 `version` 属性与 `test_button_clicked()` 槽，供 `--selftest` 验证“C++ 侧显式连接”链路。

## 构建与测试方式

```bash
cmake --preset windows-msvc
cmake --build --preset windows-msvc-debug
./build/windows-msvc/Debug/Schedule.exe
```

- 本模块由根 `CMakeLists.txt` 通过 `add_subdirectory(src/engine)` 引入。
- 桥接层的端到端验证通过 app 层的 `--selftest`（阶段 7 扩展为导入导出全流程）。
- 针对桥接对象的 QTest 单元测试在**阶段 7** 补充（本阶段以手工运行与 `--selftest` 验证）。

## 与上下层交互方式

- 向下：调用 `core::ScheduleService` 与 `data` 的仓库 / 导入导出管理器 / 设置门面。
- 向上：由 `app` 层实例化并以上下文属性（`schedule`）注入 QML；`ui` 层只读取属性。

## 信号连接约定

- 与桥接对象之间的信号连接**统一在 C++ 侧显式建立**（`QObject::connect`，见 `src/app/main.cpp`）；QML **不写** `onClicked` / `Connections` 等按名称隐式连接的写法。
- QML 控件通过 `objectName` 暴露给 C++，由 `app` 层在 `engine.load()` 之后 `findChild` 并连接。
- 命名遵循全小写 + 下划线（槽 `save_course()`、信号 `courses_changed()` 等）。
- 桥接对象对 QML 暴露的状态一律通过 `Q_PROPERTY`（只读 + `NOTIFY`）提供，避免 QML 直接持有业务对象指针。
- **服务 → 模型的刷新链**：`ScheduleService` 的信号在 `ScheduleBridge::connect_service()` 中集中连接，先重算冲突再刷新模型，最后发出桥接层信号；QML 只需绑定属性。

## 扩展点与注意事项

- **扩展点**：新增桥接能力时优先在既有桥接对象上增加 `Q_PROPERTY` / 槽 / 信号，而不是让 QML 直接访问 `core` / `data` 类型。
- **对象生命周期**：桥接对象由 `app` 持有（栈对象），声明顺序必须在 `QQmlApplicationEngine` **之前**，保证引擎先析构。
- **模型更新**：两个模型都采用 `beginResetModel()` / `endResetModel()` 整体刷新。数据量（一学期几十门课）很小，整表刷新比增量维护更不易出错。
- **课程编辑的数据格式**：`save_course(map)` 的 map 字段与 `CourseListModel::get()` 的输出完全一致，因此“读出来编辑 → 原样写回”是闭合的；新增课程时把 `courseId` 置空。
- **自动保存**：所有修改类槽都会调用 `save_to_repository()`；保存失败会发 `errorOccurred` 但不会回滚内存状态（用户可重试）。
- **`slots` 是 Qt 宏**：不要用 `slots` 作为局部变量名（会展开成空）。

## 后续阶段计划

| 阶段   | 内容                                                                                                  |
| ------ | ----------------------------------------------------------------------------------------------------- |
| 阶段 5 | QML 界面消费本层属性与模型；`app` 层按 `objectName` 建立全部交互连接                                  |
| 阶段 6 | ✅ 已完成：`NotificationService` + `INotificationBackend`（桌面托盘 / Android 本地通知 / 应用内横幅） |
| 阶段 7 | 补充桥接对象的 QTest 单元测试                                                                         |
| 阶段 9 | 注册可选教务适配器（接口在 `core`，实现在 `data`，注册在 `app`）                                      |

## 相关文档

- [根 README](../README.md)
- [阶段路线图](../docs/ROADMAP.md)
- [模块 README 模板](../docs/README_TEMPLATE.md)
- [core](../core/README.md) — 本层依赖的核心逻辑层
- [data](../data/README.md) — 本层依赖的基础设施层
- [data/import_export](../data/import_export/README.md) — 导入导出子系统
- [ui](../ui/README.md) / [app](../app/README.md) — 使用本层桥接对象的上层
