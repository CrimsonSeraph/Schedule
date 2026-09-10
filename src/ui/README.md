# ui（QML 界面层）

## 职责

提供全部界面（QML）与静态资源，按目标平台/形态区分桌面与移动布局，通过 `AppBridge` 与 C++ 侧交互。

## 依赖

- `ScheduleEngine`（桥接层，提供 `AppBridge`）
- Qt 6.9.3（组件：`Quick`、`Qml`；界面使用 Quick Controls 2 / Layouts）

## 产物

- `ScheduleUI`（静态库 + QML 模块，URI: `Schedule`，版本 1.0）
- QML 文件以资源方式内嵌，运行时路径前缀为 `qrc:/qt/qml/Schedule/`

## 组件结构

```text
src/ui/
├── CMakeLists.txt
├── README.md
├── qml/
│   ├── MainDesktop.qml   # 桌面/平板宽屏布局（960x640）
│   └── MainMobile.qml    # 手机竖屏布局（480x800）
└── resources/
    └── assets.qrc        # 静态资源清单（占位）
```

两个主界面均：

- 显示版本标签 `bridge.version`（`bridge` 为 C++ 侧注入的上下文属性，见 app 层说明）；
- 提供文本为 “测试” 的 `Button`（`objectName: "testButton"`），其 `clicked` 信号由 C++ 侧显式连接到 `AppBridge::test_button_clicked()`（触发 qDebug 输出与 `test_signal`）；QML 中不再书写 `onClicked` / `Connections` 等隐式连接。

## 使用

由 app 入口根据平台加载对应 QML：

```cpp
// 桌面：qrc:/qt/qml/Schedule/qml/MainDesktop.qml
// 移动：qrc:/qt/qml/Schedule/qml/MainMobile.qml
```

## 构建

本模块由根 `CMakeLists.txt` 通过 `add_subdirectory(src/ui)` 引入。

## 相关文档

- [根 README](../README.md) — 架构总览与构建指南
- [engine](../engine/README.md) — 提供 AppBridge 的桥接层
- [core](../core/README.md) — 底层核心逻辑
- [app](../app/README.md) — 加载本模块 QML 的应用入口
