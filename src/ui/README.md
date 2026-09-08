# ui（QML 界面层）

## 职责

提供全部界面（QML）与静态资源，按目标平台/形态区分桌面与移动布局，通过 `AppBridge` 与 C++ 侧交互。

## 依赖

- `MyEngine`（桥接层，提供 `AppBridge`）
- Qt 6.9.3（组件：`Quick`、`Qml`；界面使用 Quick Controls 2 / Layouts）

## 产物

- `MyUI`（静态库 + QML 模块，URI: `MyApp`，版本 1.0）
- QML 文件以资源方式内嵌，运行时路径前缀为 `qrc:/qt/qml/MyApp/`

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

- `import MyApp 1.0` 使用注册到 QML 的类型；
- 实例化 `AppBridge` 并显示 `bridge.version` 标签；
- 提供文本为 “测试” 的 `Button`，`onClicked` 调用 `bridge.testButtonClicked()`（触发 C++ 侧 qDebug 输出与 `testSignal`）。

## 使用

由 app 入口根据平台加载对应 QML：

```cpp
// 桌面：qrc:/qt/qml/MyApp/qml/MainDesktop.qml
// 移动：qrc:/qt/qml/MyApp/qml/MainMobile.qml
```

## 构建

本模块由根 `CMakeLists.txt` 通过 `add_subdirectory(src/ui)` 引入。

## 相关文档

- [根 README](../README.md) — 架构总览与构建指南
- [engine](../engine/README.md) — 提供 AppBridge 的桥接层
- [core](../core/README.md) — 底层核心逻辑
- [app](../app/README.md) — 加载本模块 QML 的应用入口
