# data/adapter（可选教务适配器）

## 职责

提供**可选的、仅本地主动触发**的教务课表导入能力，共有两条路线：

| 路线 | 触发方式 | 数据来源 | 适用场景 |
| --- | --- | --- | --- |
| **接口抓取** | `ImportExportBridge::import_from_adapter()` | `GenericSchoolAdapter` + `NetworkScheduleFetcher` 请求一个地址 | 教务系统的课表接口能直接返回 JSON / CSV / ICS / 正方 HTML |
| **网页抓取** | `ImportExportBridge::submit_web_capture()` | 内嵌浏览器中**用户当前打开的页面** | 页面需要交互式登录、或课表页地址带会话相关参数（如安徽工程大学） |

两条路线最终都会走 `ImportManager` 的同一套「嗅探 → 解析 → 冲突检测 → 预览 → 策略应用」，因此用户体验与文件导入完全一致。路线由界面决定，不需要用户理解其中的差别：

- 入口列表里**只有登录页**的适配器 → 走网页抓取（在内嵌浏览器里登录后抓当前页面）；
- 入口列表里配了**数据地址**的适配器 → 也可以走接口抓取。

本目录只包含**第二类路线的抓取器与通用适配器实现**；网页抓取用到的浏览器组件与注入脚本分别位于 `ui`（QML 后端）与 `engine`（`BrowserCaptureScript`），本层不依赖任何 Web 模块。

## 分层与依赖

| 组件 | 所在层 | 说明 |
| --- | --- | --- |
| `AdapterInfo` / `AdapterSession` / `IScheduleFetcher` / `ISchoolAdapter` / `SchoolAdapterRegistry` | **core**（`core/adapter/SchoolAdapter.h`） | 只有 `QtCore` 依赖，接口不依赖网络与 UI |
| `GenericSchoolAdapter` / `LocalFileScheduleFetcher` / `NetworkScheduleFetcher` | **data**（本目录） | 使用 `Qt6::Network` 与 `ImportManager` |
| 内嵌浏览器后端 + `EmbeddedBrowser` 统一外壳 | **ui**（`src/ui/qml/Embedded*.qml`） | 承载用户自己的登录会话 |
| 抓取脚本 + 抓取结果入预览 | **engine**（`BrowserCaptureScript.*`、`ImportExportBridge`） | 不读写 Cookie，只搬运页面原文 |
| 具体适配器的**构造与注册** | **app**（`src/app/main.cpp`） | 组装层决定启用哪些适配器 |

> 这正是需求中的分层约定：**接口在 core，实现在 data，注册在 app**。

## 隐私约束（硬性）

- **绝不接收、绝不保存明文密码**：`AdapterSession` 只有 Cookie 串与 User-Agent；
- Cookie **只驻留内存**，不写入 `settings` 表、不写日志、不进入导出文件；用户可随时点击设置页的“清除凭证”，`AdapterSession::clear()` 会先填零再清空；
- **网页抓取路线连 Cookie 都不接触**：会话完全留在内嵌 Web 组件内部，应用只接收 `document.documentElement.outerHTML`；注入脚本不读 Cookie、不读 localStorage、不发网络请求；
- **不做后台同步、不做定时轮询、不做增量合并**——只有用户点击“导入课表 / 从适配器导入”时才触发一次；
- 没有账号体系、没有用户表、没有 Token 存储。

## 工作流

### 路线一：接口抓取

```text
用户在设置页「高级选项」中填写课表接口地址（http(s) 或本地文件路径）
        ▼
ImportExportBridge::import_from_adapter(index, cookie)
        │
        ├─ GenericSchoolAdapter::fetch_schedule(session, ...)
        │     ├─ 按 URL 协议选择 NetworkScheduleFetcher / LocalFileScheduleFetcher
        │     └─ 用 ImportManager 嗅探并解析返回内容（JSON / CSV / ICS / 正方 HTML）
        ▼
ImportManager::preview_snapshot(...)  ← 与文件导入完全相同的冲突检测与重复统计
```

### 路线二：网页抓取

```text
用户在「从教务导入」列表中选择入口（固定的“打开内置浏览器”或某所已适配学校）
        ▼
EmbeddedBrowser 在内嵌后端（Qt WebView / Qt WebEngine）中打开该地址
        │
        ├─ 用户自行登录，并停在课表页面
        ▼
点“导入课表” → runJavaScript(BrowserCaptureScript)
        │
        ├─ 取回整页 HTML，并在页面右下角注入“抓取课表”悬浮按钮
        ▼
ImportExportBridge::submit_web_capture(html, url)
        ▼
ImportManager::preview_data(html, source, current)  ← 与文件导入同一条路径
```

两条路线之后都是：**用户在导入向导中选择策略 → `apply` → 写入内存 → 落库**。

## 已适配的学校

| 适配器 id | 名称 | 配置 | 说明 |
| --- | --- | --- | --- |
| `local-sample` | 本地样本适配器 | 数据地址 = `samples/schedule_sample.json` | 离线演示与测试，走接口抓取 |
| `generic-jwgl` | 通用教务适配器（实验性） | 地址由用户填写 | 走接口抓取，需要用户提供 Cookie |
| `ahpu-jwxt` | 安徽工程大学教务系统（正方 V9） | 仅登录页 `http://xjwxt.ahpu.edu.cn/ahpu/localLogin.action` | **浏览器直达入口**，走网页抓取 |
| `ecjtu-jwzhglxt` | 华东交通大学教务综合管理系统 | 仅登录页 `https://jwxt.ecjtu.edu.cn` | **浏览器直达入口**，走网页抓取 |

### 安徽工程大学（`ahpu-jwxt`）

- 只配置 `login_url`，**不配置** `schedule_url`：课表页实测为 `/ahpu/courseTableForStd!courseTable.action`，但其 `ids` 参数与会话绑定，交给用户在内嵌浏览器里自然地走一遍（登录 → 个人课表）比在应用里猜地址更稳；
- 页面是**正方教务 V9** 的标准课表页，导出的 `课表.xls` 与它同构，因此抓到的原文由 `ZhengfangTimetableIo` 直接解析（见 `../import_export/README.md`）；
- 由于没有 `schedule_url`，对它的“从适配器导入”会给出明确引导而不是发出无效请求。

### 华东交通大学（`ecjtu-jwzhglxt`）

- 同样只配置 `login_url`（`https://jwxt.ecjtu.edu.cn`），理由与安徽工程大学一致：课表页要在登录会话内才有；
- 该校使用的**不是**正方教务，页面结构不同，因此另行实现了 `EcjtuTimetableIo`（见 `../import_export/README.md`）；

`AdapterInfo::is_valid()` 因此放宽为：**`schedule_url` 与 `login_url` 至少填一个**。只有登录页的适配器是合法的“浏览器直达入口”，而不是配置错误。

## 为什么不按学校写 C++ 适配器

各校教务系统页面差异极大，为每所学校维护一个 C++ 适配器成本高、难以验证。本实现的分工是：

1. **学校差异收敛到“注册一个 `AdapterInfo`”**：多数情况只需在 `app` 层加一条登录页 / 数据地址，不需要新的 C++ 类型；
2. **同源教务系统共用一个解析器**：正方教务（zfn / zfsoft V9）被大量高校使用，其页面结构一致，因此只写**一个** `ZhengfangTimetableIo`，而不是每校一个；
3. **解析器只认结构、不认学校**：`ZhengfangTimetableIo` 判断的是 `manualArrangeCourseTable` / `TaskActivity` 这类结构特征，新学校只要用同源系统即可直接复用。

只有当某校使用**完全不同**的页面结构时，才需要新增一个 `IScheduleImporter` 实现（放在 `data/import_export/academic_affairs/`），并在 `ImportManager` 构造函数中注册。

## 公开接口与关键类型

| 类型 | 说明 |
| --- | --- |
| `AdapterInfo` | id / 名称 / 说明 / 课表接口地址 / 登录页地址 / 是否需要会话 / 是否实验性；两个地址至少填一个 |
| `AdapterSession` | 仅内存的 Cookie 会话；`is_empty()` / `clear()` / `age_hours()` |
| `IScheduleFetcher` | `supports(url)` + `fetch(url, session, error)`；便于注入测试替身 |
| `ISchoolAdapter` | `info()` / `set_endpoints()` / `can_handle()` / `last_format()` / `fetch_schedule()` |
| `SchoolAdapterRegistry` | 注册 / 按 id 查找 / 按下标访问 / 同 id 覆盖 / 清空 |
| `GenericSchoolAdapter` | 通用实现：抓取 + 复用导入器解析；纯登录入口会给出可执行提示 |
| `LocalFileScheduleFetcher` | 本地文件抓取（忽略会话） |
| `NetworkScheduleFetcher` | HTTP(S) 同步 GET，默认 15 秒超时，必须主线程调用 |

## 内嵌浏览器后端

后端在**配置期**由根 `CMakeLists.txt` 探测（`SCHEDULE_BROWSER_BACKEND`），QML 通过 `schedule.importExport.webBrowserBackend` 读取：

| 取值 | 实现文件 | 触发条件 |
| --- | --- | --- |
| `webview` | `ui/qml/EmbeddedWebView.qml` | 有 `Qt6::WebView` + `Qt6::WebChannel`，**且**桌面平台存在 `<Qt>/plugins/webview` 后端插件 |
| `webengine` | `ui/qml/EmbeddedWebEngine.qml` | 上一条不成立，但有 `Qt6::WebEngineQuick` |
| `none` | `ui/qml/EmbeddedFallback.qml` | 两者都没有（**官方 MinGW 套件即如此**） |

可以整体关闭：`-DSCHEDULE_ENABLE_EMBEDDED_BROWSER=OFF`，此时不链接任何 Web 模块。

为什么桌面要额外检查插件目录：Qt WebView 在桌面只是“外壳”，真正渲染的是 `plugins/webview` 下的后端插件（Windows 官方包为 `qtwebview_webengine`，即**基于Qt WebEngine**）。官方 MinGW 套件不附带该插件，若照常启用，程序只会打印 `No WebView plug-in found!` 而完全不可用，因此在配置期就把它排除，回退到 WebEngine 或 none。Android / iOS 的后端由平台自身提供，不需要该插件目录。

`none` 时功能不缺失，只是换成兜底路径：**系统浏览器打开 + 教务系统“导出”后走文件导入**。

### 抓取为什么用 `runJavaScript` 拉取，而不是 QWebChannel 推送

Qt WebView（本项目的首选后端）**没有 `webChannel` 属性**，只有 Qt WebEngine 支持。若用 QWebChannel，就需要为不同后端维护两套通信协议。改用 `runJavaScript(script, callback)` 的**拉取**模型后，三种后端行为完全一致，也不必把任何 Qt 对象暴露给教务页面。脚本同时把结果写入 `window.__scheduleCapturePayload`，页面内的“抓取课表”按钮复用同一份数据。

### 抓取为什么必须取整页 HTML

正方教务把**逐周位图**放在页内脚本的 `new TaskActivity(...)` 参数里，而渲染出来的表格只显示折叠后的周次文本：单周课的单元格写作 `第1-11`（真实含义是 1,3,5,7,9,11）。仅凭表格文本无法还原真实周次，所以抓取必须保留脚本原文。若某个页面完全在客户端拼装表格、HTML 里没有 `TaskActivity`，解析会**明确失败并给出可执行提示**，而不是把折叠过的周次当作真实数据静默写入课表。

## 构建与测试

```bash
cmake --preset windows-msvc -DBUILD_TESTS=ON
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc -C Debug -R tst_school_adapter
```

`tst_school_adapter` 用**假抓取器**覆盖解析链路（无需联网），并验证：

- 注册表行为与同 id 覆盖；
- `requires_session` 为真且会话为空时给出“请先登录并粘贴 Cookie”的可操作提示；
- **浏览器直达入口**（仅登录页）合法，且对它的抓取请求会引导用户去内嵌浏览器；
- 两个地址都为空时提示去设置中填写；
- 本地文件抓取器能读取 `samples/` 中的真实样本；
- 同时提供两种抓取器时，`file://` 地址会挑到本地实现。

## 扩展点与注意事项

- **接入新学校（推荐）**：在 `app` 层 `register_adapter()` 一条 `AdapterInfo` 即可。若该校用正方教务，只填 `login_url` 就已经可用（浏览器直达 + 网页抓取）。
- **接入新学校（页面结构不同）**：在 `data/import_export/academic_affairs/` 下新增一个 `IScheduleImporter` 实现，并在 `ImportManager` 构造函数中注册，抓取路线会自动复用它。
- **凭据过期**：`AdapterSession::age_hours()` 可提示用户凭证可能过期；会话为空时 `requires_session` 适配器会直接报错而不是发出匿名请求。
- **线程**：`NetworkScheduleFetcher::fetch()` 内部启动局部事件循环，**只能在主线程调用**；内嵌浏览器的 `runJavaScript` 同样是异步回调，但回调回到主线程执行。
- **失败不要静默**：抓取 / 解析失败都会通过 `error_message` 返回中文原因，并由 `ImportExportBridge` 转发为 `errorOccurred`、`webCaptureFinished` 与预览失败提示。

## 相关文档

- [../README.md](../README.md) — data 层总览
- [../import_export/README.md](../import_export/README.md) — 导入导出子系统与正方解析器
- [../../core/README.md](../../core/README.md) — 适配器接口所在的层
- [../../app/README.md](../../app/README.md) — 适配器的注册位置与界面连线
- [../../ui/README.md](../../ui/README.md) — 内嵌浏览器的 QML 结构
