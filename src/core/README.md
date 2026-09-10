# core（核心层）

## 职责

存放与界面无关的纯业务 / 数据逻辑：课表领域模型（学期、课程、上课时间段、周次）、
周次表达式解析、当前周计算、冲突检测与课表服务。**本模块不依赖任何 GUI 技术**
（QWidget / QML），仅使用 `Qt6::Core`，保证核心逻辑可在无显示环境下单元测试与复用。

明确不负责：文件读写、数据库、QML 绑定、界面展示。

## 依赖

| 依赖 | 类型 | 说明 |
| ---- | ---- | ---- |
| `Qt6::Core` | 外部 | `QString`、`QDate`、`QTime`、`QJsonObject`、`QObject` 等 |

- 允许依赖：无（本层是最底层，不依赖任何其他项目模块）
- 禁止依赖：`ScheduleData`、`ScheduleEngine`、`ScheduleUI`、`Schedule`（app）

## 产物

- 目标名：`ScheduleCore`（静态库）
- 公开头文件目录：`include/`（引用方式：`#include "core/DataEngine.h"`）

## 目录结构

```text
src/core/
├── CMakeLists.txt
├── README.md
├── include/core/
│   ├── DataEngine.h
│   └── ...           # 领域模型与服务（阶段 1 起）
└── src/
    ├── DataEngine.cpp
    └── ...
```

## 公开接口与关键类型

| 类型 | 头文件 | 说明 |
| ---- | ------ | ---- |
| `DataEngine` | `core/DataEngine.h` | 核心数据引擎；`get_version()` 返回应用版本 `"1.0.0"` |

> 阶段 1 将新增：`Semester`、`Course`、`CourseSession`、`TimeSlot`、`WeekMask`、`Conflict`
> 六个领域类型，以及 `ScheduleService`、`ConflictDetector`、`WeekCalculator` 三个核心服务，
> 详见下方“后续阶段计划”。

## 构建与测试方式

```bash
# 配置（含单元测试）
cmake --preset windows-msvc -DBUILD_TESTS=ON
# 构建
cmake --build --preset windows-msvc-debug
# 运行测试
ctest --preset windows-msvc
```

- 测试文件位置：`src/core/tests/`（阶段 1 起）
- 是否可在无 GUI 环境测试：**是**。本层不链接 `Qt6::Gui` / `Qt6::Quick`，
  单元测试可直接在 CI 的无头环境中运行。

## 与上下层交互方式

- 向下：本层为最底层，不调用任何项目内其他模块。
- 向上：`data` 层直接使用本层的领域模型做序列化与落盘；`engine` 层通过
  `ScheduleBridge` 把 `ScheduleService` 暴露给 QML；`app` 层负责组装。
- 领域模型设计为**值类型**（可拷贝、可比较），服务类设计为 `QObject`，
  以便上层通过信号感知状态变化。

## 信号连接约定

- 本层服务类可定义信号，但**不包含任何 QML 类型**；信号连接由上层（engine / app）建立。
- 命名遵循全小写 + 下划线：槽 `test_button_clicked()`、信号 `test_signal(message)`。
- 桥接对象与 QML 之间的信号连接**统一在 C++ 侧显式建立**（`QObject::connect`）；
  QML 中不写 `onClicked` / `Connections` 等按名称隐式连接的写法。

## 扩展点与注意事项

- **扩展点**：新增业务规则应优先以“纯函数 + 值类型”的形式加入本层，
  便于单元测试；需要通知上层时再叠加信号。
- **边界条件**：周次统一以 1 为起点，单学期周数上限暂定 64（用 64 位掩码表示）；
  日期与周次的换算以“第 1 周周一”为基准，跨月、跨年由 `QDate` 自行处理。
- **平台差异**：本层刻意不引入平台宏，任何平台相关逻辑都应放到 `data` / `engine` / `app`。
- **时间表示**：仅处理“本地时间 + 无时区”的课表语义，不引入时区换算。

## 后续阶段计划

| 阶段 | 内容 |
| ---- | ---- |
| 阶段 1 | 新增 `Semester` / `Course` / `CourseSession` / `TimeSlot` / `WeekMask` / `Conflict` 与 `ScheduleService` / `ConflictDetector` / `WeekCalculator`，并补充 QTest 单元测试 |
| 阶段 6 | 可选：在本层实现 `ReminderScheduler` 的**纯计算**部分（下一次提醒时刻），平台通知放 `engine` |
| 阶段 9 | 定义可选教务适配器接口 `SchoolAdapter`（实现在 `data`） |

## 相关文档

- [根 README](../README.md)
- [阶段路线图](../docs/ROADMAP.md)
- [模块 README 模板](../docs/README_TEMPLATE.md)
- [data](../data/README.md) — 依赖本层的持久化与导入导出
- [engine](../engine/README.md) — 依赖本层的 QML 桥接层
- [ui](../ui/README.md) / [app](../app/README.md) — 上层界面与应用入口
