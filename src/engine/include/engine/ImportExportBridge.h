#pragma once

#include "core/adapter/SchoolAdapter.h"
#include "core/service/ScheduleService.h"
#include "data/AppSettings.h"
#include "data/IScheduleRepository.h"
#include "data/import_export/ExportManager.h"
#include "data/import_export/ImportExportTypes.h"
#include "data/import_export/ImportManager.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariantList>

namespace Schedule {

    /**
     * @brief 导入导出桥接对象：把 `data` 层的导入导出能力暴露给 QML
     *
     * **职责边界（重要）**：
     *  - 文件 / 目录选择对话框在 **QML 层**（`QtQuick.Dialogs`），本对象只接收 `QUrl`；
     *  - 数据的解析与落盘在 `data` 层，本对象只做编排、状态缓存与信号转发
     *
     * **典型交互（信号连接全部在 C++ 侧建立）**：
     *  1. 用户点“导入”按钮 → C++ 打开 QML 的 `FileDialog`；
     *  2. `FileDialog::accepted` → C++ 读取其 `selectedFile` → 调用 `preview_import(url)`；
     *  3. 界面展示预览（冲突数、重复数、提示）→ 用户选择策略；
     *  4. 点“确认导入” → C++ 调用 `apply_import(strategyIndex)`；
     *  5. `import_finished(...)` 由 C++ 侧连接回界面提示
     *
     * 导出流程同理：`FolderDialog` → `export_schedule(formatIndex, directoryUrl)` →
     * `export_finished(...)`（其中包含**实际写入路径**）
     */
    class ImportExportBridge : public QObject {
        Q_OBJECT

        /** 默认导入目录（未设置时为 `Documents/Schedule`） */
        Q_PROPERTY(QString defaultImportDir READ default_import_dir NOTIFY directoriesChanged)

        /** 默认导出目录（未设置时为 `Documents/Schedule`） */
        Q_PROPERTY(QString defaultExportDir READ default_export_dir NOTIFY directoriesChanged)

        /** 最近一次导入使用的目录，用作文件对话框的初始位置 */
        Q_PROPERTY(QString lastImportDir READ last_import_dir NOTIFY directoriesChanged)

        /** 最近一次导出使用的目录，用作目录对话框的初始位置 */
        Q_PROPERTY(QString lastExportDir READ last_export_dir NOTIFY directoriesChanged)

        /** 是否已有一个待确认的导入预览 */
        Q_PROPERTY(bool hasPendingPreview READ has_pending_preview NOTIFY previewChanged)

        /** 预览摘要文本 */
        Q_PROPERTY(QString previewSummary READ preview_summary NOTIFY previewChanged)

        /** 预览中的文件格式显示名 */
        Q_PROPERTY(QString previewFormatName READ preview_format_name NOTIFY previewChanged)

        /** 预览中预计新增的课程数 */
        Q_PROPERTY(int previewNewCount READ preview_new_count NOTIFY previewChanged)

        /** 预览中与现有课表重复的课程数 */
        Q_PROPERTY(int previewDuplicateCount READ preview_duplicate_count NOTIFY previewChanged)

        /** 预览中“本次导入新引入”的冲突数量 */
        Q_PROPERTY(int previewConflictCount READ preview_conflict_count NOTIFY previewChanged)

        /** 预览中的非阻断提示列表 */
        Q_PROPERTY(QStringList previewWarnings READ preview_warnings NOTIFY previewChanged)

        /** 预览中的冲突明细（map 列表，字段见 `conflict_to_map`） */
        Q_PROPERTY(QVariantList previewConflicts READ preview_conflicts NOTIFY previewChanged)

        /** 全部导入策略的展示名，供 ComboBox 使用 */
        Q_PROPERTY(QStringList strategyNames READ strategy_names CONSTANT)

        /** 全部导出格式的展示名，供 ComboBox 使用 */
        Q_PROPERTY(QStringList formatNames READ format_names CONSTANT)

        /**
         * 已适配的课表类型（`展示名 · 扩展名`），来自已注册的导入器
         *
         * 由注册表实时派生而不是在 QML 里硬编码，新增导入器后界面说明自动同步
         */
        Q_PROPERTY(QStringList adaptedTimetableTypes READ adapted_timetable_types CONSTANT)

        /** 最近一次导出的**实际文件路径**（失败时为空） */
        Q_PROPERTY(QString lastExportPath READ last_export_path NOTIFY exportFinishedChanged)

        /** 最近一次导出的摘要文本 */
        Q_PROPERTY(QString lastExportSummary READ last_export_summary NOTIFY exportFinishedChanged)

        /** 最近一次导入的摘要文本 */
        Q_PROPERTY(QString lastImportSummary READ last_import_summary NOTIFY importFinishedChanged)

        /** 最近一次错误信息（面向用户的中文） */
        Q_PROPERTY(QString lastError READ last_error NOTIFY errorOccurred)

        /** 导入 / 导出进度百分比（0..100） */
        Q_PROPERTY(int progress READ progress NOTIFY progressChanged)

        /** 已注册的教务适配器（map 列表：`id` / `name` / `description` / `scheduleUrl` / `loginUrl` / `requiresSession`） */
        Q_PROPERTY(QVariantList adapterOptions READ adapter_options NOTIFY adaptersChanged)

        /** 适配器会话状态文本（是否已粘贴 Cookie、凭证年龄） */
        Q_PROPERTY(QString adapterSessionStatus READ adapter_session_status NOTIFY adaptersChanged)

        /**
         * 内嵌浏览器后端：`webview` / `webengine` / `none`
         *
         * 由 CMake 在**编译期**判定后经编译定义注入，运行期不变，故为 `CONSTANT`
         * QML 依据它选择对应的浏览器实现文件；`none` 时只提供“用系统浏览器打开”
         */
        Q_PROPERTY(QString webBrowserBackend READ web_browser_backend CONSTANT)

        /** 是否具备内嵌浏览器（`webBrowserBackend != "none"`） */
        Q_PROPERTY(bool hasEmbeddedBrowser READ has_embedded_browser CONSTANT)

        /**
         * “从教务导入”的可选入口列表
         *
         * 固定第一项是**打开内置浏览器**（不预设地址，由用户自行前往课表页），
         * 其后是已注册适配器中带 http(s) 入口的项（登录页优先，退回数据地址）
         * 落地页（本地文件路径）与未配置地址的适配器不会出现，避免给出无效入口
         */
        Q_PROPERTY(QVariantList browserEntries READ browser_entries NOTIFY adaptersChanged)

        /** 最近一次网页抓取的摘要（成功为“已抓取 N 字节…”，失败为中文原因） */
        Q_PROPERTY(QString webCaptureSummary READ web_capture_summary NOTIFY webCaptureChanged)

        /** 最近一次抓取的来源地址（用于预览提示与导入留痕） */
        Q_PROPERTY(QString webCaptureSource READ web_capture_source NOTIFY webCaptureChanged)

        /**
         * 注入内嵌浏览器的抓取脚本（页面内执行的 JavaScript）
         *
         * 由 QML 侧在执行 `runJavaScript` 时取用；职责与取舍见 `BrowserCaptureScript.h`
         */
        Q_PROPERTY(QString webCaptureScript READ web_capture_script CONSTANT)

        /** 只注入「抓取课表」悬浮按钮的脚本，不抓取内容；页面加载完成后由 app 层调用 */
        Q_PROPERTY(QString webInjectButtonScript READ web_inject_button_script CONSTANT)

    public:
        /**
         * @param service    课表服务（不持有所有权）；导入结果会通过它加载
         * @param repository 仓库（不持有所有权）；导入成功后落库
         * @param settings   设置门面（不持有所有权）；读写默认目录
         * @param parent     父对象
         */
        ImportExportBridge(ScheduleService* service, IScheduleRepository* repository, AppSettings* settings, QObject* parent = nullptr);

        ~ImportExportBridge() override;

        // ------------------------------------------------------------ 属性读取

        QString default_import_dir() const;
        QString default_export_dir() const;
        QString last_import_dir() const;
        QString last_export_dir() const;
        bool has_pending_preview() const;
        QString preview_summary() const;
        QString preview_format_name() const;
        int preview_new_count() const;
        int preview_duplicate_count() const;
        int preview_conflict_count() const;
        QStringList preview_warnings() const;
        QVariantList preview_conflicts() const;
        QStringList strategy_names() const;
        QStringList format_names() const;
        QStringList adapted_timetable_types() const;
        QString last_export_path() const;
        QString last_export_summary() const;
        QString last_import_summary() const;
        QString last_error() const;
        int progress() const;
        QVariantList adapter_options() const;
        QString adapter_session_status() const;
        QString web_browser_backend() const;
        bool has_embedded_browser() const;
        QVariantList browser_entries() const;
        QString web_capture_summary() const;
        QString web_capture_source() const;
        QString web_capture_script() const;
        QString web_inject_button_script() const;

        /** @return 由 `formatNames()` 下标解析格式；越界返回 JSON */
        Q_INVOKABLE int format_index_of(const QString& machine_name) const;

    public slots:
        /** 重新读取设置中的默认 / 最近目录 */
        void refresh_directories();

        /**
         * @brief 解析文件并生成导入预览（不修改任何数据）
         * @param file_url 由 QML `FileDialog::selectedFile` 提供的 URL
         */
        void preview_import(const QUrl& file_url);

        /**
         * @brief 按指定策略应用当前预览，并把结果加载进服务、写入仓库
         * @param strategy_index `strategyNames()` 的下标（0=合并，1=去重，2=覆盖）
         */
        void apply_import(int strategy_index);

        /** @brief 放弃当前预览 */
        void cancel_import();

        /**
         * @brief 导出到指定目录
         * @param format_index   `formatNames()` 的下标（0=JSON，1=CSV，2=ICS）
         * @param directory_url  由 QML `FolderDialog::selectedFolder` 提供的 URL
         */
        void export_schedule(int format_index, const QUrl& directory_url);

        /** @brief 把默认导入目录设置为给定目录（写入设置） */
        void set_default_import_dir(const QUrl& directory_url);

        /** @brief 把默认导出目录设置为给定目录（写入设置） */
        void set_default_export_dir(const QUrl& directory_url);

        /** @brief 恢复默认导入 / 导出目录为 `Documents/Schedule` */
        void reset_default_directories();

        /**
         * @brief 注册教务适配器仓库（不接管所有权）
         *
         * 适配器由 `app` 层构造并注册；本对象只负责按需触发一次抓取
         */
        void set_adapter_registry(SchoolAdapterRegistry* registry);

        /**
         * @brief 保存某个适配器的数据 / 登录地址（写入设置，供下次启动复用）
         * @param index        适配器下标
         * @param schedule_url 课表数据接口地址（http(s) 或本地文件路径）
         * @param login_url    登录页地址（供 WebView 打开；可为空）
         */
        void save_adapter_endpoints(int index, const QString& schedule_url, const QString& login_url);

        /**
         * @brief 主动触发一次教务适配器导入
         *
         * 抓取成功后与文件导入走完全相同的“预览 → 策略 → 应用”流程
         *
         * @param index         适配器下标
         * @param cookie_header WebView 登录后取得的 Cookie 串；为空表示沿用已保存的会话
         *
         * @note 本方法**不接收也不保存任何密码**；`cookie_header` 只保留在内存中，
         *       可随时通过 `clear_adapter_session()` 擦除
         */
        void import_from_adapter(int index, const QString& cookie_header);

        /** @brief 立即擦除内存中的适配器会话凭证 */
        void clear_adapter_session();

        /**
         * @brief 提交内嵌浏览器抓取到的页面内容并生成导入预览
         *
         * 与文件导入走**完全相同**的流程：格式嗅探（`ImportManager::preview_data`）
         * → 冲突检测 → 预览；用户确认后再调用 `apply_import()`
         *
         * @param page_html  当前页面的 HTML 原文
         * @param source_url 页面地址，仅用于错误信息与导入留痕
         *
         * @note 只接收**页面内容**：不接收也不保存密码，不读取浏览器 Cookie
         */
        void submit_web_capture(const QString& page_html, const QString& source_url);

    signals:
        /** 目录相关设置变化后发出（QML 据此刷新显示） */
        void directoriesChanged();

        /** 导入预览生成或清除后发出 */
        void previewChanged();

        /**
         * @brief 预览完成
         * @param success 是否解析成功
         * @param summary 面向用户的摘要（成功为预览摘要，失败为错误原因）
         */
        void importPreviewReady(bool success, const QString& summary);

        /** 导入结果变化后发出（用于刷新界面文本） */
        void importFinishedChanged();

        /**
         * @brief 导入完成
         * @param success 是否成功
         * @param summary 面向用户的摘要
         */
        void importFinished(bool success, const QString& summary);

        /** 导出结果变化后发出 */
        void exportFinishedChanged();

        /** 适配器列表或会话状态变化后发出 */
        void adaptersChanged();

        /** 网页抓取结果变化后发出 */
        void webCaptureChanged();

        /**
         * @brief 网页抓取完成并已生成预览
         * @param success 是否成功
         * @param message 面向用户的中文摘要（成功为预览摘要，失败为原因）
         */
        void webCaptureFinished(bool success, const QString& message);

        /**
         * @brief 导出完成
         * @param success   是否成功
         * @param summary   面向用户的摘要（成功时包含**实际路径**）
         * @param file_path 实际写入的绝对路径；失败时为空
         */
        void exportFinished(bool success, const QString& summary, const QString& file_path);

        /**
         * @brief 进度变化
         * @param percent 0..100
         * @param message 当前阶段描述
         */
        void progressChanged(int percent, const QString& message);

        /**
         * @brief 发生错误
         * @param message 面向用户的中文错误信息
         */
        void errorOccurred(const QString& message);

    private:
        /** @return 把 `QUrl` 转成本地路径；若是普通路径则原样返回 */
        static QString local_path_of(const QUrl& url);

        /** @return 冲突的 QML 友好表示 */
        static QVariantMap conflict_to_map(const Conflict& conflict);

        /** @brief 设置错误并发出信号 */
        void report_error(const QString& message);

        /** @brief 更新进度并发出信号 */
        void set_progress(int percent, const QString& message);

        /** 课表服务；不持有所有权 */
        ScheduleService* m_service = nullptr;

        /** 仓库；不持有所有权 */
        IScheduleRepository* m_repository = nullptr;

        /** 设置门面；不持有所有权 */
        AppSettings* m_settings = nullptr;

        /** 教务适配器仓库；不持有所有权，可为空（表示未启用适配器） */
        SchoolAdapterRegistry* m_adapter_registry = nullptr;

        /** 适配器会话（**仅内存**，绝不写入数据库） */
        AdapterSession m_adapter_session;

        /** 导入编排器 */
        ImportManager m_import_manager;

        /** 导出编排器 */
        ExportManager m_export_manager;

        /** 待确认的导入预览 */
        ImportPreview m_pending_preview;

        /** 最近一次导出的实际路径 */
        QString m_last_export_path;

        /** 最近一次导出的摘要 */
        QString m_last_export_summary;

        /** 最近一次导入的摘要 */
        QString m_last_import_summary;

        /** 最近一次错误 */
        QString m_last_error;

        /** 最近一次网页抓取的摘要 */
        QString m_web_capture_summary;

        /** 最近一次网页抓取的来源地址 */
        QString m_web_capture_source;

        /** 进度百分比 */
        int m_progress = 0;
    };

} // namespace Schedule
