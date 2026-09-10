# data（基础设施层）

## 职责

`data` 层是课表应用的**基础设施层**，负责把 `core` 层的纯业务模型落到真实介质上：

- **持久化**：SQLite 数据库（建表、迁移、备份、恢复）与 JSON 序列化 / 反序列化；
- **导入导出**（阶段 3 起）：JSON / CSV / ICS 三种格式的解析与生成，落盘到用户指定目录；
- **设置存储**：默认导入 / 导出目录、提醒开关、主题等键值配置；
- **可选教务适配器**（阶段 9，按需）：仅本地主动触发的课表抓取。

明确**不负责**：

- 不包含任何界面代码（QML / QWidget）。文件选择对话框一律由 UI 层提供，
  本层只接收文件**路径**（`QString`）或 `QUrl`。
- 不做冲突检测 / 周次计算等业务规则判定——这些属于 `core`；本层只负责“搬运与落盘”。
- 不做云同步、账号系统、OAuth、用户表、同步队列、后端 API。
  `import_sources` 表只记录**本地**文件路径与导入时间，不含任何凭证。

## 依赖

| 依赖 | 类型 | 说明 |
| ---- | ---- | ---- |
| `ScheduleCore` | 项目内 | 直接读写 `Semester` / `Course` / `CourseSession` / `TimeSlot` / `ScheduleSnapshot` |
| `Qt6::Core` | 外部 | `QJson*`、`QFile`、`QSaveFile`、`QStandardPaths`、`QUuid` 等 |
| `Qt6::Sql` | 外部 | `QSqlDatabase` + QSQLITE 驱动 |

- 允许依赖：`core`
- 禁止依赖：`engine`、`ui`、`app`

## 产物

- 目标名：`ScheduleData`（静态库）
- 公开头文件目录：`include/`（引用方式 `#include "data/SqliteScheduleRepository.h"`）
- 数据库默认位置：`QStandardPaths::AppDataLocation/schedule.db`（`SqliteScheduleRepository::default_database_path()`）
- 默认导入 / 导出目录：`QStandardPaths::DocumentsLocation + "/Schedule"`（`AppSettings::fallback_directory()`）

## 目录结构

```text
src/data/
├── CMakeLists.txt
├── README.md
├── include/data/
│   ├── AppSettings.h                 # 设置门面（类型安全访问器）
│   ├── IScheduleRepository.h         # 仓库接口
│   ├── ImportSource.h                # 导入来源记录
│   ├── ScheduleJson.h                # 领域模型 <-> JSON 映射（唯一权威）
│   ├── SettingsKeys.h                # 设置键名
│   └── SqliteScheduleRepository.h    # SQLite 实现
├── src/                              # 与 include/ 同构的实现文件
└── tests/                            # QTest 单元测试（BUILD_TESTS=ON 时构建）
    ├── CMakeLists.txt
    ├── README.md
    ├── tst_schedule_json.cpp
    └── tst_sqlite_repository.cpp
```

## 公开接口与关键类型

| 类型 | 头文件 | 说明 |
| ---- | ------ | ---- |
| `IScheduleRepository` | `data/IScheduleRepository.h` | 持久化能力契约：生命周期、学期 CRUD、快照读写、设置、导入来源、备份 / 恢复。上层只依赖本接口 |
| `SqliteScheduleRepository` | `data/SqliteScheduleRepository.h` | SQLite 实现：`user_version` 迁移、事务化快照写入、`VACUUM INTO` 备份、恢复前自动留 `.bak` |
| `ScheduleJson` | `data/ScheduleJson.h` | 领域模型 ↔ JSON 的**唯一**映射；`to_document()` / `from_document()` / `write_file()` / `read_file()` |
| `AppSettings` | `data/AppSettings.h` | 设置门面：默认 / 最近目录、提醒开关与分钟数、主题、当前学期 id；`fallback_directory()` / `ensure_directory()` |
| `SettingsKeys` | `data/SettingsKeys.h` | 设置键名常量（`io/default_export_dir` 等），避免硬编码漂移 |
| `ImportSource` | `data/ImportSource.h` | 导入来源留痕：文件路径、格式、时间、课程数、备注（**纯本地**） |

### 数据库表结构（`user_version = 1`）

| 表 | 关键列 | 说明 |
| -- | ------ | ---- |
| `semesters` | `id`(PK), `name`, `start_date`, `total_weeks`, `is_current`, `created_at`, `updated_at` | 学期元数据；`is_current` 由 `set_current_semester()` 保证全局唯一 |
| `time_slots` | `(semester_id, slot_index)`(PK), `label`, `start_time`, `end_time` | 学期作息表；随学期级联删除 |
| `courses` | `id`(PK), `semester_id`(FK), `name`, `code`, `teacher`, `location`, `color`, `credits`, `notes` | 课程 |
| `course_sessions` | `id`(PK), `course_id`(FK), `day_of_week`, `start_slot`, `slot_count`, `week_bits`, `week_expression`, `location`, `teacher` | 上课时间段 |
| `settings` | `setting_key`(PK), `setting_value` | 键值配置 |
| `import_sources` | `id`(PK), `file_path`, `format`, `imported_at`, `course_count`, `note` | 本地导入留痕 |

外键 `ON DELETE CASCADE` 生效的前提是每条连接都执行 `PRAGMA foreign_keys = ON`，
`open()` 已统一处理。

### JSON 文档结构

```json
{
    "format": "schedule",
    "version": 1,
    "exported_at": "2024-09-10T12:00:00",
    "semester": { "id": "...", "name": "...", "start_date": "2024-09-02", "total_weeks": 16 },
    "time_slots": [{ "index": 1, "label": "第 1 节", "start": "08:00", "end": "08:45" }],
    "courses": [
        {
            "id": "...",
            "name": "高等数学",
            "sessions": [
                { "day_of_week": 1, "start_slot": 1, "slot_count": 2, "weeks": "1-16", "week_bits": "65535" }
            ]
        }
    ]
}
```

兼容策略：`week_bits` 为权威位图，缺失时回退解析 `weeks` 表达式；
未知字段忽略、缺失字段取默认值，便于向后兼容与手工编辑。

## 构建与测试方式

```bash
cmake --preset windows-msvc -DBUILD_TESTS=ON
cmake --build --preset windows-msvc-debug

# Visual Studio 生成器为多配置，必须带 -C Debug
ctest --preset windows-msvc -C Debug
```

- 测试文件位置：`src/data/tests/`，说明见 [`tests/README.md`](tests/README.md)
- 是否可在无 GUI 环境测试：**是**。测试使用 `:memory:` 内存库与 `QTemporaryDir`，
  不触碰真实用户数据，适合 CI。

## 与上下层交互方式

- 向下：使用 `core` 的领域模型与 `ScheduleSnapshot` 做序列化与落盘。
- 向上：被 `engine` 直接调用；`engine` 与 `app` 只依赖 `IScheduleRepository` 抽象，
  具体实现（SQLite / 测试替身）由 `app` 注入。
- `app` 负责选择数据库路径并调用 `open()`；`data` 层不关心应用启动流程。

## 信号连接约定

- 本层为基础设施层，**不产生面向 QML 的信号**，也不包含任何 QML 类型。
- 所有错误通过返回 `false` + `QString* error_message` 输出中文原因；
  进度 / 结果信号由 `engine` 层包装后暴露给 QML。
- `engine` 暴露相关信号时遵循全小写 + 下划线命名（如 `import_progress_changed(int)`）。

## 扩展点与注意事项

- **新增存储实现**：实现 `IScheduleRepository` 并在 `app` 层注入即可，上层无需改动。
- **数据库迁移**：**只追加、不修改**已发布的 `migration_steps()`；
  每个版本一个事务，失败整体回滚；版本高于实现支持上限时拒绝打开并提示升级应用。
- **事务嵌套**：`save_snapshot()` → `save_semester()` → `set_current_semester()` 存在嵌套调用，
  因此统一使用 `begin_transaction()` / `commit_transaction()` / `rollback_transaction()`，
  由最外层负责真正提交，避免 SQLite 报 "cannot start a transaction within a transaction"。
- **null QString 陷阱**：Qt 会把 `QString()`（null）绑定为 SQL `NULL`，
  而本 schema 的 TEXT 列都是 `NOT NULL DEFAULT ''`。所有字符串绑定必须经过内部
  `non_null()` 归一化，否则报 "NOT NULL constraint failed"。
- **文件写入安全**：导出统一使用 `QSaveFile` 原子写入。
- **备份 / 恢复**：备份使用 SQLite 的 `VACUUM INTO`（无需关闭连接且保证一致性）；
  恢复会覆盖当前数据，覆盖前自动留一份 `<db>.bak`，并清理 WAL 附属文件
  （`-wal` / `-shm`），避免新旧文件不匹配。
- **路径安全**：默认目录可能不存在，写入前统一 `QDir::mkpath`；
  路径拼接使用 `QDir::filePath` 而非字符串拼接。
- **禁止事项**：不得在此层引入账号、密码、Token 等持久化字段。

## 后续阶段计划

| 阶段 | 内容 |
| ---- | ---- |
| 阶段 3 | 新增 `import_export/` 子目录：`IScheduleImporter` / `IScheduleExporter` / `ImportManager` / `ExportManager` 与 JSON / CSV / ICS 实现 |
| 阶段 7 | 补充数据库迁移与导入导出样本测试 |
| 阶段 9 | 可选教务适配器：接口在 `core`，实现在本层，注册在 `app` |

## 相关文档

- [根 README](../README.md)
- [阶段路线图](../docs/ROADMAP.md)
- [模块 README 模板](../docs/README_TEMPLATE.md)
- [tests/README.md](tests/README.md) — 本层测试说明
- [core](../core/README.md) — 本层依赖的领域模型
- [engine](../engine/README.md) — 本层的主要调用方
