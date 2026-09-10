# engine（引擎 / QML 桥接层）

## 职责

把 C++ 侧的服务以 QObject 形式暴露给 QML（属性 / 槽 / 信号），是 UI 层与 core 层之间的桥梁。

## 依赖

- `ScheduleCore`（核心层静态库）
- Qt 6.9.3（组件：`Core`、`Qml`，需要 `AUTOMOC` 生成 Q_OBJECT 元数据）

## 产物

- `ScheduleEngine`（静态库，PUBLIC 链接 ScheduleCore / Qt6::Core / Qt6::Qml）
- 公开头文件目录：`include/`（引用方式：`#include "engine/AppBridge.h"`）

## 当前内容

| 类型 | 说明 |
| --- | --- |
| `AppBridge` | 桥接对象；C++ 侧持有并注入 QML；属性 `version`（来自 `DataEngine::get_version()`）、公开槽 `test_button_clicked()`（qDebug 输出测试信息并发出信号）、信号 `test_signal(message)` |

## 注入到 QML

由应用入口（`src/app/main.cpp`）在 C++ 侧实例化 `Schedule::AppBridge`，并在加载 QML 前注入为上下文属性：

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

信号连接约定：

- 与桥接对象之间的信号连接统一在 C++ 侧显式建立（`QObject::connect`，见 `src/app/main.cpp`）；QML 不再使用 `onClicked` / `Connections` 等按名称隐式连接的写法；
- 命名遵循全小写 + 下划线（如槽 `test_button_clicked()`、信号 `test_signal(message)`）。

## 构建

本模块由根 `CMakeLists.txt` 通过 `add_subdirectory(src/engine)` 引入。

## 相关文档

- [根 README](../README.md) — 架构总览与构建指南
- [core](../core/README.md) — 本层依赖的核心逻辑层
- [ui](../ui/README.md) / [app](../app/README.md) — 使用 AppBridge 的上层
