# engine（引擎 / QML 桥接层）

## 职责

把 C++ 侧的服务以 `QObject` 形式暴露给 QML（属性 / 槽 / 信号），是 UI 层与
`core` + `data` 之间的桥梁：持有仓库与服务实例、维护 `QAbstractListModel` 派生模型、
转发导入导出进度与结果、封装本地提醒与系统通知。

明确不负责：业务规则判定（`core`）、文件解析与落盘（`data`）、界面绘制（`ui`）。

## 依赖

| 依赖 | 类型 | 说明 |
| ---- | ---- | ---- |
| `ScheduleCore` | 项目内 | 领域模型与 `ScheduleService` |
| `ScheduleData` | 项目内 | 仓库实现、导入导出管理器（阶段 2/3 起） |
| `Qt6::Core` / `Qt6::Qml` | 外部 | `QObject`、`QAbstractListModel`、元对象系统（需 `AUTOMOC`） |

- 允许依赖：`core`、`data`
- 禁止依赖：`ui`、`app`

## 产物

- 目标名：`ScheduleEngine`（静态库，PUBLIC 链接 ScheduleCore / ScheduleData / Qt6::Core / Qt6::Qml）
- 公开头文件目录：`include/`（引用方式：`#include "engine/AppBridge.h"`）

## 目录结构

```text
src/engine/
├── CMakeLists.txt
├── README.md
├── include/engine/
│   ├── AppBridge.h          # 版本 / 测试用桥接对象
│   └── ...                  # 阶段 4 起：ScheduleBridge、CourseListModel、ImportExportBridge
└── src/
    ├── AppBridge.cpp
    └── ...
```

## 公开接口与关键类型

| 类型 | 说明 |
| ---- | ---- |
| `AppBridge` | 桥接对象；C++ 侧持有并注入 QML；属性 `version`（来自 `DataEngine::get_version()`）、公开槽 `test_button_clicked()`（qDebug 输出测试信息并发出信号）、信号 `test_signal(message)` |

> 阶段 4 将新增 `ScheduleBridge`、`CourseListModel`、`ImportExportBridge`，
> 阶段 6 将新增提醒与通知相关桥接对象，详见下方“后续阶段计划”。

## 构建与测试方式

```bash
cmake --preset windows-msvc
cmake --build --preset windows-msvc-debug
```

- 本模块由根 `CMakeLists.txt` 通过 `add_subdirectory(src/engine)` 引入。
- 桥接层逻辑通过 app 层的 `--selftest` 做端到端验证；纯逻辑测试放在对应下层模块。

## 与上下层交互方式

- 向下：调用 `core` 的服务与 `data` 的仓库 / 导入导出管理器。
- 向上：由 `app` 层实例化并注入 QML 上下文；`ui` 层只读取属性、调用槽函数。

由应用入口（`src/app/main.cpp`）在 C++ 侧实例化 `Schedule::AppBridge`，
并在加载 QML 前注入为上下文属性：

```cpp
Schedule::AppBridge app_bridge;
engine.rootContext()->setContextProperty("bridge", &app_bridge);
```

随后 QML 侧即可（无需 import Schedule）：

```qml
Text { text: bridge.version }
Button {
    id: testButton
    objectName: "testButton"
    text: qsTr("测试")
}
```

## 信号连接约定

- 与桥接对象之间的信号连接**统一在 C++ 侧显式建立**（`QObject::connect`，见 `src/app/main.cpp`）；
  QML 不使用 `onClicked` / `Connections` 等按名称隐式连接的写法；
- 命名遵循全小写 + 下划线（如槽 `test_button_clicked()`、信号 `test_signal(message)`）；
- QML 控件通过 `objectName` 暴露给 C++，由 `app` 层在加载后 `findChild` 并连接；
- 桥接对象对 QML 暴露的状态一律通过 `Q_PROPERTY`（只读属性 + `NOTIFY` 信号）提供，
  避免 QML 直接持有业务对象指针。

## 扩展点与注意事项

- **扩展点**：新增桥接能力时，优先在既有桥接对象上增加 `Q_PROPERTY` / 槽 / 信号，
  而不是在 QML 中新增直接访问 C++ 类的路径；QML 始终只使用 `app` 注入的上下文属性。
- **对象生命周期**：桥接对象由 `app` 持有（栈对象或 `QObject` 父子关系），
  注入 QML 时必须保证其生命周期长于 `QQmlApplicationEngine`。
- **模型更新**：`QAbstractListModel` 的增删改必须成对发出 `beginInsertRows` / `endInsertRows`
  等信号，否则 QML 视图会与数据不一致。
- **平台差异**：系统通知（阶段 6）在桌面与移动端 API 不同，需以平台宏分支封装。

## 后续阶段计划

| 阶段 | 内容 |
| ---- | ---- |
| 阶段 4 | `ScheduleBridge`（学期 / 当前周 / 课程 CRUD）、`CourseListModel`（课程列表模型）、`ImportExportBridge`（导入导出进度与结果） |
| 阶段 6 | `ReminderScheduler` + `NotificationService` 的桥接与系统通知落地 |
| 阶段 9 | 注册可选教务适配器（接口在 `core`，实现在 `data`，注册在 `app`） |

## 相关文档

- [根 README](../README.md)
- [阶段路线图](../docs/ROADMAP.md)
- [模块 README 模板](../docs/README_TEMPLATE.md)
- [core](../core/README.md) — 本层依赖的核心逻辑层
- [data](../data/README.md) — 本层依赖的基础设施层
- [ui](../ui/README.md) / [app](../app/README.md) — 使用本层桥接对象的上层
