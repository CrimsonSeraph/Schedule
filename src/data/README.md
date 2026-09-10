# data（基础设施层）

> **当前状态：占位（阶段 0）**。本模块在**阶段 2** 落地实现，阶段 3 在其上扩展导入导出子系统。

## 职责

`data` 层是课表应用的**基础设施层**，负责把 `core` 层的纯业务模型落到真实介质上：

- **持久化**：SQLite 数据库（建表、迁移、备份、恢复）与 JSON 序列化 / 反序列化；
- **导入导出**：JSON / CSV / ICS 三种格式的解析与生成，落盘到用户指定目录；
- **可选教务适配器**（阶段 9，按需）：仅本地主动触发的课表抓取。

明确**不负责**：

- 不包含任何界面代码（QML / QWidget）。文件选择对话框一律由 UI 层提供，
  本层只接收文件**路径**（`QString`）或 `QUrl`。
- 不做冲突检测 / 周次计算等业务规则判定——这些属于 `core`；本层只负责“搬运与落盘”。
- 不做云同步、账号系统、OAuth、用户表、同步队列、后端 API。

## 依赖

| 依赖 | 类型 | 说明 |
| ---- | ---- | ---- |
| `ScheduleCore` | 项目内 | 直接读写 `Semester` / `Course` / `CourseSession` / `TimeSlot` 等领域模型 |
| `Qt6::Core` | 外部 | `QJson*`、`QFile`、`QSaveFile`、`QStandardPaths`、`QUuid` 等 |
| `Qt6::Sql` | 外部 | `QSqlDatabase` + QSQLITE 驱动 |

- 允许依赖：`core`
- 禁止依赖：`engine`、`ui`、`app`（反向依赖会导致分层破坏，构建期不可见但运行期难维护）

## 产物

- 目标名：`ScheduleData`（静态库）
- 公开头文件目录：`include/`（引用方式 `#include "data/IScheduleRepository.h"`）
- 数据库默认位置：`QStandardPaths::AppDataLocation` 下的 `schedule.db`
- 默认导入 / 导出目录：`QStandardPaths::DocumentsLocation + "/Schedule"`

## 目录结构（规划）

```text
src/data/
├── CMakeLists.txt
├── README.md
├── include/data/
│   ├── IScheduleRepository.h        # 仓库接口
│   ├── SqliteScheduleRepository.h   # SQLite 实现
│   ├── ScheduleJson.h               # 领域模型 <-> JSON 映射
│   └── import_export/
│       ├── IScheduleImporter.h
│       ├── IScheduleExporter.h
│       ├── ImportManager.h
│       ├── ExportManager.h
│       └── ...（JSON / CSV / ICS 具体实现）
└── src/
    └── ...
```

## 公开接口与关键类型（规划）

| 类型 | 头文件 | 说明 |
| ---- | ------ | ---- |
| `IScheduleRepository` | `data/IScheduleRepository.h` | 学期 / 节次 / 课程 / 设置 / 导入来源的读写接口 |
| `SqliteScheduleRepository` | `data/SqliteScheduleRepository.h` | SQLite 实现，含 schema 迁移与备份恢复 |
| `IScheduleImporter` | `data/import_export/IScheduleImporter.h` | 导入器接口（格式识别 + 解析为内存模型） |
| `IScheduleExporter` | `data/import_export/IScheduleExporter.h` | 导出器接口（序列化并写入指定路径） |
| `ImportManager` | `data/import_export/ImportManager.h` | 编排导入：识别格式 → 预览 → 冲突检测 → 合并/覆盖 |
| `ExportManager` | `data/import_export/ExportManager.h` | 编排导出：生成文件名 → 写入指定目录 → 回传实际路径 |

## 构建与测试方式

```bash
cmake --preset windows-msvc -DBUILD_TESTS=ON
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc
```

- 测试文件位置：`src/data/tests/`（阶段 2 起）
- 是否可在无 GUI 环境测试：**是**。SQLite 使用内存库（`:memory:`）或临时目录文件，
  不依赖显示环境，适合 CI。

## 与上下层交互方式

- 向下：使用 `core` 的领域模型与 `ScheduleService` 做规则校验（如导入后冲突检测）。
- 向上：被 `engine` 直接调用；`engine` 只依赖 `IScheduleRepository` / `ImportManager` /
  `ExportManager` 等抽象，便于替换实现与注入测试替身。
- 由 `app` 决定具体实现与数据库路径，并注入 `engine`。

## 信号连接约定

- 本层为**基础设施层**，原则上不产生面向 QML 的信号；进度 / 结果由 `engine` 层包装后暴露。
- 若实现内部需要回调，使用 `std::function` 或普通虚函数，不引入 QML 相关类型。
- 一旦向 `engine` 暴露进度信号，将被 `engine` 转发为全小写 + 下划线的槽 / 信号命名
  （例如信号 `import_progress_changed(int)`）。

## 扩展点与注意事项

- **新增导入 / 导出格式**：实现 `IScheduleImporter` / `IScheduleExporter` 并注册到
  `ImportManager` / `ExportManager` 的格式表即可，无需改动上层。
- **数据库迁移**：所有结构变更必须通过 `user_version` 递增的迁移脚本完成，
  禁止直接 `ALTER` 后不回填版本号；迁移前自动备份到 `<db>.bak`。
- **文件写入安全**：导出统一使用 `QSaveFile` 原子写入，避免中途失败留下半个文件。
- **编码差异**：CSV 可能由 Excel 生成（GBK / UTF-8 BOM），ICS 必须为 UTF-8；
  解析时需探测 BOM 并处理换行符 `CRLF`。
- **路径安全**：默认目录可能不存在，写入前需 `QDir::mkpath`；路径拼接统一用
  `QDir::filePath` 而非字符串拼接。
- **禁止事项**：不得在此层引入账号、密码、Token 等持久化字段。

## 相关文档

- [根 README](../README.md)
- [阶段路线图](../docs/ROADMAP.md)
- [模块 README 模板](../docs/README_TEMPLATE.md)
- [core](../core/README.md) — 本层依赖的领域模型
- [engine](../engine/README.md) — 本层的主要调用方
