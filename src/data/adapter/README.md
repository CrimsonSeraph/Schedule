# data/adapter（可选教务适配器）

## 职责

提供**可选的、仅本地主动触发**的教务课表抓取能力：

- `LocalFileScheduleFetcher`：从磁盘读取“课表原文”（离线演示、测试、手工另存流程）；
- `NetworkScheduleFetcher`：用 `QNetworkAccessManager` 发起一次带 Cookie 的 GET；
- `GenericSchoolAdapter`：把抓取到的原文交给已有的 `ImportManager` 解析成课表快照。

## 分层与依赖

| 组件 | 所在层 | 说明 |
| ---- | ------ | ---- |
| `AdapterInfo` / `AdapterSession` / `IScheduleFetcher` / `ISchoolAdapter` / `SchoolAdapterRegistry` | **core**（`core/adapter/SchoolAdapter.h`） | 只有 `QtCore` 依赖，接口不依赖网络与 UI |
| `GenericSchoolAdapter` / `LocalFileScheduleFetcher` / `NetworkScheduleFetcher` | **data**（本目录） | 使用 `Qt6::Network` 与 `ImportManager` |
| 具体适配器的**构造与注册** | **app**（`src/app/main.cpp`） | 组装层决定启用哪些适配器 |

> 这正是需求中的分层约定：**接口在 core，实现在 data，注册在 app**。

## 隐私约束（硬性）

- **绝不接收、绝不保存明文密码**：`AdapterSession` 只有 Cookie 串与 User-Agent；
- Cookie **只驻留内存**，不写入 `settings` 表、不写日志、不进入导出文件；
  用户可随时点击设置页的“清除凭证”，`AdapterSession::clear()` 会先填零再清空；
- **不做后台同步、不做定时轮询、不做增量合并**——只有用户点击“从适配器导入”时才发起一次请求；
- 没有账号体系、没有用户表、没有 Token 存储。

## 工作流

```text
用户在设置页选择适配器
        │
        ├─ 填写“课表接口地址”（http(s) 或本地文件路径）
        ├─ （如需登录）在浏览器/WebView 中登录教务系统，复制 Cookie 请求头粘贴到设置页
        ▼
ImportExportBridge::import_from_adapter(index, cookie)
        │
        ├─ GenericSchoolAdapter::fetch_schedule(session, ...)
        │     ├─ 按 URL 协议选择 NetworkScheduleFetcher / LocalFileScheduleFetcher
        │     └─ 用 ImportManager 嗅探并解析返回内容（JSON / CSV / ICS）
        ▼
ImportManager::preview_snapshot(...)  ← 与文件导入完全相同的冲突检测与重复统计
        ▼
用户在导入向导中选择策略 → apply → 写入内存 → 落库
```

## 为什么不做“按学校写解析器”

各校教务系统页面结构差异极大，为每所学校维护一个 C++ 解析器成本高且难以验证。
本实现改为：**适配器只负责“拿到字节”，解析交给已有的标准格式导入器**。
因此接入一所新学校只需满足其一：

1. 教务系统能导出 / 暴露 JSON、CSV 或 ICS；
2. 或者由用户手工把页面另存为文件后走本地文件导入（完全不联网）。

如果确实需要解析 HTML 页面，正确做法是**在 `data` 层新增一个 `IScheduleImporter` 实现**
（例如 `HtmlTimetableIo`），然后适配器会自动复用它——无需改动 `GenericSchoolAdapter`。

## 公开接口与关键类型

| 类型 | 说明 |
| ---- | ---- |
| `AdapterInfo` | id / 名称 / 说明 / 课表接口地址 / 登录页地址 / 是否需要会话 / 是否实验性 |
| `AdapterSession` | 仅内存的 Cookie 会话；`is_empty()` / `clear()` / `age_hours()` |
| `IScheduleFetcher` | `supports(url)` + `fetch(url, session, error)`；便于注入测试替身 |
| `ISchoolAdapter` | `info()` / `set_endpoints()` / `can_handle()` / `last_format()` / `fetch_schedule()` |
| `SchoolAdapterRegistry` | 注册 / 按 id 查找 / 按下标访问 / 同 id 覆盖 / 清空 |
| `GenericSchoolAdapter` | 通用实现：抓取 + 复用导入器解析 |
| `LocalFileScheduleFetcher` | 本地文件抓取（忽略会话） |
| `NetworkScheduleFetcher` | HTTP(S) 同步 GET，默认 15 秒超时，必须主线程调用 |

## 构建与测试

```bash
cmake --preset windows-msvc -DBUILD_TESTS=ON
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc -C Debug -R tst_school_adapter
```

`tst_school_adapter` 用**假抓取器**覆盖解析链路（无需联网），并验证：

- 注册表行为与同 id 覆盖；
- `requires_session` 为真且会话为空时给出“请先登录并粘贴 Cookie”的可操作提示；
- 未配置接口地址时提示去设置中填写；
- 本地文件抓取器能读取 `samples/` 中的真实样本；
- 同时提供两种抓取器时，`file://` 地址会挑到本地实现。

## 扩展点与注意事项

- **接入新学校**：在 `app` 层 `register_adapter()` 一个新的 `GenericSchoolAdapter` 即可，
  不需要改动 `core` / `data`；若该校需要特殊页面解析，则新增一个 `IScheduleImporter` 实现。
- **凭据过期**：`AdapterSession::age_hours()` 可提示用户凭证可能过期；
  会话为空时 `requires_session` 适配器会直接报错而不是发出匿名请求。
- **线程**：`NetworkScheduleFetcher::fetch()` 内部启动局部事件循环，**只能在主线程调用**；
  若将来需要后台抓取，应改为异步实现并在 `engine` 层暴露进度信号。
- **失败不要静默**：抓取 / 解析失败都会通过 `error_message` 返回中文原因，
  并由 `ImportExportBridge` 转发为 `errorOccurred` 与预览失败提示。

## 相关文档

- [../README.md](../README.md) — data 层总览
- [../import_export/README.md](../import_export/README.md) — 导入导出子系统
- [../../core/README.md](../../core/README.md) — 适配器接口所在的层
- [../../app/README.md](../../app/README.md) — 适配器的注册位置
