# data/import_export（课表导入导出子系统）

## 职责

把课表在**文件**与**内存模型**之间来回搬运，并把文件落到用户指定的目录：

- 定义导入 / 导出接口（`IScheduleImporter` / `IScheduleExporter`）；
- 编排导入流程（`ImportManager`）：格式识别 → 解析 → **预览** → 冲突检测 → 合并 / 覆盖；
- 编排导出流程（`ExportManager`）：生成文件名 → 创建目录 → 原子写入 → 返回**实际路径**；
- 提供 JSON / CSV / ICS 三种通用实现，以及正方教务系统导出的 HTML 课表导入器。

明确**不负责**：

- 不弹出任何文件选择对话框（属 UI 层职责），本子系统只接收**路径**或 `QUrl`；
- 不做周次 / 冲突的业务规则判定（复用 `core::ConflictDetector`）；
- 不直接写数据库（`ImportManager::apply()` 只修改内存快照，落库由调用方完成）。

## 依赖

| 依赖                  | 类型   | 说明                                                |
| --------------------- | ------ | --------------------------------------------------- |
| `ScheduleCore`        | 项目内 | 领域模型、`ConflictDetector`、`WeekCalculator`      |
| `data/ScheduleJson.h` | 项目内 | JSON 映射的唯一权威                                 |
| `data/AppSettings.h`  | 项目内 | `ensure_directory()` 用于创建导出目录               |
| `Qt6::Core`           | 外部   | `QFile` / `QSaveFile` / `QStringConverter` / 正则等 |

## 产物

本子系统不产生独立 CMake 目标，编译进 `ScheduleData` 静态库。公开头文件位于 `include/data/import_export/`。

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
│   ├── ExportManager.h         # 导出编排
│   └── academic_affairs/       # 各校教务系统专用解析（只导入）
│       ├── ZhengfangTimetableIo.h  # 正方教务（zfn / zfsoft V9）课表页解析
│       ├── DocConvertUtil.h        # 旧版 .doc → .docx 转换（Word / LibreOffice）
│       ├── CharsetUtil.h           # 字符集嗅探与 GBK 解码
│       └── GbkTable.h              # GBK→Unicode 码表声明（实现为生成文件）
└── src/import_export/          # 与 include/ 同构的实现文件
    └── academic_affairs/
        ├── ZhengfangTimetableIo.cpp
        ├── DocConvertUtil.cpp
        ├── CharsetUtil.cpp
        └── GbkTable.cpp            # 由 tools/gen_gbk_table.py 生成，勿手工修改
```

## 公开接口与关键类型

| 类型 | 说明 |
| --- | --- |
| `ScheduleFormat` | 格式枚举：`Json` / `Csv` / `Ics` / `ZhengfangHtml` / `Unknown`；配套 `format_to_string()`、`format_from_extension()`、`format_from_content()` |
| `ImportStrategy` | `Merge`（同 id 更新，其余新增）/ `SkipDuplicates`（重复跳过）/ `Overwrite`（清空后写入） |
| `ImportPreview` | 只读预览：格式、学期、作息表、课程、**新引入的冲突**、重复数、新增数、提示、错误 |
| `ImportResult` | 导入统计：新增 / 更新 / 跳过数量、导入后的冲突、摘要文本 |
| `ExportResult` | 导出结果：**实际文件路径**、字节数、课程数、摘要文本 |
| `IScheduleImporter` | 导入器契约：`format()` / `can_import()` / `parse()` / `parse_data()` |
| `IScheduleExporter` | 导出器契约：`format()` / `extension()` / `serialize()` / `write()` |
| `ImportManager` | 注册表 + 预览 + 应用策略；`register_importer()` 为扩展点 |
| `ExportManager` | 注册表 + 文件名规则 + 目录导出；`suggested_file_name()` / `sanitize_file_component()` |
| `DocConvertUtil` | 旧版 `.doc` → `.docx` 转换；后端可插拔（Word / LibreOffice），失败不静默 |
| `supported_file_extensions()` | **全部可导入**格式的扩展名（含 `.xls`），用于导入侧提示 |
| `export_file_extensions()` | **仅可导出**格式的扩展名（不含 `.xls`），用于文件对话框过滤器 |

> `supported_file_extensions()` 与 `export_file_extensions()` 的分工是必要的：正方教务页面只能导入不能导出，若共用一份列表，导出对话框会给出无法生成的 `.xls` 过滤器。

## `.doc` → `.docx` 转换（`DocConvertUtil`）

教务系统「导出」的课表有两个时代：老的 Word 97-2003 `.doc`，和新的 Word 表格（`.doc` 的
「另存为网页」形态或真正的 `.docx`）。`.doc` 是 OLE 复合文档，没有可用的开源解析路径；
`.docx` 只是一个 zip，其中的 `word/document.xml` 可以直接读。`DocConvertUtil` 负责把前者
变成后者，让解析器只需要面对一种结构。

### 后端

| 机器名 | 平台 | 做法 |
| --- | --- | --- |
| `microsoft-word` | Windows | `QProcess` 起 `cscript`，执行一段 VBScript 调用本机 Word 的 COM 接口 `SaveAs(..., 12)` |
| `libreoffice` | 全平台 | `soffice --headless --convert-to docx --outdir <dir> <file>` |

按 `microsoft-word` → `libreoffice` 的顺序取第一个**可用**的后端；某个后端装了但这次转换失败，
会继续尝试下一个，并把两者失败的原因都累积进最终错误信息。

`available_backend_names()` 给出当前可用的后端中文名，供界面提示。

### 关键取舍

- **Word 后端不用 `QAxObject`**：本仓库使用的 Qt 套件（官方 MinGW / MSVC 安装包）不含 ActiveQt
  （`Qt6::AxContainer`），`QAxObject` 链接不上。改用 `QProcess` + `cscript` 同样走 Word 的 COM 接口，
  却不引入额外 Qt 模块，且与 LibreOffice 后端共用同一套「起进程 → 等退出 → 校验产物」逻辑。
  将来若换用自带 ActiveQt 的套件，通过 `set_backends()` 追加一个 `QAxObject` 版本即可，本类其余部分不用动。
- **`output_dir` 是必填参数**：产物路径必须活得比函数调用久，这件事只能由调用方保证
  （典型做法是调用方自己持有一个 `QTemporaryDir`），因此不提供「默认写到临时目录」的重载。
- **转换后有校验**：两个后端都会用 `is_ooxml_package()` 确认产物真的是 zip 包，
  避免把「改了扩展名的 HTML」当成转换成功。
- **失败不静默**：两个后端都不可用时，错误信息会说明缺什么、以及「装 LibreOffice 或改用 .docx」。

### 后端可插拔

`default_backends()` 返回的是数据（`Backend{name, display_name, available, convert}`）而不是硬编码分支，
`set_backends()` 可以在测试里注入假后端，因此「回退链」「全部不可用」这些分支无需真装
Word / LibreOffice 就能覆盖。**`set_backends()` 仅供测试**，用毕需 `reset_backends()`。

## 四种格式的映射要点

### JSON（无损）

直接复用 `ScheduleJson`：保留颜色、学分、备注、时间段级地点 / 教师覆盖与精确周次位图。文件扩展名 `.json`。

### CSV（Excel 友好）

一行 = 一个上课时间段，首行为表头；列名支持中英文别名、顺序无关：

```text
课程名称,课程代码,教师,地点,星期,开始节次,节次数量,周次,学分,备注,开始时间,结束时间
高等数学,MATH101,张老师,教一 101,周一,1,2,1-16,4,需带教材,08:00,09:40
```

- 必需列：课程名称、星期、开始节次、周次；
- 导出为 **UTF-8 with BOM**（Excel 双击可正确识别中文）；
- 导入时若非 UTF-8（如 GBK），会给出“请另存为 UTF-8”的明确提示——Qt 6 默认不再内置 GBK 编解码器，CSV 路径有意不做猜测（见下方「编码策略」）；
- CSV 不含学期信息，导入时按内容合成一个不早于 20 周的学期，由 `ImportManager` 在存在“当前学期”时替换为真实学期。

### 正方教务课表（只导入）

教务系统「导出 / 打印」出的课表文件通常命名为 `课表.xls`，但内容**不是** Excel 二进制，而是 Excel 兼容的 HTML 页面：外层是可打印表格（`<table id="manualArrangeCourseTable">`），内层 `<script>` 是页面数据源。识别标志串为 `manualArrangeCourseTable` / `courseTableForStd`，或同时出现 `CourseTable(` 与 `TaskActivity(`。

解析以 `<script>` 中的 `TaskActivity` 为准（只有它带逐周位图），关键约定：

| 约定 | 说明 |
| --- | --- |
| 单元格下标 | `index = 星期 * unitCount + 节次`；**星期从 0 开始**（0=周一），**节次从 0 开始** |
| 周次位图 | 第 i 个字符为 `1` 表示第 i 周有课（下标 0 不使用，第 1 周对应下标 1） |
| 连续节次 | 一个活动可挂在多个 `index` 上：同一天且节次连续→合并为一段，否则拆成多段 |
| 教师名 | 不在 `TaskActivity` 参数里，取自紧邻其上的 `actTeachers = [{name:"…"}]` |
| 课程代码 | 形如 `19626(04311190.02)`，取括号内的 `04311190.02`，括号外为教学班序号 |
| 聚合 | 同一课程代码的多个活动合并为一门课程的多段时间段 |
| 学期 | 取页面 `<h3>` 标题（如 `2025-2026学年第二学期`）；起始日期页面未提供，先按本周一合成，再由 `ImportManager` 换成当前学期 |
| 作息表 | 页面不含上课时间，按 `unitCount` 生成 1..N 节，已有默认作息时间的节次直接带入 |
| 条件字段 | 课程级教师 / 地点取第一个时间段的值，与之相同的时间段不再重复存储（留空表示沿用课程级值） |

`.xls` / `.html` / `.htm` 三种扩展名都会映射到本格式。**该格式只导入**：本应用不会生成教务页面，因此它出现在 `supported_file_extensions()` 而不在 `export_file_extensions()` 中。

> 样本与回归测试见 [`samples/schedule_sample_zhengfang.xls`](../../../samples/README.md) 与 [`../tests/tst_zhengfang_timetable.cpp`](../tests/tst_zhengfang_timetable.cpp)。样本是**虚构数据**，由 `tools/gen_zhengfang_sample.py` 生成，结构与真实导出页一致。

### ICS（iCalendar）

一门课的每个时间段对应一个 `VEVENT`：

| 课表字段               | ICS 属性                                                                         |
| ---------------------- | -------------------------------------------------------------------------------- |
| 上课起止时刻           | `DTSTART` / `DTEND`（本地浮动时间）                                              |
| 重复周次               | `RRULE`（等差周次）或 `RDATE`（非等差，如 `1-4,6,9-10`）                         |
| 课程名称 / 地点 / 教师 | `SUMMARY` / `LOCATION` / `DESCRIPTION`                                           |
| 精确节次与周次         | `X-SCHEDULE-SLOT`、`X-SCHEDULE-SLOT-COUNT`、`X-SCHEDULE-DAY`、`X-SCHEDULE-WEEKS` |

扩展属性让本应用导出的 ICS 能**无损往返**；来自其它日历应用的 ICS 则回退到“按 `DTSTART` 推导星期与节次、按 `RRULE` / `RDATE` 推导周次”，并用出现过的上课时间合成作息表。输出严格使用 `CRLF` 并按 RFC 5545 的 75 字节规则折行（不会切断多字节字符）。

## 指定目录约定（硬性要求）

- 默认目录：`QStandardPaths::DocumentsLocation + "/Schedule"` （见 `AppSettings::fallback_directory()`）；用户可在设置页修改，修改后写入 `settings` 表的 `io/default_export_dir` / `io/default_import_dir`。
- 导出文件名：`Schedule_<学期>_<yyyyMMdd_HHmmss>.<ext>` （学期名为空时退化为 `Schedule_<时间戳>.<ext>`；学期名会经过 `sanitize_file_component()` 清洗）。
- 导出目录不存在时由 `ExportManager` 自动创建（`QDir::mkpath`）。
- 导出完成后，`ExportResult::file_path` 是**实际写入的绝对路径**，界面必须原样提示用户。
- 导入时从指定目录选择文件：**文件对话框在 UI 层**，数据层只接收路径。

## 编码策略

Qt 6 的 `QStringConverter` 只覆盖 UTF / Latin-1 / System，**不含 GBK**；`Qt6Core5Compat` （提供 `QTextCodec`）也不在本项目的依赖清单里。而正方教务系统的导出页一律声明 `<meta charset="GBK">`，课程名 / 教师名 / 教室名都是 GBK 汉字，不解码就无法使用。

因此 `academic_affairs/CharsetUtil.*` 自带一条最小解码链路：

- `GbkTable.cpp` 是由 `tools/gen_gbk_table.py` 生成的 23940 项 GBK→Unicode（BMP）码表，重新生成用 `python3 tools/gen_gbk_table.py`，校验用 `--check`；
- `gbk_to_unicode()` 处理 ASCII 与双字节序列，非法 / 未定义码位映射为 `U+FFFD`，不抛错；
- `sniff_declared_charset()` 读取 `<meta charset=…>`；
- `decode_html_bytes()` 按「声明优先、内容兜底」决策：声明 UTF 系列时先校验字节是否真是合法 UTF-8（部分教务系统声称 UTF-8 实际输出 GBK），不合法则退回 GBK；未声明时合法 UTF-8 按 UTF-8，否则按 GBK。

支持范围是 ASCII + GBK 双字节区（覆盖 GB2312 / GBK 常用汉字）；GB18030 的四字节扩展区不在范围内。该解码链路**只服务于教务页面导入**，CSV / ICS 仍严格要求 UTF-8：通用格式不做编码猜测，才能避免把乱码静默写进课表。

## 构建与测试方式

```bash
cmake --preset windows-msvc -DBUILD_TESTS=ON
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc -C Debug
```

测试实现：

- [`../tests/tst_doc_convert.cpp`](../tests/tst_doc_convert.cpp) — `.doc` → `.docx` 的后端优先级与回退链、入参校验、无后端时的可执行提示、OOXML 识别；
- [`../tests/tst_import_export.cpp`](../tests/tst_import_export.cpp) — 格式识别、文件名规则、三种通用格式往返、预览与冲突、三种合并策略、错误路径与内存导入；
- [`../tests/tst_zhengfang_timetable.cpp`](../tests/tst_zhengfang_timetable.cpp) — GBK 解码、格式嗅探、连续节次合并、不连续节次拆分、同课程多活动聚合、周次位图还原、导出过滤器不含 `.xls`，以及经 `ImportManager` 的完整预览流程。

## 与上下层交互方式

- 向下：使用 `core` 的模型与 `ConflictDetector`、`data` 的 `ScheduleJson` / `AppSettings`。
- 向上：被 `engine` 直接调用（由 `ImportExportBridge` 暴露给 QML）。典型调用链：

```cpp
// 1) 预览（UI 层提供文件路径）
ImportPreview preview = import_manager.preview(file_path, current_snapshot);
// 2) 用户确认后应用
ImportResult result = import_manager.apply(preview, ImportStrategy::Merge, &snapshot);
// 3) 落库（由 engine / app 层负责）
repository->save_snapshot(snapshot, &error);
```

教务适配器与内嵌浏览器抓取到的是**已解析或已抓到的原文**，因此走另外两个入口，但复用同一套冲突检测与合并逻辑：

```cpp
// 适配器已解析成快照
ImportPreview preview = import_manager.preview_snapshot(parsed, format, adapter_name, current);
// 网页抓取到的页面原文（字节），按内容嗅探格式
ImportPreview preview = import_manager.preview_data(page_html, "browser://课表", current);
```

## 信号连接约定

本子系统不产生 QObject 信号（纯同步 API，便于单元测试）。进度 / 结果信号由 `engine` 层的 `ImportExportBridge` 包装后以全小写 + 下划线的命名暴露给 QML（如 `import_finished(bool, QString)`）。

## 扩展点与注意事项

- **新增格式**：实现 `IScheduleImporter` / `IScheduleExporter`，在 `ImportManager` / `ExportManager` 构造函数中 `register_*`，并在 `ImportExportTypes.cpp` 的 `FORMATS` 表补一行（机器名、显示名、扩展名、`exportable`）。`exportable = false` 表示只导入格式，它不会进入导出过滤器。
- **新增学校**：若能拿到结构化数据，优先让教务系统导出通用格式；若只能拿到页面，在 `include/data/import_export/academic_affairs/` 下新增导入器，与 `ZhengfangTimetableIo` 同级。
- **非文件来源**（剪贴板、分享码、教务适配器、网页抓取）：不必新增抽象——`preview_data()` 负责原始字节， `preview_snapshot()` 负责已解析快照。
- **预览的冲突语义**：预览报告的是**本次导入新引入的冲突** （导入后冲突集合 − 导入前冲突集合），避免把历史问题重复报给用户。
- **覆盖策略**：`Overwrite` 会清空当前课程后写入文件内容；作息表仅在文件提供时才被替换。
- **重复判定**：有 id 时按 id；无 id（CSV / ICS / 正方教务）时按“课程名称 + 课程代码”。
- **编码**：CSV 支持 UTF-8（含 BOM）；ICS 按 RFC 必须为 UTF-8；正方教务页面由 `CharsetUtil` 嗅探并支持 GBK。详见上文「编码策略」。
- **安全**：所有写入都使用 `QSaveFile` 原子替换；文件名经过清洗，不会突破用户选择的目录。
- **隐私**：导入过程只读文件，**不访问网络、不读取任何凭证**；正方导入器只取课程 / 教师 / 教室 / 周次 / 节次，**不保存学号、姓名、班级**等页面上的个人信息。

## 相关文档

- [../README.md](../README.md) — data 层总览
- [../../core/README.md](../../core/README.md) — `ConflictDetector` / `WeekCalculator`
- [根 README](../../../README.md)
- [阶段路线图](../../../docs/ROADMAP.md)
