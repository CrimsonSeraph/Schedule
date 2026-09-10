# ui（QML 界面层）

## 职责

提供全部界面（QML）与静态资源，按目标平台 / 形态区分桌面与移动布局，通过 `engine`
层注入的桥接对象与 C++ 侧交互。承载**文件选择对话框**（数据层只接收路径）。

明确不负责：业务规则、文件解析、数据持久化——这些都在 `core` / `data`。

## 依赖

| 依赖 | 类型 | 说明 |
| ---- | ---- | ---- |
| `ScheduleEngine` | 项目内 | 提供 `AppBridge` 等桥接对象（阶段 4 起提供 `ScheduleBridge` 等） |
| `Qt6::Quick` / `Qt6::Qml` | 外部 | 界面使用 Quick Controls 2 / Layouts |

- 允许依赖：`engine`
- 禁止依赖：`data`（不得直接调用仓库或导入导出实现）、`app`

## 产物

- 目标名：`ScheduleUI`（静态库 + QML 模块，URI: `Schedule`，版本 1.0）
- QML 文件以资源方式内嵌，运行时路径前缀为 `qrc:/qt/qml/Schedule/`

## 目录结构

```text
src/ui/
├── CMakeLists.txt
├── README.md
├── qml/
│   ├── MainDesktop.qml   # 桌面 / 平板宽屏布局（960x640）
│   ├── MainMobile.qml    # 手机竖屏布局（480x800）
│   └── ...               # 阶段 5 起：WeekView / DayView / CourseCard / CourseEditor ...
└── resources/
    └── assets.qrc        # 静态资源清单（占位）
```

## 公开接口与关键类型

| 类型 | 说明 |
| ---- | ---- |
| `MainDesktop.qml` | 桌面主界面；显示版本标签并承载“测试”按钮 |
| `MainMobile.qml` | 移动主界面；同上，采用移动端间距与尺寸 |

两个主界面均：

- 显示版本标签 `bridge.version`（`bridge` 为 C++ 侧注入的上下文属性，见 app 层说明）；
- 提供文本为 “测试” 的 `Button`（`objectName: "testButton"`），其 `clicked` 信号由 C++ 侧
  显式连接到 `AppBridge::test_button_clicked()`（触发 qDebug 输出与 `test_signal`）；
  QML 中不书写 `onClicked` / `Connections` 等隐式连接。

## 构建与测试方式

```bash
cmake --preset windows-msvc
cmake --build --preset windows-msvc-debug
```

- 由根 `CMakeLists.txt` 通过 `add_subdirectory(src/ui)` 引入。
- 界面自检通过 app 层 `-DBUILD_SELFTEST=ON` + `Schedule.exe --selftest` 完成；
  自检依赖关键控件提供稳定的 `objectName`。

## 与上下层交互方式

- 向下：只通过 `app` 注入的上下文属性（`bridge` 等）读写状态，不 `import` C++ 类型。
- 向上：由 `app` 按平台加载对应主 QML：

```cpp
// 桌面：qrc:/qt/qml/Schedule/qml/MainDesktop.qml
// 移动：qrc:/qt/qml/Schedule/qml/MainMobile.qml
```

- 文件选择（导入 / 导出目录）使用 `QtQuick.Dialogs` 的 `FileDialog` / `FolderDialog`，
  拿到 `QUrl` 后交给桥接对象的槽函数，数据层只接收路径。

## 信号连接约定

- QML **不写** `onClicked` / `Connections` 等按名称隐式连接的写法；所有交互控件的
  `clicked` / `accepted` 等信号由 `app` 层在 C++ 侧用 `objectName` 找到控件后显式连接。
- 因此**每个交互控件必须设置唯一且稳定的 `objectName`**（供 C++ 连接与 UI 自检定位）。
- QML 只通过属性绑定读取桥接对象状态；状态变化由桥接对象的 `NOTIFY` 信号驱动。

## 扩展点与注意事项

- **扩展点**：新增页面时，在 `qml/` 下新增文件并注册到 `src/ui/CMakeLists.txt` 的
  `QML_FILES` 列表；交互事件同样交由 C++ 侧连接。
- **命名约定**：QML 内部 id / 属性用 `camelCase`（遵循 Prettier 与 Qt 惯例），
  但 `objectName` 面向 C++ 侧连接，需与 C++ 中的字符串常量保持一致。
- **模块化注意**：使用 `qt_policy(QTP0001/QTP0004)` 后资源前缀为 `/qt/qml`，
  QML 内引用同模块文件使用相对路径，跨模块需 `import Schedule`。
- **移动端注意**：触摸目标不小于 48dp；`MainMobile.qml` 避免使用悬浮窗口类控件。

## 后续阶段计划

| 阶段 | 内容 |
| ---- | ---- |
| 阶段 5 | `WeekView` / `DayView` / `CourseCard` / `CourseEditor` / `ImportWizard` / `ExportDialog` / `SettingsPage` / `SemesterPage`，桌面与移动两套主布局 |
| 阶段 7 | 关键控件 `objectName` 补全，支撑 `--selftest` 自动化 |

## 相关文档

- [根 README](../README.md)
- [阶段路线图](../docs/ROADMAP.md)
- [模块 README 模板](../docs/README_TEMPLATE.md)
- [engine](../engine/README.md) — 提供桥接对象的层
- [core](../core/README.md) / [data](../data/README.md) — 底层核心逻辑与基础设施
- [app](../app/README.md) — 加载本模块 QML 并建立信号连接的应用入口
