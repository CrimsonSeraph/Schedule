# app（应用入口）

## 职责

组装各层产物，创建 Qt 应用并启动 QML 界面，并**在 C++ 侧显式建立全部 QML ↔ C++ 信号连接**。
本层是唯一允许同时依赖 `core` / `data` / `engine` / `ui` 的模块。

明确不负责：业务规则、数据持久化、导入导出算法、界面绘制。

## 依赖

| 依赖 | 类型 | 说明 |
| ---- | ---- | ---- |
| `ScheduleUI` | 项目内 | QML 模块（MainDesktop.qml / MainMobile.qml） |
| `ScheduleEngine` | 项目内 | 桥接对象（`AppBridge`，阶段 4 起扩展） |
| `ScheduleData` | 项目内 | 阶段 2 起：选择并实例化仓库、导入导出管理器 |
| `Qt6::Core` / `Qt6::Qml` / `Qt6::Quick` | 外部 | 应用与 QML 引擎 |

- 允许依赖：全部下层
- 禁止依赖：无上层

## 产物

- 目标名：`Schedule`（可执行程序）
- Windows 桌面构建后由 `windeployqt` 自动部署 Qt 运行库到 `Schedule.exe` 同级目录

## 目录结构

```text
src/app/
├── CMakeLists.txt
├── README.md
└── main.cpp
```

## 公开接口与关键类型

本模块不对外提供库接口，仅提供可执行程序与命令行开关：

| 参数 | 说明 |
| ---- | ---- |
| `--selftest` | 仅在 `-DBUILD_SELFTEST=ON` 构建中存在；自动点击“测试”按钮并退出（`0` 成功 / `2` 失败） |

## 启动流程

1. 创建 `QGuiApplication`，设置组织名 / 应用名 / 版本；
2. 在 C++ 侧实例化 `Schedule::AppBridge`，并注入为 QML 上下文属性 `bridge`；
3. 创建 `QQmlApplicationEngine`；
4. 按平台宏选择主 QML 并加载：
    - `Q_OS_ANDROID` / `Q_OS_IOS` → `qrc:/qt/qml/Schedule/qml/MainMobile.qml`
    - 其他平台（Windows / Linux / macOS）→ `qrc:/qt/qml/Schedule/qml/MainDesktop.qml`
5. 加载成功后在 C++ 侧显式建立信号连接（QML 不隐式连接）：
    - “测试”按钮 `clicked` → `AppBridge::test_button_clicked()`；
    - `AppBridge::test_signal(message)` → 应用日志输出；
6. 加载失败（rootObjects 为空）返回 `-1`，否则进入事件循环。

## 构建与测试方式

```bash
$env:QTDIR = "D:/Qt/6.9.3/msvc2022_64"
cmake --preset windows-msvc
cmake --build --preset windows-msvc-debug
./build/windows-msvc/Debug/Schedule.exe
```

自动化自检（需以 `-DBUILD_SELFTEST=ON` 配置）：

```bash
cmake --preset windows-msvc -DBUILD_SELFTEST=ON
cmake --build --preset windows-msvc-debug
./build/windows-msvc/Debug/Schedule.exe --selftest
```

点击界面中的“测试”按钮，控制台 / 调试输出应出现：

```text
Test button clicked!
[app] test_signal received: test_button_clicked
```

## 与上下层交互方式

- 向下：实例化 `data` 层仓库与导入导出管理器（阶段 2/3 起）、`engine` 层桥接对象；
  通过 `setContextProperty` 注入 QML。
- 向上：无。

## 信号连接约定

- 本层是**唯一**建立 QML ↔ C++ 信号连接的地方：
  - QML 控件信号在公开 C++ 头文件中不可见，因此按元对象签名连接
    （`SIGNAL(clicked())` → `SLOT(xxx_clicked())`）；
  - `engine` 桥接对象的信号使用函数指针形式（编译期检查）连接到本层 lambda。
- 连接对象通过 `objectName` 查找（`root->findChild<QObject*>("testButton")`），
  因此 UI 层每个交互控件必须提供稳定 `objectName`。
- 命名遵循全小写 + 下划线（槽 `test_button_clicked()`、信号 `test_signal(message)`）。
- 查找失败必须输出 `qWarning` 告警，避免静默丢失交互。

## 扩展点与注意事项

- **扩展点**：新增页面交互时，在 `main.cpp` 中集中追加连接；当连接数量增长到一定规模，
  应拆分为 `AppController` 类，但**仍保持全部连接在 C++ 侧建立**这一约定。
- **生命周期**：`AppBridge` 等桥接对象必须在 `QQmlApplicationEngine` 之前声明，
  保证析构顺序正确（引擎先析构，桥接对象后析构）。
- **连接时机**：必须在 `engine.load()` 之后再 `findChild`，否则控件尚未创建。
- **移动端**：`--selftest` 依赖 `Qt6::Test` 与鼠标事件，仅供桌面开发验证，不进入移动发布包。
- **禁止事项**：不得在此层实现业务规则或直接读写数据库（应下沉到 `core` / `data`）。

## 后续阶段计划

| 阶段 | 内容 |
| ---- | ---- |
| 阶段 2 | 实例化 `SqliteScheduleRepository`，确定数据库路径（`AppDataLocation`） |
| 阶段 4 | 注入 `ScheduleBridge` / `CourseListModel` / `ImportExportBridge` 并建立连接 |
| 阶段 7 | 扩展 `--selftest`：自动导入样本、导出到指定目录并校验文件存在 |
| 阶段 9 | 注册可选教务适配器实现 |

## 相关文档

- [根 README](../README.md)
- [阶段路线图](../docs/ROADMAP.md)
- [模块 README 模板](../docs/README_TEMPLATE.md)
- [ui](../ui/README.md) — 本程序加载的 QML 模块
- [engine](../engine/README.md) / [data](../data/README.md) / [core](../core/README.md) — 下层依赖
