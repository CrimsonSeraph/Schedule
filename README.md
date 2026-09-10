# Schedule

基于 **Qt 6.9.3 / C++20 / CMake** 的跨平台（Windows · Linux · macOS · Android · iOS）**本地课表应用**。

- 课表领域模型：学期、课程、上课时间段、节次、周次表达式、冲突检测、当前周计算；
- 本地持久化：SQLite（建表 / 迁移 / 备份 / 恢复）与 JSON 序列化；
- **导入导出到指定目录**：JSON / CSV / ICS 三种格式，支持预览、冲突检测、合并 / 去重 / 覆盖；
- QML 界面：周视图、日视图、课程编辑、导入导出向导、学期与设置页，桌面 / 移动两套布局；
- 本地提醒：上课前 5 / 10 / 15 分钟系统通知（桌面托盘 / Android 本地通知 / 应用内横幅兜底）；
- 可选教务适配器：仅本地主动触发，只接收 Cookie、不保存密码，不做后台同步。

> **明确不做**：云同步、账号系统、OAuth、用户表、同步队列、后端 API。数据只保存在本机，导入导出完全由用户手动触发。

---

## 架构总览

项目采用自底向上的**五层架构**，层间只能依赖下层，**禁止反向依赖**：

```text
app  ──►  ui  ──►  engine  ──►  data  ──►  core
```

| 层 | 目录 | 目标 | 依赖 | 职责 |
| --- | --- | --- | --- | --- |
| core | `src/core` | `ScheduleCore`（静态库） | `Qt6::Core` | 领域模型与核心服务；**无任何 GUI 依赖**，可在无显示环境单元测试 |
| data | `src/data` | `ScheduleData`（静态库） | `ScheduleCore`、`Qt6::Core`、`Qt6::Sql` | SQLite 持久化、JSON 映射、导入导出实现、设置存储 |
| engine | `src/engine` | `ScheduleEngine`（静态库） | `ScheduleData`、`ScheduleCore`、`Qt6::Core`、`Qt6::Qml` | QML 桥接对象与 `QAbstractListModel`；导入导出编排；本地提醒服务 |
| ui | `src/ui` | `ScheduleUI`（QML 模块，URI `Schedule`） | `ScheduleEngine`、`Qt6::Quick`、`Qt6::Qml`、`Qt6::QuickDialogs2` | QML 界面与静态资源；**文件 / 目录选择对话框** |
| app | `src/app` | `Schedule`（可执行程序） | 全部下层（桌面另加 `Qt6::Widgets`） | 组装各层、加载 QML、**在 C++ 侧显式建立全部信号连接** |

关键约束：

- `core` 只链接 `Qt6::Core`，不出现 `QWidget` / `QQmlEngine` / `QSqlDatabase`；
- **数据层只接收路径（`QString`）或 `QUrl`**，文件选择对话框一律放在 UI 层；
- QML 中**不写** `onClicked` / `Connections` / `onXxx`；全部交互由 `app/UiConnector` 按 `objectName` 在 C++ 侧显式连接；
- 槽与信号命名统一为全小写 + 下划线（如 `test_button_clicked()`、`test_signal(message)`）。

---

## 目录结构

```text
Schedule/
├── CMakeLists.txt              # 根构建脚本（五层依次 add_subdirectory）
├── CMakePresets.json           # 共享预设（Qt / windows-desktop-msvc / android-arm64-v8a）
├── CMakeUserPresets.json       # 本机预设（含 Qt 路径，已被 .gitignore 忽略）
├── CodingStyle.md              # 组织编码规范
├── LICENSE
├── cmake/
│   └── ScheduleQtDeploy.cmake  # 统一的 Qt 运行库部署辅助（windeployqt）
├── docs/
│   ├── README_TEMPLATE.md      # 模块 README 模板
│   ├── ROADMAP.md              # 分层约定与阶段路线图
│   └── PACKAGING.md            # 各平台打包与发布说明
├── i18n/
│   ├── README.md               # 翻译工作流
│   └── schedule_en.ts          # 英文翻译源（源语言为简体中文）
├── samples/
│   ├── README.md
│   ├── schedule_sample.json    # 三种内容等价的课表样本
│   ├── schedule_sample.csv
│   └── schedule_sample.ics
└── src/
    ├── core/                   # 领域模型 + 核心服务（+ tests/）
    ├── data/                   # 持久化 + 导入导出（+ tests/、import_export/README.md）
    ├── engine/                 # QML 桥接 + 提醒服务
    ├── ui/                     # QML 界面
    └── app/                    # 应用入口 + UiConnector + 自检
```

---

## 环境要求

- CMake ≥ 3.20
- 支持 C++20 的编译器（MSVC / GCC 11+ / Clang 14+）
- Qt **6.9.3**，组件：`Core`、`Gui`、`Qml`、`Quick`、`QuickDialogs2`、`Sql`、`Test`（测试）、 `Widgets`（仅桌面，用于系统托盘通知）、`LinguistTools`（国际化，可选）
- Android 交叉编译：NDK + Ninja，工具链取自 `$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake`

---

## 构建命令

```bash
# ---------------------------------------------------------------- Windows 桌面
$env:QTDIR = "D:/Qt/6.9.3/msvc2022_64"        # PowerShell；Linux/macOS 用 export
cmake --preset windows-msvc
cmake --build --preset windows-msvc-debug
./build/windows-msvc/Debug/Schedule.exe

# ------------------------------------------------------------------------ Linux
export QTDIR=/opt/Qt/6.9.3/gcc_64
cmake -S . -B build/linux -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=$QTDIR
cmake --build build/linux -j

# ----------------------------------------------------------------------- macOS
cmake -S . -B build/macos -G Xcode -DCMAKE_PREFIX_PATH=$HOME/Qt/6.9.3/macos
cmake --build build/macos --config Release

# --------------------------------------------------------------------- Android
$env:QTDIR = "D:/Qt/6.9.3/android_arm64_v8a"
$env:QT_HOST_PATH = "D:/Qt/6.9.3/msvc2022_64"   # 同版本桌面套件，提供 androiddeployqt 等宿主工具
$env:ANDROID_NDK_ROOT = "C:/Users/<用户>/AppData/Local/Android/Sdk/ndk/30.0.16138531"
$env:ANDROID_NDK_HOME = $env:ANDROID_NDK_ROOT
$env:ANDROID_SDK_ROOT = "C:/Users/<用户>/AppData/Local/Android/Sdk"
cmake --preset android
cmake --build build/android --target apk        # 打 APK（--target aab 出 Google Play 的 AAB）
```

> `windows-msvc` / `android` 属于**本机预设**，定义在已被 `.gitignore` 忽略的 `CMakeUserPresets.json` 中；新克隆的仓库需自行创建该文件，继承 `CMakePresets.json` 里的基础预设并填入本机 Qt / NDK 路径（`android` 预设还需要 `QT_HOST_PATH`）。
> Android 打包必须走 Qt 自带的 `qt.toolchain.cmake`（预设里已配置），只用 NDK 工具链只能编译出 `.so`，详见 [docs/PACKAGING.md](docs/PACKAGING.md) 第 4 节。

### 构建选项

| 选项             | 默认  | 说明                                                      |
| ---------------- | ----- | --------------------------------------------------------- |
| `BUILD_TESTS`    | `OFF` | 构建 QTest 单元测试（9 个目标）并启用 CTest               |
| `BUILD_SELFTEST` | `OFF` | 构建 `Schedule.exe --selftest` 端到端自检（仅桌面开发用） |

发布的正式包应当**同时关闭**这两个选项。

---

## 测试与自检

```bash
# 单元测试（Visual Studio 生成器为多配置，需显式指定 -C Debug）
cmake --preset windows-msvc -DBUILD_TESTS=ON
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc -C Debug
```

| 测试目标 | 位置 | 覆盖内容 |
| --- | --- | --- |
| `tst_week_mask` | `src/core/tests` | 周次表达式解析（区间 / 步长 / 单双周 / 混合片段）、集合运算、`MAX_WEEKS` 边界 |
| `tst_week_calculator` | `src/core/tests` | 日期 ↔ 周次 ↔ 星期换算、当前周、非周一开学对齐、时间与星期文本解析 |
| `tst_conflict_detector` | `src/core/tests` | 时间重叠 / 边界、课程内部重叠、缺时间段、节次与周次越界、重复课程、排序去重 |
| `tst_schedule_service` | `src/core/tests` | 学期切换清空、课程 CRUD、信号发射、课表查询、快照往返、默认作息回退 |
| `tst_reminder_scheduler` | `src/core/tests` | 提前分钟数、每日提醒排序、单双周过滤、时间窗口、到点判定、下一次提醒 |
| `tst_schedule_json` | `src/data/tests` | JSON 往返、信封字段校验、字段兼容与回退、原子写文件 |
| `tst_sqlite_repository` | `src/data/tests` | 六表齐备、空库迁移、版本过高拒绝、快照往返、当前学期唯一性、设置、备份恢复 |
| `tst_import_export` | `src/data/tests` | 格式识别、文件名规则、三格式往返、预览与新引入冲突、三种合并策略、错误路径 |
| `tst_sample_files` | `src/data/tests` | `samples/` 三格式解析结果必须**内容等价**（含 RRULE/RDATE 两条路径） |

### 端到端自检

```bash
cmake --preset windows-msvc -DBUILD_TESTS=ON -DBUILD_SELFTEST=ON
cmake --build --preset windows-msvc-debug
./build/windows-msvc/Debug/Schedule.exe --selftest
```

自检会：点击导航按钮 → 点击设置页“自检”按钮（验证 C++ 侧连接）→ 导入 `samples/schedule_sample.json` → 导出 JSON / CSV / ICS 到临时目录并**校验文件确实存在** → 把导出的 JSON 再导入回来校验往返一致性。共 22 项断言，全部通过退出码 `0`，失败 `2`。

---

## 导入导出（指定目录）

- **默认目录**：`QStandardPaths::DocumentsLocation + "/Schedule"`，可在“设置 → 导入 / 导出目录”中修改，修改后写入数据库 `settings` 表；
- **导出文件名**：`Schedule_<学期>_<yyyyMMdd_HHmmss>.<ext>`，学期名会自动清洗掉路径分隔符等非法字符；导出完成后界面**原样展示实际写入路径**；
- **导入流程**：选择文件 → 预览（格式 / 新增数 / 重复数 / **本次新引入的冲突** / 提示）→ 选择策略（合并 / 去重合并 / 覆盖）→ 应用并落库；
- **数据层只接收路径或 `QUrl`**，文件与目录选择对话框在 UI 层（`QtQuick.Dialogs` 的 `FileDialog` / `FolderDialog`）。

三种格式的映射细节见 [src/data/import_export/README.md](src/data/import_export/README.md)：

| 格式 | 特点                                                                                                          |
| ---- | ------------------------------------------------------------------------------------------------------------- |
| JSON | **无损**：含作息表、周次位图、颜色、学分、时间段级地点 / 教师覆盖                                             |
| CSV  | Excel 友好，一行一个上课时间段；导出为 UTF-8 with BOM，列名支持中英文别名                                     |
| ICS  | 与系统日历互操作；等差周次用 `RRULE`（含 `INTERVAL`），非等差用 `RDATE`；扩展属性 `X-SCHEDULE-*` 保证往返无损 |

---

## 可选教务适配器

分层约定：**接口在 `core`（`core/adapter/SchoolAdapter.h`），实现在 `data`，注册在 `app`**。

- 只在用户点击“从适配器导入”时发起**一次**请求；不做定时轮询、不做后台同步、不做增量合并；
- **不接收也不保存明文密码**：凭证只来自 WebView / 浏览器登录后的 Cookie，且仅驻留内存，可在设置页一键清除；
- 适配器只负责“拿到字节”，解析复用已有的 JSON / CSV / ICS 导入器与同一套预览 / 冲突检测流程；
- 内置两个示例：`local-sample`（离线读取 `samples/schedule_sample.json`）与 `generic-jwgl`（实验性，地址与 Cookie 由用户在设置页填写）。

详见 [src/data/adapter/README.md](src/data/adapter/README.md)。

## 本地提醒

- 支持提前 **5 / 10 / 15** 分钟提醒，可在设置页开关与调整；
- `core::ReminderScheduler` 只做纯计算（可单元测试），`engine::NotificationService` 每 30 秒轮询一次并去重（同一节课只提醒一次，跨天自动重置）；
- 通知后端可插拔（`INotificationBackend`）：

| 后端                         | 平台    | 说明                                                                |
| ---------------------------- | ------- | ------------------------------------------------------------------- |
| `TrayNotificationBackend`    | 桌面    | 系统托盘气泡（`QSystemTrayIcon`，需 Qt Widgets，故实现在 `app` 层） |
| `AndroidNotificationBackend` | Android | `NotificationManager` 本地通知（需真机验证）                        |
| `NullNotificationBackend`    | 其它    | 回退为**应用内横幅**，保证提醒不丢失                                |

---

## 国际化

源语言为**简体中文**（`qsTr()` / `tr()` 直接写中文），其它语言按 `i18n/schedule_<locale>.ts` 提供：

```bash
cmake --build --preset windows-msvc-debug --target update_translations   # 扫描源码更新 .ts
linguist i18n/schedule_en.ts                                            # 翻译
cmake --build --preset windows-msvc-debug                                # 自动 lrelease 并嵌入 :/i18n
```

详见 [i18n/README.md](i18n/README.md)。

---

## 打包与发布

各平台的部署步骤（`windeployqt` / `macdeployqt` / `androiddeployqt` / linuxdeploy）、依赖清单、数据目录与移动端注意事项见 **[docs/PACKAGING.md](docs/PACKAGING.md)**。

Windows 桌面构建已内置 `windeployqt` 自动部署（`cmake/ScheduleQtDeploy.cmake`）。

免费签名与未签名产物说明见 [docs/FREE_SIGNING.md](docs/FREE_SIGNING.md)。

---

## 数据存放位置

| 内容                | Windows                                   | Linux / macOS                                     |
| ------------------- | ----------------------------------------- | ------------------------------------------------- |
| 数据库              | `%APPDATA%\Schedule\Schedule\schedule.db` | `~/.local/share/Schedule/Schedule/schedule.db` 等 |
| 备份                | `<数据库>.bak`（恢复前自动生成）          | 同左                                              |
| 默认导入 / 导出目录 | `文档\Schedule`                           | `~/Documents/Schedule`                            |

数据库使用 SQLite（`user_version` 迁移）；不支持云同步，请通过“导出”功能自行备份。

---

## 模块文档

- [src/core/README.md](src/core/README.md) — 领域模型与核心服务（`ScheduleCore`）
- [src/core/tests/README.md](src/core/tests/README.md) — 核心层单元测试
- [src/data/README.md](src/data/README.md) — 持久化与基础设施（`ScheduleData`）
- [src/data/import_export/README.md](src/data/import_export/README.md) — 导入导出子系统
- [src/data/tests/README.md](src/data/tests/README.md) — 持久化层单元测试
- [src/engine/README.md](src/engine/README.md) — QML 桥接与提醒服务（`ScheduleEngine`）
- [src/ui/README.md](src/ui/README.md) — QML 界面（`ScheduleUI`，URI `Schedule`）
- [src/app/README.md](src/app/README.md) — 应用入口、`UiConnector`、适配器注册与 `--selftest`
- [src/data/adapter/README.md](src/data/adapter/README.md) — 可选教务适配器（隐私约束与扩展方式）
- [samples/README.md](samples/README.md) — 课表样本文件
- [i18n/README.md](i18n/README.md) — 翻译工作流
- [docs/ROADMAP.md](docs/ROADMAP.md) — 分层约定与阶段路线图
- [docs/PACKAGING.md](docs/PACKAGING.md) — 打包与发布
- [docs/README_TEMPLATE.md](docs/README_TEMPLATE.md) — 模块 README 模板

---

## 开发约定

- C++ 源码遵循组织编码规范（`.clang-format` / `.editorconfig` / `.gitattributes`），提交前执行：

    ```bash
    find src -name "*.cpp" -o -name "*.h" | xargs clang-format -i
    ```

- 公共类 / 结构体 / 枚举 / 函数 / 信号 / 槽 / `Q_PROPERTY` 必须有 Doxygen 风格中文注释；复杂逻辑、边界条件、平台差异与格式映射必须写注释说明原因；
- 提交信息使用中文，格式：

    ```text
    feat(xx): 简述
    - 变更点 1
    - 变更点 2
    - 变更点 3
    ```

    `xx` 取模块名：`core`、`data`、`engine`、`ui`、`app`、`test`、`build`、`docs`。

---

## 许可证

见 [LICENSE](LICENSE)。
