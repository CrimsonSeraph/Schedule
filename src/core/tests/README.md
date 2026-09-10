# core/tests（核心层单元测试）

## 职责

用 QTest 覆盖 `ScheduleCore` 的**纯逻辑**行为：周次表达式解析与集合运算、日期 ↔ 周次换算、
冲突检测、课表服务的增删改查与快照往返。测试只依赖 `Qt6::Core` 与 `Qt6::Test`，
不创建窗口，可在无显示环境下运行。

## 依赖

| 依赖 | 类型 | 说明 |
| ---- | ---- | ---- |
| `ScheduleCore` | 项目内 | 被测目标 |
| `Qt6::Test` | 外部 | `QCOMPARE` / `QVERIFY` / `QSignalSpy` |

## 产物

`BUILD_TESTS=ON` 时生成 4 个测试可执行文件，并注册到 CTest：

| 测试目标 | 覆盖内容 |
| -------- | -------- |
| `tst_week_mask` | 周次表达式解析（区间 / 步长 / `A/S` / 关键字 / 中英文分隔符）、非法输入拒绝、规范化表达式往返、集合运算、`MAX_WEEKS` 边界 |
| `tst_week_calculator` | 学期自检、日期 → 周次、周次 → 日期、当前周、非周一开学的自然周对齐、星期文本解析、时间解析与格式化、上课开始时刻 |
| `tst_conflict_detector` | 时间重叠 / 不重叠（周次、星期、节次边界）、课程内部重叠、缺时间段、节次越界、周次越界、重复课程、结果排序与去重 |
| `tst_schedule_service` | 学期设置与切换清空、课程增删改查与默认值补齐、信号发射（`QSignalSpy`）、按周 / 星期 / 日期查询、单双周过滤、冲突检测、快照往返、默认作息回退 |

## 目录结构

```text
src/core/tests/
├── CMakeLists.txt              # 为每个 .cpp 建立可执行文件并 add_test()
├── README.md
├── tst_conflict_detector.cpp
├── tst_schedule_service.cpp
├── tst_week_calculator.cpp
└── tst_week_mask.cpp
```

## 公开接口与关键类型

本目录不对外提供接口，只提供测试用例。新增测试的方式：

1. 在 `src/core/tests/` 下新建 `tst_xxx.cpp`，使用 `QTEST_GUILESS_MAIN(TestXxx)`；
2. 文件末尾 `#include "tst_xxx.moc"`（`AUTOMOC` 生成）；
3. 把目标名追加到 `CMakeLists.txt` 的 `SCHEDULE_CORE_TESTS` 列表。

## 构建与测试方式

```bash
cmake --preset windows-msvc -DBUILD_TESTS=ON
cmake --build --preset windows-msvc-debug

# Visual Studio 生成器是多配置的，必须显式指定配置
ctest --preset windows-msvc -C Debug

# 只跑单个测试并输出详细断言信息
./build/windows-msvc/Debug/tst_week_mask.exe -v2
```

## 与上下层交互方式

- 向下：直接链接 `ScheduleCore`，使用其公开头文件（`include/` 由 `ScheduleCore` 以
  `PUBLIC` 方式导出，无需额外 `target_include_directories`）。
- 向上：测试结果通过 CTest 汇总，供 `--selftest`（app 层）之外的自动化流程消费。

## 信号连接约定

- 测试中以 `QSignalSpy` 观察 `ScheduleService` 的信号，验证“状态变化 → 信号发射”一一对应。
- 不使用 `QObject::connect` 连接 lambda 做断言（`QSignalSpy` 已足够，且失败信息更清晰）。

## 扩展点与注意事项

- **命名**：`tst_<被测对象>.cpp`，测试类 `Test<被测对象>`，槽函数命名 `snake_case`。
- **测试数据**：统一使用 `2024-09-02`（周一）作为学期起始日，避免“今天”带来的不确定性；
  涉及“当前周”的用例必须显式传入日期参数，禁止依赖 `QDate::currentDate()`。
- **禁止**：测试不得访问真实数据库、真实用户目录或网络；需要落盘时使用
  `QTemporaryDir`。
- **注意**：`slots` 是 Qt 关键字宏（与 `signals` 相同），局部变量不可命名为 `slots`。

## 相关文档

- [../README.md](../README.md) — core 层说明
- [根 README](../../README.md)
- [阶段路线图](../../docs/ROADMAP.md)
