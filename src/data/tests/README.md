# data/tests（持久化层单元测试）

## 职责

用 QTest 覆盖 `ScheduleData` 的行为：JSON 映射的正确性与兼容性、SQLite 建表迁移、
学期 / 快照 / 设置 / 导入记录的读写、备份与恢复、以及设置门面 `AppSettings` 的默认值规则。

**不触碰真实用户数据**：数据库测试一律使用 `:memory:` 或 `QTemporaryDir` 下的临时文件。

## 依赖

| 依赖 | 类型 | 说明 |
| ---- | ---- | ---- |
| `ScheduleData` | 项目内 | 被测目标 |
| `Qt6::Test` | 外部 | `QCOMPARE` / `QVERIFY` |

## 产物

`BUILD_TESTS=ON` 时生成 3 个测试可执行文件，并注册到 CTest：

| 测试目标 | 覆盖内容 |
| -------- | -------- |
| `tst_import_export` | 格式识别（扩展名 / 内容嗅探）、导出文件名规则与清洗、导出到指定目录（自动建目录、返回实际路径）、JSON / CSV / ICS 三种格式往返、ICS 的 `RRULE` 与 `RDATE`、预览的重复统计与提示、**只报告新引入的冲突**、合并 / 去重 / 覆盖三种策略、错误路径（文件不存在、内容无法识别、非 UTF-8、缺列）、内存导入入口 |
| `tst_schedule_json` | 快照 → 文档 → 快照往返、文档信封字段、拒绝非课表 JSON / 过高版本、`week_bits` 缺失时回退解析表达式、`start_time`/`end_time` 兼容命名、缺省作息表回退、学期缺日期报错、`write_file` 自动建目录与原子覆盖、文件不存在报错 |
| `tst_sqlite_repository` | 建表迁移与幂等、快照往返与“整体替换”语义、学期 upsert / 排序 / 级联删除、当前学期唯一性、设置读写、导入记录倒序与字段自动补齐、`VACUUM INTO` 备份与恢复（含 `.bak`）、备份文件校验、未打开时的优雅失败、默认数据库路径、`AppSettings` 门面与默认目录规则 |

## 目录结构

```text
src/data/tests/
├── CMakeLists.txt
├── README.md
├── tst_import_export.cpp
├── tst_schedule_json.cpp
└── tst_sqlite_repository.cpp
```

## 公开接口与关键类型

本目录不对外提供接口。新增测试的方式：

1. 新建 `tst_xxx.cpp`，使用 `QTEST_GUILESS_MAIN(TestXxx)`，末尾 `#include "tst_xxx.moc"`；
2. 把目标名追加到 `CMakeLists.txt` 的 `SCHEDULE_DATA_TESTS` 列表；
3. 测试会自动获得 Qt 运行库部署（`schedule_deploy_qt_runtime`），无需手工复制 DLL。

## 构建与测试方式

```bash
cmake --preset windows-msvc -DBUILD_TESTS=ON
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc -C Debug

# 只跑单个测试
./build/windows-msvc/Debug/tst_sqlite_repository.exe -v2
```

> Windows 上测试可执行文件需要 Qt6Sql.dll 与 `sqldrivers/qsqlite.dll`，
> 由 `cmake/ScheduleQtDeploy.cmake` 在构建后自动部署。

## 与上下层交互方式

- 向下：链接 `ScheduleData`（其以 `PUBLIC` 方式导出 `ScheduleCore` 与 `include/`），
  因此测试可直接包含 `core/...` 与 `data/...` 头文件。
- 向上：结果经 CTest 汇总，与其他模块测试统一在阶段 7 的测试报告中呈现。

## 信号连接约定

本层测试不涉及 QML 信号；`ScheduleData` 本身也不产生 QObject 信号，
错误一律通过 `QString* error_message` 返回，测试直接断言该字符串内容。

## 扩展点与注意事项

- **临时目录**：所有落盘测试必须使用 `QTemporaryDir`；禁止写入
  `QStandardPaths` 返回的真实用户目录（`default_database_path()` 只校验返回值形态，不实际写入）。
- **内存库**：需要覆盖 `open()` / `migration` 之外的逻辑时优先用 `:memory:`，速度更快。
- **中文错误信息**：断言错误信息时只匹配稳定的关键字（如“尚未打开”“版本”“起始日期”），
  避免措辞微调导致测试脆弱。
- **DLL 部署**：新增测试目标时必须调用 `schedule_deploy_qt_runtime()`，
  否则在 Windows 上会出现 `0xc0000135`（STATUS_DLL_NOT_FOUND）。

## 相关文档

- [../README.md](../README.md) — data 层说明
- [根 README](../../README.md)
- [阶段路线图](../../docs/ROADMAP.md)
