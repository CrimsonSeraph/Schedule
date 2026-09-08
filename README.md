# Schedule

基于 **Qt 6.9 / C++20 / CMake** 的跨平台（Windows · Linux · macOS · Android · iOS）课表应用。

> 仓库工程名（CMake target 前缀）为 `MyQtApp`，应用展示名为 Schedule。四层模块已全部启用并可本地构建运行。

## 架构总览

项目采用自底向上的四层架构（**core → engine → ui → app**），层间只能依赖下层：

```text
┌───────────────────────────────────────────┐
│  app   可执行入口 main.cpp，平台分流加载 QML  │
├───────────────────────────────────────────┤
│  ui    QML 模块 MyUI（URI: MyApp），界面与资源 │
├───────────────────────────────────────────┤
│  engine C++/QML 桥接层（AppBridge），Qt6::Qml │
├───────────────────────────────────────────┤
│  core   纯 C++ 业务逻辑（DataEngine），无 GUI  │
└───────────────────────────────────────────┘
```

| 层     | 目录         | 产物                 | 依赖                  |
| ------ | ------------ | -------------------- | --------------------- |
| core   | `src/core`   | `MyCore`（静态库）   | Qt6::Core             |
| engine | `src/engine` | `MyEngine`（静态库） | MyCore, Qt6::Core/Qml |
| ui     | `src/ui`     | `MyUI`（QML 模块）   | MyEngine              |
| app    | `src/app`    | `MyApp`（可执行）    | MyUI, MyEngine        |

## 目录结构

```text
Schedule/
├── CMakeLists.txt          # 根构建脚本
├── CMakePresets.json       # CMake 预设（windows-msvc / android）
├── cmake/                  # 工具链 / CMake 辅助文件（android 工具链占位）
├── src/
│   ├── core/               # 核心层
│   ├── engine/             # 引擎/桥接层
│   ├── ui/                 # QML 界面层
│   └── app/                # 应用入口
└── README.md
```

## 环境要求

- CMake ≥ 3.20
- 支持 C++20 的编译器（MSVC / Visual Studio 2026 / GCC 11+ / Clang 14+）
- Qt 6.9.3（含 `Core`、`Qml` 组件；构建 UI 还需 `Quick`、`QuickControls2`）
- Android 交叉编译：NDK + Ninja，并需在 `cmake/` 提供工具链文件（见下）

## 构建命令（CMakePresets）

```bash
# Windows 桌面（MSVC, Visual Studio 2026）
$env:QT_ROOT = "D:/Qt/6.9.3/msvc2022_64"      # 按实际安装路径设置
cmake --preset windows-msvc
cmake --build --preset windows-msvc-debug

# Android（占位预设：需先补全 cmake/android.toolchain.cmake）
# cmake --preset android
# cmake --build build/android
```

### 构建选项

- `BUILD_TESTS`：构建单元测试（默认 `OFF`）。
- `BUILD_SELFTEST`：桌面自动化 UI 自检（默认 `OFF`）；启用后运行 `MyApp.exe --selftest` 会自动点击“测试”按钮并输出 `Test button clicked!`，用于验证 QML→C++ 调用链路。

### 格式与规范

项目根目录自带组织编码规范（`.clang-format`、`.editorconfig`、`.gitattributes`），C++ 源码提交前执行：

```bash
clang-format -i src/core/src/*.cpp src/core/include/**/*.h \
    src/engine/src/*.cpp src/engine/include/**/*.h \
    src/app/*.cpp
```

## 开发约定

- C++ 源码遵循组织编码规范（.clang-format），每阶段提交前执行格式化。
- 提交信息使用中文，格式：`feat(scope): 简要描述` + 变更列表。

## 运行方法

```bash
# 1) 配置（首次）
$env:QT_ROOT = "D:/Qt/6.9.3/msvc2022_64"   # Windows PowerShell；Linux/macOS 用 export
cmake --preset windows-msvc

# 2) 构建
cmake --build --preset windows-msvc-debug

# 3) 运行（桌面：加载 MainDesktop.qml）
./build/windows-msvc/Debug/MyApp.exe
```

自动化自检（可选，需以 `-DBUILD_SELFTEST=ON` 配置）：

```bash
cmake --preset windows-msvc -DBUILD_SELFTEST=ON
cmake --build --preset windows-msvc-debug
./build/windows-msvc/Debug/MyApp.exe --selftest   # 输出 Test button clicked! 后退出
```

移动端（Android/iOS）在源码层通过 `Q_OS_ANDROID/Q_OS_IOS` 自动加载 `MainMobile.qml`；本地打包部署需另行配置 Qt for Android/iOS 工具链，本仓库暂不包含 CI 配置。

## 模块文档

- [src/core/README.md](src/core/README.md) — 核心层：`MyCore` + `DataEngine`，纯 C++ 无 GUI
- [src/engine/README.md](src/engine/README.md) — 引擎层：`MyEngine` + `AppBridge`，C++/QML 桥接
- [src/ui/README.md](src/ui/README.md) — UI 层：`MyUI` QML 模块（MyApp），桌面/移动布局
- [src/app/README.md](src/app/README.md) — 应用入口：`MyApp` 可执行程序与启动流程
