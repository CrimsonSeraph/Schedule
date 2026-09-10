# app（应用入口）

## 职责

组装各层产物，创建 Qt 应用并启动 QML 界面，并**在 C++ 侧显式建立全部 QML ↔ C++ 信号连接**。本层是唯一允许同时依赖 `core` / `data` / `engine` / `ui` 的模块。

明确不负责：业务规则、数据持久化、导入导出算法、界面绘制。

## 依赖

| 依赖 | 类型 | 说明 |
| --- | --- | --- |
| `ScheduleUI` | 项目内 | QML 模块（MainDesktop.qml / MainMobile.qml） |
| `ScheduleEngine` | 项目内 | 桥接对象（`AppBridge`，阶段 4 起扩展） |
| `ScheduleData` | 项目内 | 选择并实例化仓库、导入导出管理器 |
| `Qt6::Widgets` | 外部 | **仅桌面**：`TrayNotificationBackend` 使用 `QSystemTrayIcon` |
| `ScheduleData` 的适配器实现 | 项目内 | `GenericSchoolAdapter` / `NetworkScheduleFetcher` / `LocalFileScheduleFetcher` |
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

| 参数         | 说明                                                                               |
| ------------ | ---------------------------------------------------------------------------------- |
| `--selftest` | 仅在 `-DBUILD_SELFTEST=ON` 构建中存在；执行端到端自检后退出（`0` 成功 / `2` 失败） |

### `--selftest` 自检内容（`src/app/SelfTest.cpp`）

| 步骤 | 验证内容                                                                                        |
| ---- | ----------------------------------------------------------------------------------------------- |
| 1    | 鼠标点击导航按钮 `navSettingsButton`，验证 C++ 侧导航连接与 `pageStack.currentIndex` 变化       |
| 2    | 鼠标点击设置页的 `testButton`，验证 `Button::clicked` → `AppBridge::test_button_clicked()` 链路 |
| 3    | 从 `<exe>/samples/schedule_sample.json` 生成导入预览，验证解析、冲突检测与重复统计              |
| 4    | 按“合并”策略应用导入，验证课表写入内存并落库                                                    |
| 5    | 依次导出 JSON / CSV / ICS 到临时目录，验证**导出文件确实存在于指定目录且非空**                  |
| 6    | 把刚导出的 JSON 再导入一次，验证 4 门课程全部识别为重复（往返一致性）                           |

共 22 项断言。样本文件由 `src/app/CMakeLists.txt` 的 `copy_directory` 复制到可执行文件同级目录，因此自检不依赖仓库路径；数据层始终只接收路径，不会读取内嵌资源。

## 启动流程

1. 创建 `QGuiApplication`，设置组织名 / 应用名 / 版本；
2. 组装数据层：`SqliteScheduleRepository`（`AppDataLocation/schedule.db`）→ `AppSettings` → `ScheduleService`；数据库打开失败时自动降级为 `:memory:` 内存库；
3. 在 C++ 侧实例化桥接对象：
    - `Schedule::AppBridge` → 上下文属性 `bridge`；
    - `Schedule::ScheduleBridge`（持有课程模型与导入导出桥接）→ 上下文属性 `schedule`；
    - `Schedule::NotificationService`（+ 平台通知后端）→ 上下文属性 `reminders`；桌面使用 `TrayNotificationBackend`（系统托盘气泡），此时改用 `QApplication` 以满足 `QSystemTrayIcon` 的要求；后端不可用时自动回退为应用内横幅；
4. 调用 `schedule_bridge.initialize()` 从数据库载入当前学期，**先有数据再加载 QML**；
5. 创建 `QQmlApplicationEngine`；
6. 按平台宏选择主 QML 并加载：
    - `Q_OS_ANDROID` / `Q_OS_IOS` → `qrc:/qt/qml/Schedule/qml/MainMobile.qml`
    - 其他平台（Windows / Linux / macOS）→ `qrc:/qt/qml/Schedule/qml/MainDesktop.qml`
7. 加载成功后在 C++ 侧显式建立信号连接（QML 不隐式连接）：
    - `UiConnector` 统一连接**全部界面交互**（导航、周次、课程编辑、导入导出向导、设置页），以及动态生成的课卡热区 `sessionCardClick`；
    - 设置页 `testButton` `clicked` → `AppBridge::test_button_clicked()`；
    - `AppBridge::test_signal(message)` → 应用日志输出；
    - `ScheduleBridge::errorOccurred` / `infoMessage` → 应用日志；
    - `ImportExportBridge::exportFinished` / `importFinished` → 应用日志（导出日志包含**实际写入路径**，便于排查“文件写到哪里了”）；
8. 加载失败（rootObjects 为空）返回 `-1`，否则进入事件循环。

> 桥接对象与 `UiConnector` 必须声明在 `QQmlApplicationEngine` **之前**，保证引擎先析构。

## 教务适配器的注册（阶段 9）

按分层约定，适配器**接口在 `core`、实现在 `data`、注册在 `app`**。`main.cpp` 中注册了两个：

| id | 名称 | 说明 |
| --- | --- | --- |
| `local-sample` | 本地样本适配器 | 指向 `<exe>/samples/schedule_sample.json`，**离线可用**，用于验证适配器全链路 |
| `generic-jwgl` | 通用教务适配器（实验性） | 地址从设置读取；需要用户在浏览器 / WebView 登录后粘贴 Cookie |

```cpp
std::vector<std::shared_ptr<Schedule::IScheduleFetcher>> schedule_fetchers = {
    std::make_shared<Schedule::NetworkScheduleFetcher>(),
    std::make_shared<Schedule::LocalFileScheduleFetcher>(),
};
Schedule::SchoolAdapterRegistry adapter_registry;
adapter_registry.register_adapter(
    std::make_unique<Schedule::GenericSchoolAdapter>(generic_adapter_info, schedule_fetchers));
...
schedule_bridge.import_export()->set_adapter_registry(&adapter_registry);
```

隐私约束（与项目“不做账号系统”的要求一致）：

- 适配器**只接收 Cookie**，不接收也不保存密码；
- Cookie 只存在于 `AdapterSession` 内存对象里，可随时通过设置页的“清除凭证”擦除；
- 只在用户点击“从适配器导入”时发起一次请求，**不做后台同步与定时轮询**。

详见 [../data/adapter/README.md](../data/adapter/README.md)。

## QML ↔ C++ 连接（`UiConnector`）

`UiConnector` 是**唯一**建立 QML 交互连接的地方。之所以不在 QML 中写 `onClicked`，是因为项目规范要求“信号连接统一在 C++ 侧显式建立”。实现要点：

- 每个交互控件通过 `objectName` 暴露；`UiConnector::find()` 负责定位。
- QML 控件的信号在公开 C++ 头文件中不可见，`QObject::connect(sender, SIGNAL(...), context, lambda)` 这种重载并不存在，因此采用：

    ```cpp
    m_handlers.insert(object, handler);                       // 控件 → 处理函数
    QObject::connect(object, "2clicked()", this, SLOT(dispatch()));  // 统一分发槽
    ```

    `dispatch()` 用 `sender()` 查表执行对应处理函数，连接仍然全部发生在 C++ 侧。

- 动态生成的课卡（`Repeater`）在模型 `modelReset`、周次 / 星期变化后**延迟一拍**重新扫描，并用 `destroyed` 信号清理映射，避免悬空指针。
- 启动日志会输出已建立的连接数量，便于确认界面契约是否完整。

## 目录结构（补充）

```text
src/app/
├── CMakeLists.txt
├── README.md
├── UiConnector.h    # QML ↔ C++ 连接集中管理
├── UiConnector.cpp
├── TrayNotificationBackend.h    # 桌面系统托盘通知后端（仅桌面构建）
├── TrayNotificationBackend.cpp
└── main.cpp
```

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
# 输出 22 项 PASS 与导出的三个文件路径，退出码 0；任一步失败退出码 2
```

点击界面中的“测试”按钮，控制台 / 调试输出应出现：

```text
Test button clicked!
[app] test_signal received: test_button_clicked
```

## 与上下层交互方式

- 向下：实例化 `data` 层仓库与导入导出管理器（阶段 2/3 起）、`engine` 层桥接对象；通过 `setContextProperty` 注入 QML。
- 向上：无。

## 信号连接约定

- 本层是**唯一**建立 QML ↔ C++ 信号连接的地方：
    - QML 控件信号在公开 C++ 头文件中不可见，因此按元对象签名连接（`SIGNAL(clicked())` → `SLOT(xxx_clicked())`）；
    - `engine` 桥接对象的信号使用函数指针形式（编译期检查）连接到本层 lambda。
- 连接对象通过 `objectName` 查找（`root->findChild<QObject*>("testButton")`），因此 UI 层每个交互控件必须提供稳定 `objectName`。
- 命名遵循全小写 + 下划线（槽 `test_button_clicked()`、信号 `test_signal(message)`）。
- 查找失败必须输出 `qWarning` 告警，避免静默丢失交互。

## 扩展点与注意事项

- **扩展点**：新增页面交互时，在 `main.cpp` 中集中追加连接；当连接数量增长到一定规模，应拆分为 `AppController` 类，但**仍保持全部连接在 C++ 侧建立**这一约定。
- **生命周期**：`AppBridge` 等桥接对象必须在 `QQmlApplicationEngine` 之前声明，保证析构顺序正确（引擎先析构，桥接对象后析构）。
- **连接时机**：必须在 `engine.load()` 之后再 `findChild`，否则控件尚未创建。
- **移动端**：`--selftest` 依赖 `Qt6::Test` 与鼠标事件，仅供桌面开发验证，不进入移动发布包。
- **禁止事项**：不得在此层实现业务规则或直接读写数据库（应下沉到 `core` / `data`）。

## 后续阶段计划

| 阶段   | 内容                                                                        |
| ------ | --------------------------------------------------------------------------- |
| 阶段 2 | 实例化 `SqliteScheduleRepository`，确定数据库路径（`AppDataLocation`）      |
| 阶段 4 | 注入 `ScheduleBridge` / `CourseListModel` / `ImportExportBridge` 并建立连接 |
| 阶段 7 | 扩展 `--selftest`：自动导入样本、导出到指定目录并校验文件存在               |
| 阶段 9 | 注册可选教务适配器实现                                                      |

## 相关文档

- [根 README](../README.md)
- [阶段路线图](../docs/ROADMAP.md)
- [模块 README 模板](../docs/README_TEMPLATE.md)
- [ui](../ui/README.md) — 本程序加载的 QML 模块
- [engine](../engine/README.md) / [data](../data/README.md) / [core](../core/README.md) — 下层依赖
