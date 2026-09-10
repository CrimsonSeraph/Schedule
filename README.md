# Schedule

基于 **Qt 6.9 / C++20 / CMake** 的跨平台（Windows · Linux · macOS · Android · iOS）课表应用。

## 架构总览

项目采用自底向上的四层架构（**core → engine → ui → app**），层间只能依赖下层：

| 层     | 目录         | 产物                       | 依赖                        |
| ------ | ------------ | -------------------------- | --------------------------- |
| core   | `src/core`   | `ScheduleCore`（静态库）   | Qt6::Core                   |
| engine | `src/engine` | `ScheduleEngine`（静态库） | ScheduleCore, Qt6::Core/Qml |
| ui     | `src/ui`     | `ScheduleUI`（QML 模块）   | ScheduleEngine              |
| app    | `src/app`    | `Schedule`（可执行）       | ScheduleUI, ScheduleEngine  |

## 目录结构

```text
Schedule/
├── CMakeLists.txt          # 根构建脚本
├── CMakePresets.json       # 共享预设（基础预设：Qt / windows-desktop-msvc / android-arm64-v8a）
├── CMakeUserPresets.json   # 本机预设（windows-msvc / android，含 Qt 套件路径；已被 .gitignore 忽略）
├── cmake/                  # CMake 辅助文件目录（当前仅占位 .gitkeep）
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
- Android 交叉编译：NDK + Ninja；工具链文件直接取自 `$env:ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake`，无需在 `cmake/` 另行提供

## 构建命令（CMakePresets）

```bash
# Windows 桌面（MSVC, Visual Studio 2026）
$env:QTDIR = "D:/Qt/6.9.3/msvc2022_64"       # 按实际安装路径设置，基础预设以此填充 CMAKE_PREFIX_PATH
cmake --preset windows-msvc
cmake --build --preset windows-msvc-debug

# Android（arm64-v8a, Ninja）
$env:QTDIR = "D:/Qt/6.9.3/android_arm64_v8a"
$env:ANDROID_NDK_HOME = "C:/Users/<用户>/AppData/Local/Android/Sdk/ndk/30.0.16138531"
$env:ANDROID_SDK_ROOT = "C:/Users/<用户>/AppData/Local/Android/Sdk"
cmake --preset android
cmake --build build/android
```

> `windows-msvc` / `android` 属于**本机预设**，定义在已被 `.gitignore` 忽略的 `CMakeUserPresets.json` 中；新克隆的仓库需自行创建该文件，继承 `CMakePresets.json` 里的基础预设并填入本机 Qt / NDK 路径。

### 构建选项

- `BUILD_TESTS`：构建单元测试（默认 `OFF`）。
- `BUILD_SELFTEST`：桌面自动化 UI 自检（默认 `OFF`）；启用后运行 `Schedule.exe --selftest` 会自动点击“测试”按钮并输出 `Test button clicked!`，用于验证 C++ 侧显式建立的按钮信号连接链路。

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
$env:QTDIR = "D:/Qt/6.9.3/msvc2022_64"   # Windows PowerShell；Linux/macOS 用 export
cmake --preset windows-msvc

# 2) 构建
cmake --build --preset windows-msvc-debug

# 3) 运行（桌面：加载 MainDesktop.qml）
./build/windows-msvc/Debug/Schedule.exe
```

自动化自检（可选，需以 `-DBUILD_SELFTEST=ON` 配置）：

```bash
cmake --preset windows-msvc -DBUILD_SELFTEST=ON
cmake --build --preset windows-msvc-debug
./build/windows-msvc/Debug/Schedule.exe --selftest   # 输出 Test button clicked! 后退出
```

移动端（Android/iOS）在源码层通过 `Q_OS_ANDROID/Q_OS_IOS` 自动加载 `MainMobile.qml`；本地打包部署需另行配置 Qt for Android/iOS 工具链，本仓库暂不包含 CI 配置。

## 模块文档

- [src/core/README.md](src/core/README.md) — 核心层：`ScheduleCore` + `DataEngine`，纯 C++ 无 GUI
- [src/engine/README.md](src/engine/README.md) — 引擎层：`ScheduleEngine` + `AppBridge`，C++/QML 桥接
- [src/ui/README.md](src/ui/README.md) — UI 层：`ScheduleUI` QML 模块（URI: `Schedule`），桌面/移动布局
- [src/app/README.md](src/app/README.md) — 应用入口：`Schedule` 可执行程序与启动流程
