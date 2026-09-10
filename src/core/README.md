# core（核心层）

## 职责

存放与界面无关的纯业务 / 数据逻辑：课表领域模型（学期、课程、上课时间段、周次、冲突）、
周次表达式解析、当前周计算、冲突检测与课表服务。**本模块不依赖任何 GUI 技术**
（QWidget / QML），仅使用 `Qt6::Core`，保证核心逻辑可在无显示环境下单元测试与复用。

明确不负责：文件读写、数据库、QML 绑定、界面展示。

## 依赖

| 依赖 | 类型 | 说明 |
| ---- | ---- | ---- |
| `Qt6::Core` | 外部 | `QString`、`QDate`、`QTime`、`QObject`（仅 `ScheduleService`）等 |

- 允许依赖：无（本层是最底层，不依赖任何其他项目模块）
- 禁止依赖：`ScheduleData`、`ScheduleEngine`、`ScheduleUI`、`Schedule`（app）

## 产物

- 目标名：`ScheduleCore`（静态库）
- 公开头文件目录：`include/`（引用方式 `#include "core/model/WeekMask.h"`）

## 目录结构

```text
src/core/
├── CMakeLists.txt
├── README.md
├── include/core/
│   ├── DataEngine.h                  # 版本信息
│   ├── model/                        # 领域模型（纯值类型）
│   │   ├── Conflict.h
│   │   ├── Course.h
│   │   ├── CourseSession.h
│   │   ├── ScheduleSnapshot.h
│   │   ├── Semester.h
│   │   ├── TimeSlot.h
│   │   └── WeekMask.h
│   └── service/                      # 核心服务
│       ├── ConflictDetector.h
│       ├── ReminderScheduler.h
│       ├── ScheduleService.h
│       └── WeekCalculator.h
├── src/                              # 与 include/ 同构的实现文件
└── tests/                            # QTest 单元测试（BUILD_TESTS=ON 时构建）
    ├── CMakeLists.txt
    ├── README.md
    ├── tst_conflict_detector.cpp
    ├── tst_reminder_scheduler.cpp
    ├── tst_schedule_service.cpp
    ├── tst_week_calculator.cpp
    └── tst_week_mask.cpp
```

## 公开接口与关键类型

### 领域模型（值类型，可拷贝 / 可比较）

| 类型 | 头文件 | 说明 |
| ---- | ------ | ---- |
| `WeekMask` | `core/model/WeekMask.h` | 64 位周次掩码；**周次表达式解析**（`1-16`、`1-16/2`、`2/2`、`odd`/`even`、`*`、混合片段）与集合运算（并 / 交 / 差 / 包含）；规范化表达式与人类可读文本输出 |
| `TimeSlot` | `core/model/TimeSlot.h` | 节次（作息表一行）：`index`、`label`、`start_time`、`end_time`；`default_slots()` 提供“上午 4 节 + 下午 4 节 + 晚上 3 节”的默认作息 |
| `CourseSession` | `core/model/CourseSession.h` | 上课时间段：`day_of_week`(1..7)、`start_slot`、`slot_count`、`weeks`；`location` / `teacher` 可覆盖课程级默认值；`conflicts_with()` 判定“同一天 + 节次相交 + 周次相交” |
| `Course` | `core/model/Course.h` | 课程：名称、代码、教师、地点、颜色、学分、备注 + 多个时间段；`total_weeks()` / `occurs_on()` / `display_color()` |
| `Semester` | `core/model/Semester.h` | 学期：名称、起始日期、总周数（1..64）、是否当前学期；**周次按自然周（周一为首日）对齐** |
| `Conflict` | `core/model/Conflict.h` | 冲突 / 校验问题：`Type`（时间冲突、课程内部冲突、重复课程、周次非法、节次越界、缺少时间段、时间段非法）、涉及课程与时间段、星期与周次、中文描述；`is_blocking()` 区分“必须修正”与“仅提示” |
| `ScheduleSnapshot` | `core/model/ScheduleSnapshot.h` | **聚合载体**：`semester` + `time_slots` + `courses`；在 `core` / `data` / `engine` 之间一次性传递完整课表 |

### 核心服务

| 类型 | 头文件 | 说明 |
| ---- | ------ | ---- |
| `WeekCalculator` | `core/service/WeekCalculator.h` | 纯静态工具：日期 ↔ 周次 ↔ 星期换算、当前周、`HH:mm` 宽松时间解析、星期文本解析（中 / 英 / ICS 缩写） |
| `ConflictDetector` | `core/service/ConflictDetector.h` | 纯静态检测器：`detect()` 全量、`detect_in_course()` 单课程自检、`detect_between()` 两课比较；`Options` 可放宽导入场景的校验；结果按“类型 → 星期 → 周次”排序并去重 |
| `ScheduleService` | `core/service/ScheduleService.h` | `QObject` 服务：持有**当前学期**状态；学期 / 作息表设置；课程增删改查；当前周与课表查询（`sessions_at()` / `sessions_on_date()` / `sessions_in_week()`）；冲突检测；快照读写 |
| `ReminderScheduler` | `core/service/ReminderScheduler.h` | 纯静态计算：把课表换算成“什么时候提醒哪门课”；支持 5/10/15 分钟提前量、时间窗口过滤与到点判定 |
| `DataEngine` | `core/DataEngine.h` | 版本信息（早期占位，保留） |

### 统一口径（重要）

- 星期：`1=周一 ... 7=周日`，与 `QDate::dayOfWeek()` 完全一致；
- 周次：`1..Semester::total_weeks`；`0` 表示“不在学期范围内”；
- 第 N 周起始日 = `包含 Semester::start_date 的自然周的周一 + 7*(N-1)`，因此起始日不必是周一；
- 时间：本地时间、无时区语义。

## 构建与测试方式

```bash
# 配置（含单元测试）
cmake --preset windows-msvc -DBUILD_TESTS=ON
# 构建
cmake --build --preset windows-msvc-debug
# 运行测试（VS 生成器为多配置，必须带 -C Debug）
ctest --preset windows-msvc -C Debug
```

- 测试文件位置：`src/core/tests/`，说明见 [`tests/README.md`](tests/README.md)
- 是否可在无 GUI 环境测试：**是**。本层不链接 `Qt6::Gui` / `Qt6::Quick`，
  测试以 `QTEST_GUILESS_MAIN` 启动，可直接在 CI 的无头环境中运行。

## 与上下层交互方式

- 向下：本层为最底层，不调用任何项目内其他模块。
- 向上：
  - `data` 层直接使用本层的领域模型做序列化、落盘与导入导出；
  - `engine` 层通过 `ScheduleBridge` 把 `ScheduleService` 暴露给 QML；
  - `app` 层负责实例化 `ScheduleService` 并组装仓库。

## 信号连接约定

- `ScheduleService` 是唯一的信号源，共 4 个信号：

| 信号 | 触发时机 |
| ---- | -------- |
| `semester_changed()` | 学期元数据（名称 / 起始日期 / 总周数 / 当前学期标记）变化后 |
| `time_slots_changed()` | 作息表被整体替换后（含切换学期导致的清空） |
| `courses_changed()` | 课程列表发生任何增删改后 |
| `error_occurred(message)` | 操作失败时，携带面向用户的中文原因 |

- 本层**不包含任何 QML 类型**，信号连接由上层（engine / app）建立。
- 命名遵循全小写 + 下划线；QML ↔ C++ 的连接**统一在 C++ 侧显式建立**，
  QML 中不写 `onClicked` / `Connections`。

## 提醒计算（阶段 6）

`ReminderScheduler` 只做**纯计算**，不接触定时器与通知 API：

```cpp
const QList<Reminder> due = ReminderScheduler::upcoming(snapshot, QDateTime::currentDateTime(), 10, 24);
```

- `Reminder` 携带课程、时间段、周次、上课时刻、提醒时刻与去重键；
- `upcoming()` 只返回提醒时刻落在 `[now, now + horizon]` 的条目，**过去的提醒不补发**；
- `is_due()` 的到点条件是 `remind_at <= now < start`（上课开始后不再提醒）；
- 支持的提前分钟数只有 5 / 10 / 15，非法值回退为默认 10。

## 扩展点与注意事项

- **新增业务规则**：优先以“纯函数 + 值类型”的形式加入本层（便于单元测试）；
  需要通知上层时再叠加信号，不要引入 Qt GUI / SQL 依赖。
- **周次上限**：`WeekMask` 为 64 位，`Semester::total_weeks` 上限即 64；
  超出会被 `from_expression()` 判为错误、被 `Semester::create()` 夹取。
- **表达式解析是全有或全无**：任一片段非法即整体失败并返回中文错误，
  避免“半截周次”写库；但允许尾随分隔符产生的空片段。
- **`ScheduleService` 只承载当前学期**：切换学期（`id` 变化）会清空课程与作息表，
  防止上学期数据串味；多学期管理由 `data` 层仓库负责。
- **允许冲突**：`add_course()` **不会**因为时间冲突而拒绝写入（学生可能需要先记录再调整），
  冲突通过 `detect_conflicts()` 单独查询后由界面提示。
- **`load_snapshot()` 的兜底**：快照未携带作息表时自动回退为 `TimeSlot::default_slots()`，
  保证周视图始终有节次标题可渲染。

## 后续阶段计划

| 阶段 | 内容 |
| ---- | ---- |
| 阶段 6 | ✅ 已完成：`ReminderScheduler` 与 `Reminder`（纯计算，无通知 API 依赖）；系统通知在 `engine` |
| 阶段 9 | 定义可选教务适配器接口 `SchoolAdapter`（实现在 `data`） |

## 相关文档

- [根 README](../README.md)
- [阶段路线图](../docs/ROADMAP.md)
- [模块 README 模板](../docs/README_TEMPLATE.md)
- [tests/README.md](tests/README.md) — 本层单元测试说明
- [data](../data/README.md) — 依赖本层的持久化与导入导出
- [engine](../engine/README.md) — 依赖本层的 QML 桥接层
- [ui](../ui/README.md) / [app](../app/README.md) — 上层界面与应用入口
