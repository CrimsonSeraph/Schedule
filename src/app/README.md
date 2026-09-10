# app（应用入口）

## 职责

组装各层产物，创建 Qt 应用并启动 QML 界面。

## 依赖

- `ScheduleUI`（QML 模块，提供 MainDesktop.qml / MainMobile.qml）
- `ScheduleEngine`（桥接层，提供 AppBridge 类型）
- Qt 6.9.3（组件：`Core`、`Qml`、`Quick`）

## 产物

- `Schedule`（可执行程序）

## 启动流程

1. 创建 `QGuiApplication`，设置组织名 / 应用名 / 版本；
2. 在 C++ 侧实例化 `Schedule::AppBridge`，并注入为 QML 上下文属性 `bridge`；
3. 创建 `QQmlApplicationEngine`；
4. 按平台宏选择主 QML 并加载：
    - `Q_OS_ANDROID` / `Q_OS_IOS` → `qrc:/qt/qml/Schedule/qml/MainMobile.qml`
    - 其他平台（Windows / Linux / macOS）→ `qrc:/qt/qml/Schedule/qml/MainDesktop.qml`
5. 加载成功后在 C++ 侧显式建立信号连接（QML 不再隐式连接）：
    - “测试”按钮 `clicked` → `AppBridge::test_button_clicked()`；
    - `AppBridge::test_signal(message)` → 应用日志输出；
6. 加载失败（rootObjects 为空）返回 -1，否则进入事件循环。

## 本地运行验证

```bash
$env:QTDIR = "D:/Qt/6.9.3/msvc2022_64"
cmake --preset windows-msvc
cmake --build --preset windows-msvc-debug
./build/windows-msvc/Debug/Schedule.exe
```

自动化自检：以 `-DBUILD_SELFTEST=ON` 配置后运行 `Schedule.exe --selftest`，程序会自动点击“测试”按钮并退出。点击界面中的“测试”按钮，控制台 / 调试输出应出现：

```text
Test button clicked!
[app] test_signal received: test_button_clicked
```

## 构建

本模块由根 `CMakeLists.txt` 通过 `add_subdirectory(src/app)` 引入。

## 相关文档

- [根 README](../README.md) — 架构总览与构建指南
- [ui](../ui/README.md) — 本程序加载的 QML 模块
- [engine](../engine/README.md) / [core](../core/README.md) — 下层依赖
