# data/import_export（课表导入导出子系统）

## 职责

把课表在**文件**与**内存模型**之间来回搬运，并把文件落到用户指定的目录：

- 定义导入 / 导出接口（`IScheduleImporter` / `IScheduleExporter`）；
- 编排导入流程（`ImportManager`）：格式识别 → 解析 → **预览** → 冲突检测 → 合并 / 覆盖；
- 编排导出流程（`ExportManager`）：生成文件名 → 创建目录 → 原子写入 → 返回**实际路径**；
- 提供 JSON / CSV / ICS 三种内置实现。

明确**不负责**：

- 不弹出任何文件选择对话框（属 UI 层职责），本子系统只接收**路径**或 `QUrl`；
- 不做周次 / 冲突的业务规则判定（复用 `core::ConflictDetector`）；
- 不直接写数据库（`ImportManager::apply()` 只修改内存快照，落库由调用方完成）。

## 依赖

| 依赖 | 类型 | 说明 |
| ---- | ---- | ---- |
| `ScheduleCore` | 项目内 | 领域模型、`ConflictDetector`、`WeekCalculator` |
| `data/ScheduleJson.h` | 项目内 | JSON 映射的唯一权威 |
| `data/AppSettings.h` | 项目内 | `ensure_directory()` 用于创建导出目录 |
| `Qt6::Core` | 外部 | `QFile` / `QSaveFile` / `QStringConverter` / 正则等 |

## 产物

本子系统不产生独立 CMake 目标，编译进 `ScheduleData` 静态库。
公开头文件位于 `include/data/import_export/`。

## 目录结构

```text
src/data/
├── include/data/import_export/
│   ├── ImportExportTypes.h     # 格式枚举、导入策略、预览 / 结果结构
│   ├── IScheduleImporter.h     # 导入器接口
│   ├── IScheduleExporter.h     # 导出器接口
│   ├── JsonScheduleIo.h        # JSON 实现（无损）
│   ├── CsvScheduleIo.h         # CSV 实现（Excel 友好）
│   ├── IcsScheduleIo.h         # iCalendar 实现（可与系统日历互操作）
│   ├── ImportManager.h         # 导入编排
│   └── ExportManager.h         # 导出编排
└── src/import_export/          # 与 include/ 同构的实现文件
```

## 公开接口与关键类型

| 类型 | 说明 |
| ---- | ---- |
| `ScheduleFormat` | 格式枚举：`Json` / `Csv` / `Ics` / `Unknown`；配套 `format_to_string()`、`format_from_extension()`、`format_from_content()` |
| `ImportStrategy` | `Merge`（同 id 更新，其余新增）/ `SkipDuplicates`（重复跳过）/ `Overwrite`（清空后写入） |
| `ImportPreview` | 只读预览：格式、学期、作息表、课程、**新引入的冲突**、重复数、新增数、提示、错误 |
| `ImportResult` | 导入统计：新增 / 更新 / 跳过数量、导入后的冲突、摘要文本 |
| `ExportResult` | 导出结果：**实际文件路径**、字节数、课程数、摘要文本 |
| `IScheduleImporter` | 导入器契约：`format()` / `can_import()` / `parse()` / `parse_data()` |
| `IScheduleExporter` | 导出器契约：`format()` / `extension()` / `serialize()` / `write()` |
| `ImportManager` | 注册表 + 预览 + 应用策略；`register_importer()` 为扩展点 |
| `ExportManager` | 注册表 + 文件名规则 + 目录导出；`suggested_file_name()` / `sanitize_file_component()` |

## 三种格式的映射要点

### JSON（无损）

直接复用 `ScheduleJson`：保留颜色、学分、备注、时间段级地点 / 教师覆盖与精确周次位图。
文件扩展名 `.json`。

### CSV（Excel 友好）

一行 = 一个上课时间段，首行为表头；列名支持中英文别名、顺序无关：

```text
课程名称,课程代码,教师,地点,星期,开始节次,节次数量,周次,学分,备注,开始时间,结束时间
高等数学,MATH101,张老师,教一 101,周一,1,2,1-16,4,需带教材,08:00,09:40
```

- 必需列：课程名称、星期、开始节次、周次；
- 导出为 **UTF-8 with BOM**（Excel 双击可正确识别中文）；
- 导入时若非 UTF-8（如 GBK），会给出“请另存为 UTF-8”的明确提示——
  Qt 6 默认不再内置 GBK 编解码器；
- CSV 不含学期信息，导入时按内容合成一个不早于 20 周的学期，
  由 `ImportManager` 在存在“当前学期”时替换为真实学期。

### ICS（iCalendar）

一门课的每个时间段对应一个 `VEVENT`：

| 课表字段 | ICS 属性 |
| -------- | -------- |
| 上课起止时刻 | `DTSTART` / `DTEND`（本地浮动时间） |
| 重复周次 | `RRULE`（等差周次）或 `RDATE`（非等差，如 `1-4,6,9-10`） |
| 课程名称 / 地点 / 教师 | `SUMMARY` / `LOCATION` / `DESCRIPTION` |
| 精确节次与周次 | `X-SCHEDULE-SLOT`、`X-SCHEDULE-SLOT-COUNT`、`X-SCHEDULE-DAY`、`X-SCHEDULE-WEEKS` |

扩展属性让本应用导出的 ICS 能**无损往返**；来自其它日历应用的 ICS 则回退到
“按 `DTSTART` 推导星期与节次、按 `RRULE` / `RDATE` 推导周次”，并用出现过的上课时间
合成作息表。输出严格使用 `CRLF` 并按 RFC 5545 的 75 字节规则折行（不会切断多字节字符）。

## 指定目录约定（硬性要求）

- 默认目录：`QStandardPaths::DocumentsLocation + "/Schedule"`
  （见 `AppSettings::fallback_directory()`）；用户可在设置页修改，
  修改后写入 `settings` 表的 `io/default_export_dir` / `io/default_import_dir`。
- 导出文件名：`Schedule_<学期>_<yyyyMMdd_HHmmss>.<ext>`
  （学期名为空时退化为 `Schedule_<时间戳>.<ext>`；学期名会经过 `sanitize_file_component()` 清洗）。
- 导出目录不存在时由 `ExportManager` 自动创建（`QDir::mkpath`）。
- 导出完成后，`ExportResult::file_path` 是**实际写入的绝对路径**，界面必须原样提示用户。
- 导入时从指定目录选择文件：**文件对话框在 UI 层**，数据层只接收路径。

## 构建与测试方式

```bash
cmake --preset windows-msvc -DBUILD_TESTS=ON
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc -C Debug
```

测试实现位于 [`../tests/tst_import_export.cpp`](../tests/tst_import_export.cpp)，
覆盖格式识别、文件名规则、三种格式往返、预览与冲突、三种合并策略、错误路径与内存导入。

## 与上下层交互方式

- 向下：使用 `core` 的模型与 `ConflictDetector`、`data` 的 `ScheduleJson` / `AppSettings`。
- 向上：被 `engine` 直接调用（阶段 4 起由 `ImportExportBridge` 暴露给 QML）。
  典型调用链：

```cpp
// 1) 预览（UI 层提供文件路径）
ImportPreview preview = import_manager.preview(file_path, current_snapshot);
// 2) 用户确认后应用
ImportResult result = import_manager.apply(preview, ImportStrategy::Merge, &snapshot);
// 3) 落库（由 engine / app 层负责）
repository->save_snapshot(snapshot, &error);
```

## 信号连接约定

本子系统不产生 QObject 信号（纯同步 API，便于单元测试）。
进度 / 结果信号由 `engine` 层的 `ImportExportBridge` 包装后以
全小写 + 下划线的命名暴露给 QML（如 `import_finished(bool, QString)`）。

## 扩展点与注意事项

- **新增格式**：实现 `IScheduleImporter` / `IScheduleExporter`，在
  `ImportManager` / `ExportManager` 构造函数中 `register_*`，并在
  `ImportExportTypes.cpp` 的 `FORMATS` 表补一行即可（扩展名、显示名、机器名）。
- **非文件来源**（剪贴板、分享码、教务适配器）：实现 `IScheduleImporter` 并通过
  `ImportManager::preview_data(data, "clipboard://", current)` 调用，无需新增抽象。
- **预览的冲突语义**：预览报告的是**本次导入新引入的冲突**
  （导入后冲突集合 − 导入前冲突集合），避免把历史问题重复报给用户。
- **覆盖策略**：`Overwrite` 会清空当前课程后写入文件内容；作息表仅在文件提供时才被替换。
- **重复判定**：有 id 时按 id；无 id（CSV / ICS）时按“课程名称 + 课程代码”。
- **编码**：CSV 支持 UTF-8（含 BOM）；ICS 按 RFC 必须为 UTF-8；
  非 UTF-8 输入会明确报错而不是静默产生乱码。
- **安全**：所有写入都使用 `QSaveFile` 原子替换；文件名经过清洗，
  不会突破用户选择的目录。

## 相关文档

- [../README.md](../README.md) — data 层总览
- [../../core/README.md](../../core/README.md) — `ConflictDetector` / `WeekCalculator`
- [根 README](../../../README.md)
- [阶段路线图](../../../docs/ROADMAP.md)
