#pragma once

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
     * @brief 导入导出桥接对象：把 `data` 层的导入导出能力暴露给 QML。
     *
     * **职责边界（重要）**：
     *  - 文件 / 目录选择对话框在 **QML 层**（`QtQuick.Dialogs`），本对象只接收 `QUrl`；
     *  - 数据的解析与落盘在 `data` 层，本对象只做编排、状态缓存与信号转发。
     *
     * **典型交互（信号连接全部在 C++ 侧建立）**：
     *  1. 用户点“导入”按钮 → C++ 打开 QML 的 `FileDialog`；
     *  2. `FileDialog::accepted` → C++ 读取其 `selectedFile` → 调用 `preview_import(url)`；
     *  3. 界面展示预览（冲突数、重复数、提示）→ 用户选择策略；
     *  4. 点“确认导入” → C++ 调用 `apply_import(strategyIndex)`；
     *  5. `import_finished(...)` 由 C++ 侧连接回界面提示。
     *
     * 导出流程同理：`FolderDialog` → `export_schedule(formatIndex, directoryUrl)` →
     * `export_finished(...)`（其中包含**实际写入路径**）。
     */
    class ImportExportBridge : public QObject {
        Q_OBJECT

        /** 默认导入目录（未设置时为 `Documents/Schedule`）。 */
        Q_PROPERTY(QString defaultImportDir READ default_import_dir NOTIFY directoriesChanged)

        /** 默认导出目录（未设置时为 `Documents/Schedule`）。 */
        Q_PROPERTY(QString defaultExportDir READ default_export_dir NOTIFY directoriesChanged)

        /** 最近一次导入使用的目录，用作文件对话框的初始位置。 */
        Q_PROPERTY(QString lastImportDir READ last_import_dir NOTIFY directoriesChanged)

        /** 最近一次导出使用的目录，用作目录对话框的初始位置。 */
        Q_PROPERTY(QString lastExportDir READ last_export_dir NOTIFY directoriesChanged)

        /** 是否已有一个待确认的导入预览。 */
        Q_PROPERTY(bool hasPendingPreview READ has_pending_preview NOTIFY previewChanged)

        /** 预览摘要文本。 */
        Q_PROPERTY(QString previewSummary READ preview_summary NOTIFY previewChanged)

        /** 预览中的文件格式显示名。 */
        Q_PROPERTY(QString previewFormatName READ preview_format_name NOTIFY previewChanged)

        /** 预览中预计新增的课程数。 */
        Q_PROPERTY(int previewNewCount READ preview_new_count NOTIFY previewChanged)

        /** 预览中与现有课表重复的课程数。 */
        Q_PROPERTY(int previewDuplicateCount READ preview_duplicate_count NOTIFY previewChanged)

        /** 预览中“本次导入新引入”的冲突数量。 */
        Q_PROPERTY(int previewConflictCount READ preview_conflict_count NOTIFY previewChanged)

        /** 预览中的非阻断提示列表。 */
        Q_PROPERTY(QStringList previewWarnings READ preview_warnings NOTIFY previewChanged)

        /** 预览中的冲突明细（map 列表，字段见 `conflict_to_map`）。 */
        Q_PROPERTY(QVariantList previewConflicts READ preview_conflicts NOTIFY previewChanged)

        /** 全部导入策略的展示名，供 ComboBox 使用。 */
        Q_PROPERTY(QStringList strategyNames READ strategy_names CONSTANT)

        /** 全部导出格式的展示名，供 ComboBox 使用。 */
        Q_PROPERTY(QStringList formatNames READ format_names CONSTANT)

        /** 最近一次导出的**实际文件路径**（失败时为空）。 */
        Q_PROPERTY(QString lastExportPath READ last_export_path NOTIFY exportFinishedChanged)

        /** 最近一次导出的摘要文本。 */
        Q_PROPERTY(QString lastExportSummary READ last_export_summary NOTIFY exportFinishedChanged)

        /** 最近一次导入的摘要文本。 */
        Q_PROPERTY(QString lastImportSummary READ last_import_summary NOTIFY importFinishedChanged)

        /** 最近一次错误信息（面向用户的中文）。 */
        Q_PROPERTY(QString lastError READ last_error NOTIFY errorOccurred)

        /** 导入 / 导出进度百分比（0..100）。 */
        Q_PROPERTY(int progress READ progress NOTIFY progressChanged)

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
        QString last_export_path() const;
        QString last_export_summary() const;
        QString last_import_summary() const;
        QString last_error() const;
        int progress() const;

        /** @return 由 `formatNames()` 下标解析格式；越界返回 JSON。 */
        Q_INVOKABLE int format_index_of(const QString& machine_name) const;

    public slots:
        /** 重新读取设置中的默认 / 最近目录。 */
        void refresh_directories();

        /**
         * @brief 解析文件并生成导入预览（不修改任何数据）。
         * @param file_url 由 QML `FileDialog::selectedFile` 提供的 URL
         */
        void preview_import(const QUrl& file_url);

        /**
         * @brief 按指定策略应用当前预览，并把结果加载进服务、写入仓库。
         * @param strategy_index `strategyNames()` 的下标（0=合并，1=去重，2=覆盖）
         */
        void apply_import(int strategy_index);

        /** @brief 放弃当前预览。 */
        void cancel_import();

        /**
         * @brief 导出到指定目录。
         * @param format_index   `formatNames()` 的下标（0=JSON，1=CSV，2=ICS）
         * @param directory_url  由 QML `FolderDialog::selectedFolder` 提供的 URL
         */
        void export_schedule(int format_index, const QUrl& directory_url);

        /** @brief 把默认导入目录设置为给定目录（写入设置）。 */
        void set_default_import_dir(const QUrl& directory_url);

        /** @brief 把默认导出目录设置为给定目录（写入设置）。 */
        void set_default_export_dir(const QUrl& directory_url);

        /** @brief 恢复默认导入 / 导出目录为 `Documents/Schedule`。 */
        void reset_default_directories();

    signals:
        /** 目录相关设置变化后发出（QML 据此刷新显示）。 */
        void directoriesChanged();

        /** 导入预览生成或清除后发出。 */
        void previewChanged();

        /**
         * @brief 预览完成。
         * @param success 是否解析成功
         * @param summary 面向用户的摘要（成功为预览摘要，失败为错误原因）
         */
        void importPreviewReady(bool success, const QString& summary);

        /** 导入结果变化后发出（用于刷新界面文本）。 */
        void importFinishedChanged();

        /**
         * @brief 导入完成。
         * @param success 是否成功
         * @param summary 面向用户的摘要
         */
        void importFinished(bool success, const QString& summary);

        /** 导出结果变化后发出。 */
        void exportFinishedChanged();

        /**
         * @brief 导出完成。
         * @param success   是否成功
         * @param summary   面向用户的摘要（成功时包含**实际路径**）
         * @param file_path 实际写入的绝对路径；失败时为空
         */
        void exportFinished(bool success, const QString& summary, const QString& file_path);

        /**
         * @brief 进度变化。
         * @param percent 0..100
         * @param message 当前阶段描述
         */
        void progressChanged(int percent, const QString& message);

        /**
         * @brief 发生错误。
         * @param message 面向用户的中文错误信息
         */
        void errorOccurred(const QString& message);

    private:
        /** @return 把 `QUrl` 转成本地路径；若是普通路径则原样返回。 */
        static QString local_path_of(const QUrl& url);

        /** @return 冲突的 QML 友好表示。 */
        static QVariantMap conflict_to_map(const Conflict& conflict);

        /** @brief 设置错误并发出信号。 */
        void report_error(const QString& message);

        /** @brief 更新进度并发出信号。 */
        void set_progress(int percent, const QString& message);

        /** 课表服务；不持有所有权。 */
        ScheduleService* m_service = nullptr;

        /** 仓库；不持有所有权。 */
        IScheduleRepository* m_repository = nullptr;

        /** 设置门面；不持有所有权。 */
        AppSettings* m_settings = nullptr;

        /** 导入编排器。 */
        ImportManager m_import_manager;

        /** 导出编排器。 */
        ExportManager m_export_manager;

        /** 待确认的导入预览。 */
        ImportPreview m_pending_preview;

        /** 最近一次导出的实际路径。 */
        QString m_last_export_path;

        /** 最近一次导出的摘要。 */
        QString m_last_export_summary;

        /** 最近一次导入的摘要。 */
        QString m_last_import_summary;

        /** 最近一次错误。 */
        QString m_last_error;

        /** 进度百分比。 */
        int m_progress = 0;
    };

} // namespace Schedule
