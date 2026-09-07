# Schedule

基于 **Qt 6.9 / C++20 / CMake** 的跨平台（Windows · Linux · macOS · Android · iOS）课表应用。

> 仓库工程名（CMake target 前缀）为 `MyQtApp`，应用展示名为 Schedule。当前处于分阶段搭建状态，各子模块按阶段逐步启用。

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

| 层 | 目录 | 产物 | 依赖 |
|----|------|------|------|
| core | `src/core` | `MyCore`（静态库） | Qt6::Core |
| engine | `src/engine` | `MyEngine`（静态库） | MyCore, Qt6::Core/Qml |
| ui | `src/ui` | `MyUI`（QML 模块） | MyEngine |
| app | `src/app` | `MyApp`（可执行） | MyUI, MyEngine |

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
- 支持 C++20 的编译器（MSVC 2022 / GCC 11+ / Clang 14+）
- Qt 6.9.3（含 `Core`、`Qml` 组件；构建 UI 还需 `Quick`、`QuickControls2`）
- Android 交叉编译：NDK + Ninja，并需在 `cmake/` 提供工具链文件（见下）

## 构建命令（CMakePresets）

```bash
# Windows 桌面（MSVC, Visual Studio 17 2022）
$env:QT_ROOT = "C:/Qt/6.9.3/msvc2022_64"     # 按实际安装路径设置
cmake --preset windows-msvc
cmake --build build/windows-msvc --config Debug

# Android（占位预设：需先补全 cmake/android.toolchain.cmake）
# cmake --preset android
# cmake --build build/android
```

> 当前各子模块尚未启用（`add_subdirectory` 处于注释状态），启用后将按阶段逐个加入。

## 开发约定

- C++ 源码遵循组织编码规范（.clang-format），每阶段提交前执行格式化。
- 提交信息使用中文，格式：`feat(scope): 简要描述` + 变更列表。
