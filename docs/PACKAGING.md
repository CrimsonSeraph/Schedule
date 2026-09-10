# 打包与发布

本文档说明如何把 `Schedule` 打包成各平台可分发产物，并说明各平台**需要额外注意**的事项。

> 本项目**不做云同步、账号系统与后端服务**；打包产物是纯本地应用，联网权限不是必需的。
> Android 的 `POST_NOTIFICATIONS` 是唯一与“通知”相关的权限。

## 目录

- [0. 通用准备](#0-通用准备)
- [1. Windows](#1-windows)
- [2. Linux](#2-linux)
- [3. macOS](#3-macos)
- [4. Android](#4-android)
- [5. iOS](#5-ios)
- [6. 构建选项速查](#6-构建选项速查)
- [7. 数据与目录](#7-数据与目录)

---

## 0. 通用准备

```bash
cmake --preset <preset> -DCMAKE_BUILD_TYPE=Release
cmake --build --preset <preset>-release        # Visual Studio 生成器请用 --config Release
ctest --preset <preset> -C Release             # 可选：先跑一遍测试
```

产物依赖关系（自底向上）：`ScheduleCore` → `ScheduleData` → `ScheduleEngine` → `ScheduleUI` → `Schedule`。
打包时只需分发**最终可执行文件 + Qt 运行库 + QML 模块**，静态库不会出现在产物里。

---

## 1. Windows

### 1.1 自动部署（已内置）

`src/app/CMakeLists.txt` 在构建后自动调用 `windeployqt`（见 `cmake/ScheduleQtDeploy.cmake`），
把 Qt 运行库、平台插件与 QML 模块复制到 `Schedule.exe` 同级目录：

```powershell
$env:QTDIR = "D:/Qt/6.9.3/msvc2022_64"
cmake --preset windows-msvc -DCMAKE_BUILD_TYPE=Release
cmake --build --preset windows-msvc-release
# 产物目录：build/windows-msvc/Release
```

关键参数说明：

| 参数 | 作用 |
| ---- | ---- |
| `--release` / `--debug` | 按当前配置部署对应的 Qt DLL（Debug 版 DLL 带 `d` 后缀） |
| `--qmldir <src/ui/qml>` | 扫描 QML 文件，确保 `QtQuick`、`QtQuick.Controls`、`QtQuick.Dialogs` 插件被复制 |
| `--compiler-runtime` | 一并复制 MSVC 运行时（`vcruntime140.dll` 等） |
| `--no-translations` | 不复制 Qt 自带的 qt_*.qm（应用自带 `:/i18n` 翻译资源） |

### 1.2 手工打包检查清单

- [ ] `Schedule.exe`
- [ ] `Qt6Core.dll` / `Qt6Gui.dll` / `Qt6Qml.dll` / `Qt6Quick.dll` / `Qt6Widgets.dll` / `Qt6Sql.dll`
- [ ] `platforms/qwindows.dll`
- [ ] `sqldrivers/qsqlite.dll`（**缺失会导致课表无法保存**）
- [ ] `QtQuick/`、`QtQuick/Controls/`、`QtQuick/Dialogs/`、`QtQuick/Layouts/`、`QtQuick/Window/`
- [ ] `samples/`（可选；仅自检与示例导入需要）

### 1.3 独立安装包

- **Inno Setup / NSIS**：把上述目录整体打包，安装后创建开始菜单快捷方式；
- 应用数据默认位于 `%APPDATA%\Schedule\Schedule\schedule.db`，卸载时**不要**删除，
  以免用户课表丢失。

---

## 2. Linux

Qt 官方不提供 Linux 部署工具，常用两种方式：

### 2.1 linuxdeploy（推荐，产出 AppImage）

```bash
cmake -S . -B build/linux -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/opt/Qt/6.9.3/gcc_64
cmake --build build/linux -j

mkdir -p AppDir/usr/bin
cp build/linux/src/app/Schedule AppDir/usr/bin/

wget https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
wget https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage
chmod +x linuxdeploy*.AppImage

./linuxdeploy-x86_64.AppImage --appdir AppDir \
    --plugin qt \
    --executable AppDir/usr/bin/Schedule \
    --desktop-file packaging/schedule.desktop \
    --icon-file packaging/schedule.png \
    --output appimage
```

### 2.2 发行版包

打包 `.deb` / `.rpm` 时把 `Schedule` 安装到 `/usr/bin`，
Qt 运行库交给发行版的 `qt6-base` / `qt6-declarative` 依赖即可。

> **注意**：Linux 上的系统托盘通知依赖桌面环境提供托盘（GNOME 需要扩展）。
> 托盘不可用时应用会自动回退为**应用内横幅**，提醒不会丢失。

---

## 3. macOS

```bash
cmake -S . -B build/macos -G Xcode \
    -DCMAKE_PREFIX_PATH=$HOME/Qt/6.9.3/macos \
    -DCMAKE_BUILD_TYPE=Release
cmake --build build/macos --config Release

# 生成 .app 后部署 Qt 框架
$HOME/Qt/6.9.3/macos/bin/macdeployqt \
    build/macos/src/app/Release/Schedule.app \
    -qmldir=src/ui/qml \
    -appstore-compliant
```

- `-qmldir` 与 Windows 同理，用于扫描 QML 依赖；
- 若要分发到 App Store，需要配置签名与 `Info.plist`（本项目未附带）；
- 本地运行需允许“未签名开发者”：`xattr -dr com.apple.quarantine Schedule.app`。

---

## 4. Android

### 4.1 环境

```bash
export QTDIR=/opt/Qt/6.9.3/android_arm64_v8a       # Qt 的 Android 套件
export QT_HOST_PATH=/opt/Qt/6.9.3/gcc_64           # 同版本桌面套件，提供宿主工具（Windows: D:/Qt/6.9.3/msvc2022_64）
export ANDROID_NDK_ROOT=$HOME/Android/Sdk/ndk/30.0.16138531
export ANDROID_NDK_HOME=$ANDROID_NDK_ROOT
export ANDROID_SDK_ROOT=$HOME/Android/Sdk

cmake --preset android -DCMAKE_BUILD_TYPE=Release   # 预设已指向 Qt 的 qt.toolchain.cmake
cmake --build build/android --target apk            # 产出 APK
```

`CMakePresets.json` 中的 `android-arm64-v8a` 预设已经设置好
`CMAKE_TOOLCHAIN_FILE`（**Qt 自带的 `lib/cmake/Qt6/qt.toolchain.cmake`**）、
`QT_HOST_PATH`、`ANDROID_ABI=arm64-v8a`、`ANDROID_PLATFORM=android-24`。

> **必须用 Qt 的工具链文件，而不是 NDK 自带的 `android.toolchain.cmake`。**
> 后者虽然能交叉编译，但不会注册 Qt 的 Android 打包链路（`apk` / `aab` 目标、
> `androiddeployqt`、`*-deployment-settings.json`），`cmake --build` 结束时只能得到一个
> `.so`，打不出 APK。相应地，`src/app` 必须用 `qt_add_executable`，且顶层
> `find_package(Qt6 ... COMPONENTS ...)` 要显式包含 `Gui`
> （否则报 `No target Qt6::QAndroidIntegrationPlugin`）。

### 4.2 生成 APK

```bash
cmake --build build/android --target apk     # APK
cmake --build build/android --target aab     # 可选：AAB（Google Play 上传用）
```

产物位置：

| 内容 | 路径 |
| ---- | ---- |
| 部署设置 | `build/android/src/app/android-Schedule-deployment-settings.json` |
| 最终 APK | `build/android/src/app/android-build/Schedule.apk` |
| Gradle 工程 | `build/android/src/app/android-build/` |

`apk` 目标内部的顺序是：编译 `libSchedule_<abi>.so` → `androiddeployqt` → Gradle 打包。
首次执行需要联网下载 Gradle 与 Android Gradle Plugin（CI 中缓存 `~/.gradle`）。

未设置 `QT_ANDROID_PACKAGE_SOURCE_DIR` 时 Qt 使用自带默认模板
（`$QTDIR/src/android/templates`），可以正常出包。若要声明 `POST_NOTIFICATIONS`
等清单项，需要自建 `android/AndroidManifest.xml` 并通过
`-DQT_ANDROID_PACKAGE_SOURCE_DIR=...` 指向该目录：

```xml
<uses-permission android:name="android.permission.POST_NOTIFICATIONS" />
<application android:label="Schedule" ...>
```

也可以绕开 CMake 直接调用（部署设置文件仍需先由 `apk` 目标生成）：

```bash
$QT_HOST_PATH/bin/androiddeployqt \
    --input build/android/src/app/android-Schedule-deployment-settings.json \
    --output build/android/src/app/android-build \
    --apk build/android/src/app/android-build/Schedule.apk --release
```

### 4.3 移动端注意事项

- **通知权限**：Android 13+ 需要 `POST_NOTIFICATIONS` 运行时权限；
  应用通过 `engine::AndroidNotificationBackend` 检查并在设置页提供“申请通知权限”按钮；
- **后台冻结**：应用退到后台后 `QTimer` 可能不再准时，
  `NotificationService` 会在应用回到前台时通过 `QEvent::ApplicationStateChange` 补检查一次；
- **布局**：`MainMobile.qml` 会自动加载（`Q_OS_ANDROID` / `Q_OS_IOS` 宏）；
- **数据目录**：数据库位于应用私有目录，卸载应用会一并删除；
  **请引导用户使用“导出”功能备份课表**。

---

## 5. iOS

- 使用 Xcode 生成器配合 Qt for iOS 套件；
- 本地通知需要 `UNUserNotificationCenter`，本项目**未实现 iOS 通知后端**
  （`INotificationBackend` 已预留接口，回退为应用内横幅）；
- 课表数据保存在应用沙盒 `Documents` 之外，卸载即删除，同样建议先用导出功能备份。

---

## 6. 构建选项速查

| 选项 | 默认 | 说明 |
| ---- | ---- | ---- |
| `BUILD_TESTS` | `OFF` | 构建 QTest 单元测试（9 个测试目标）并启用 CTest |
| `BUILD_SELFTEST` | `OFF` | 构建 `--selftest` 端到端自检（仅桌面，供本地/CI 验证） |

发布的正式包应当**同时关闭**这两个选项，避免把测试代码与自检逻辑带进产物。

---

## 7. 数据与目录

| 内容 | Windows | Linux | macOS | Android / iOS |
| ---- | ------- | ----- | ----- | ------------- |
| 数据库 | `%APPDATA%\Schedule\Schedule\schedule.db` | `~/.local/share/Schedule/Schedule/schedule.db` | `~/Library/Application Support/...` | 应用私有目录 |
| 默认导入/导出目录 | `文档\Schedule` | `~/Documents/Schedule` | `~/Documents/Schedule` | 无（由用户选择） |
| 备份文件 | `<数据库>.bak`（恢复前自动生成） | 同左 | 同左 | 同左 |

`AppSettings::fallback_directory()` 统一实现上述默认目录规则；
用户可在“设置 → 导入 / 导出目录”中修改，修改后写入数据库的 `settings` 表。

---

## 8. 当前验证状态

| 平台 | 状态 | 说明 |
| ---- | ---- | ---- |
| Windows (MSVC, x64) | ✅ 已验证 | 完整构建 + `windeployqt` 自动部署 + 9 个单元测试 + `--selftest` 22 项断言全部通过 |
| Android (arm64-v8a, NDK 30) | ✅ 编译已验证 | `cmake --build build/android` 全量构建通过（含 QML 编译与本地通知后端）；APK 打包已改为 Qt 工具链 + `--target apk`（原先用 NDK 工具链只能编译，出的不是 APK）；**APK 装机与通知投递需真机验证** |
| Linux | ⚠️ 未验证 | 构建脚本无平台专属逻辑；托盘通知依赖桌面环境，无托盘时回退应用内横幅 |
| macOS | ⚠️ 未验证 | 同上；`macdeployqt` 步骤见第 3 节 |
| iOS | ⚠️ 未实现通知后端 | 界面与逻辑复用移动布局；`INotificationBackend` 已预留，当前回退为应用内横幅 |

> Android 交叉编译曾暴露一个只在 Clang 下出现的可移植性问题
> （`ConflictDetector` 使用嵌套类型的默认实参），已改为重载写法修正——
> 这也说明**每次都跑一遍 Android 构建**是有价值的。

---

## 相关文档

- [根 README](../README.md) — 架构与构建命令
- [i18n/README.md](../i18n/README.md) — 翻译工作流
- [src/app/README.md](../src/app/README.md) — 启动流程与 `--selftest`
- [阶段路线图](ROADMAP.md)
