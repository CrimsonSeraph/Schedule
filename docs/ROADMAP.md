# Schedule 课表项目阶段路线图

本文档记录课表应用的**分层约定**与**阶段划分**，是各模块 `README.md` 的公共依据。
每完成一个阶段，需同步更新本文件中该阶段的状态。

## 一、分层约定

项目采用自底向上的五层架构，**层间只能依赖下层，禁止反向依赖**：

```text
app  ──►  ui  ──►  engine  ──►  data  ──►  core
```

| 层 | 目录 | 目标名 | 允许依赖 | 说明 |
| -- | ---- | ------ | -------- | ---- |
| core | `src/core` | `ScheduleCore` | `Qt6::Core` | 纯业务与数据逻辑，无任何 GUI 依赖，可在无显示环境下单元测试 |
| data | `src/data` | `ScheduleData` | `ScheduleCore`、`Qt6::Core`、`Qt6::Sql` | 基础设施层：SQLite 持久化、JSON 序列化、导入导出实现、可选教务适配器 |
| engine | `src/engine` | `ScheduleEngine` | `ScheduleData`、`ScheduleCore`、`Qt6::Core`、`Qt6::Qml` | QML 桥接层：`QObject` 桥接对象与 `QAbstractListModel` 模型 |
| ui | `src/ui` | `ScheduleUI`（QML 模块 URI `Schedule`） | `ScheduleEngine`、`Qt6::Quick`、`Qt6::Qml` | QML 界面与静态资源，区分为桌面 / 移动布局 |
| app | `src/app` | `Schedule`（可执行程序） | `ScheduleUI`、`ScheduleEngine`、`ScheduleData` | 应用入口：组装各层、加载 QML、**在 C++ 侧显式建立全部信号连接** |

约束细则：

- `core` 只链接 `Qt6::Core`，不得出现 `QWidget` / `QQmlEngine` / `QSqlDatabase` 等类型。
- 数据层只接收**路径**（`QString`）或 `QUrl`；文件选择对话框一律放在 UI 层。
- `app` 是唯一允许持有并组装全部层的模块；`ui` 不直接依赖 `data`。
- 不做云同步、账号系统、OAuth、用户表、同步队列、后端 API。

## 二、阶段划分

### 阶段 0：基线梳理与文档模板 —— 已完成

- 检查现有 `core` / `engine` / `ui` / `app` 的 CMake、README、`AppBridge`。
- 新增 [`docs/README_TEMPLATE.md`](README_TEMPLATE.md) 模块文档模板与本文档。
- 新增 `src/data/README.md` 占位，明确分层扩展为 `core → data → engine → ui → app`。
- 各模块 README 补充“后续阶段计划”。

### 阶段 1：core 领域模型与核心服务 —— 已完成

- 领域模型：`Semester`、`Course`、`CourseSession`、`TimeSlot`、`WeekMask`、`Conflict`。
- 核心服务：`ScheduleService`、`ConflictDetector`、`WeekCalculator`。
- 周次表达式：`1-16`、`1-16/2`、单双周、自定义周；支持当前周计算与冲突检测。
- QTest 单元测试 + `src/core/README.md` 详细更新。

### 阶段 2：data 持久化层 —— 已完成

- 新增 `src/data` 模块，CMake 目标 `ScheduleData`。
- 仓库接口 `IScheduleRepository`，实现 `SqliteScheduleRepository`。
- 表：`semesters`、`time_slots`、`courses`、`course_sessions`、`settings`、`import_sources`。
- 支持数据库迁移、备份、恢复，以及 JSON 序列化 / 反序列化。

### 阶段 3：课表获取、导入与导出子系统 —— 已完成

- 接口：`IScheduleImporter`、`IScheduleExporter`、`ImportManager`、`ExportManager`。
- 导入：JSON / CSV / ICS；导出：JSON / CSV / ICS。
- 默认目录 `QStandardPaths::DocumentsLocation + "/Schedule"`，可在设置中修改。
- 导入支持预览、去重、冲突检测、合并 / 覆盖策略；导出后返回实际路径。

### 阶段 4：engine 桥接与 QML 模型 —— 已完成

- 桥接对象：`ScheduleBridge`、`CourseListModel`、`ImportExportBridge`。
- 暴露课程列表模型、当前周、导入进度 / 结果、导出结果、错误信息。
- C++ 侧显式建立信号连接，QML 不写隐式 `onClicked` / `Connections`。

### 阶段 5：QML 界面 —— 已完成

- 页面：`WeekView`、`DayView`、`CourseCard`、`CourseEditor`、`ImportWizard`、
  `ExportDialog`、`SettingsPage`、`SemesterPage`。
- 布局：`MainDesktop.qml` / `MainMobile.qml`。
- 关键控件添加 `objectName`，供 C++ 侧连接与 UI 自检定位。

### 阶段 6：本地提醒与通知 —— 已完成

- `ReminderScheduler`（计算）+ `NotificationService`（平台通知）。
- 支持提前 5 / 10 / 15 分钟提醒；桌面系统托盘通知，移动端本地通知。
- 处理通知权限与后台任务差异。

### 阶段 7：测试与自检 —— 已完成

- 扩展 QTest：周次解析、冲突检测、导入导出、数据库迁移。
- 增加导入导出样本文件。
- 扩展 `--selftest`：自动导入样本、导出到指定目录、校验导出文件存在。

### 阶段 8：打包、国际化与文档收尾 —— 已完成

- `windeployqt` / `macdeployqt` / `androiddeployqt` 部署说明。
- Qt Linguist 国际化基础（`tr()` / `qsTr()` + `.ts` 文件）。
- 更新根 README 的模块索引、构建命令与整体架构。

### 阶段 9（可选）：教务适配器

- 仅在阶段 0~8 全部完成后按需实现。
- 不做账号系统、不保存明文密码、仅本地主动触发导入。
- 接口在 `core`，实现在 `data`，注册在 `app`。
