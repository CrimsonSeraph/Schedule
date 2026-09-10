#include "engine/ImportExportBridge.h"

#include "data/SettingsKeys.h"

#include <QDir>
#include <QFileInfo>

namespace Schedule {

    namespace {

        /** 导出格式下拉框的顺序：与 `ExportManager` 的内置注册顺序保持一致。 */
        const ScheduleFormat EXPORT_FORMATS[] = {ScheduleFormat::Json, ScheduleFormat::Csv, ScheduleFormat::Ics};

        constexpr int EXPORT_FORMAT_COUNT = int(sizeof(EXPORT_FORMATS) / sizeof(EXPORT_FORMATS[0]));

    } // namespace

    ImportExportBridge::ImportExportBridge(ScheduleService* service,
        IScheduleRepository* repository,
        AppSettings* settings,
        QObject* parent)
        : QObject(parent)
        , m_service(service)
        , m_repository(repository)
        , m_settings(settings) {
    }

    ImportExportBridge::~ImportExportBridge() = default;

    // ---------------------------------------------------------------- 属性读取

    QString ImportExportBridge::default_import_dir() const {
        return m_settings ? m_settings->default_import_dir() : AppSettings::fallback_directory();
    }

    QString ImportExportBridge::default_export_dir() const {
        return m_settings ? m_settings->default_export_dir() : AppSettings::fallback_directory();
    }

    QString ImportExportBridge::last_import_dir() const {
        return m_settings ? m_settings->last_import_dir() : AppSettings::fallback_directory();
    }

    QString ImportExportBridge::last_export_dir() const {
        return m_settings ? m_settings->last_export_dir() : AppSettings::fallback_directory();
    }

    bool ImportExportBridge::has_pending_preview() const {
        return m_pending_preview.is_valid;
    }

    QString ImportExportBridge::preview_summary() const {
        return m_pending_preview.summary();
    }

    QString ImportExportBridge::preview_format_name() const {
        return m_pending_preview.is_valid ? format_display_name(m_pending_preview.format) : QString();
    }

    int ImportExportBridge::preview_new_count() const {
        return m_pending_preview.new_course_count;
    }

    int ImportExportBridge::preview_duplicate_count() const {
        return m_pending_preview.duplicate_count;
    }

    int ImportExportBridge::preview_conflict_count() const {
        return static_cast<int>(m_pending_preview.conflicts.size());
    }

    QStringList ImportExportBridge::preview_warnings() const {
        return m_pending_preview.warnings;
    }

    QVariantList ImportExportBridge::preview_conflicts() const {
        QVariantList list;
        for (const Conflict& conflict : m_pending_preview.conflicts) {
            list.append(conflict_to_map(conflict));
        }
        return list;
    }

    QStringList ImportExportBridge::strategy_names() const {
        return ImportManager::strategy_names();
    }

    QStringList ImportExportBridge::format_names() const {
        QStringList names;
        for (int i = 0; i < EXPORT_FORMAT_COUNT; ++i) {
            names.append(format_display_name(EXPORT_FORMATS[i]));
        }
        return names;
    }

    QString ImportExportBridge::last_export_path() const {
        return m_last_export_path;
    }

    QString ImportExportBridge::last_export_summary() const {
        return m_last_export_summary;
    }

    QString ImportExportBridge::last_import_summary() const {
        return m_last_import_summary;
    }

    QString ImportExportBridge::last_error() const {
        return m_last_error;
    }

    int ImportExportBridge::progress() const {
        return m_progress;
    }

    int ImportExportBridge::format_index_of(const QString& machine_name) const {
        const ScheduleFormat format = format_from_string(machine_name);
        for (int i = 0; i < EXPORT_FORMAT_COUNT; ++i) {
            if (EXPORT_FORMATS[i] == format) {
                return i;
            }
        }
        return 0;
    }

    // -------------------------------------------------------------------- 工具

    QString ImportExportBridge::local_path_of(const QUrl& url) {
        if (url.isEmpty()) {
            return QString();
        }
        // QML 的 FileDialog / FolderDialog 给出的是 file:// URL；
        // 也兼容直接传入本地路径（例如设置页里手工输入的目录）。
        const QString local = url.toLocalFile();
        return local.isEmpty() ? url.toString() : local;
    }

    QVariantMap ImportExportBridge::conflict_to_map(const Conflict& conflict) {
        QVariantMap map;
        map.insert(QStringLiteral("type"), static_cast<int>(conflict.type));
        map.insert(QStringLiteral("typeName"), conflict.type_name());
        map.insert(QStringLiteral("message"), conflict.message);
        map.insert(QStringLiteral("dayOfWeek"), conflict.day_of_week);
        map.insert(QStringLiteral("week"), conflict.week);
        map.insert(QStringLiteral("courseIdA"), conflict.course_id_a);
        map.insert(QStringLiteral("courseIdB"), conflict.course_id_b);
        map.insert(QStringLiteral("blocking"), conflict.is_blocking());
        return map;
    }

    void ImportExportBridge::report_error(const QString& message) {
        m_last_error = message;
        emit errorOccurred(message);
    }

    void ImportExportBridge::set_progress(int percent, const QString& message) {
        m_progress = qBound(0, percent, 100);
        emit progressChanged(m_progress, message);
    }

    // ---------------------------------------------------------------- 目录设置

    void ImportExportBridge::refresh_directories() {
        emit directoriesChanged();
    }

    void ImportExportBridge::set_default_import_dir(const QUrl& directory_url) {
        const QString path = local_path_of(directory_url);
        if (path.isEmpty()) {
            report_error(QStringLiteral("导入目录为空"));
            return;
        }
        QString error;
        if (!m_settings || !m_settings->set_default_import_dir(path, &error)) {
            report_error(error.isEmpty() ? QStringLiteral("设置保存失败（存储尚未就绪）") : error);
            return;
        }
        emit directoriesChanged();
    }

    void ImportExportBridge::set_default_export_dir(const QUrl& directory_url) {
        const QString path = local_path_of(directory_url);
        if (path.isEmpty()) {
            report_error(QStringLiteral("导出目录为空"));
            return;
        }
        QString error;
        if (!m_settings || !m_settings->set_default_export_dir(path, &error)) {
            report_error(error.isEmpty() ? QStringLiteral("设置保存失败（存储尚未就绪）") : error);
            return;
        }
        emit directoriesChanged();
    }

    void ImportExportBridge::reset_default_directories() {
        if (!m_settings) {
            report_error(QStringLiteral("设置尚未就绪"));
            return;
        }
        // 写入空串即表示“回退到 Documents/Schedule”
        QString error;
        m_settings->set_default_import_dir(QString(), &error);
        m_settings->set_default_export_dir(QString(), &error);
        emit directoriesChanged();
    }

    // -------------------------------------------------------------------- 导入

    void ImportExportBridge::preview_import(const QUrl& file_url) {
        const QString path = local_path_of(file_url);
        if (path.isEmpty()) {
            report_error(QStringLiteral("未选择导入文件"));
            return;
        }

        set_progress(10, QStringLiteral("正在解析文件…"));
        if (m_service) {
            m_pending_preview = m_import_manager.preview(path, m_service->snapshot());
        }
        else {
            m_pending_preview = ImportPreview();
            m_pending_preview.error_message = QStringLiteral("课表服务尚未就绪");
        }
        set_progress(100, QStringLiteral("解析完成"));

        // 记住用户实际使用的目录，下次打开对话框时回到这里
        if (m_settings && m_pending_preview.is_valid) {
            m_settings->set_last_import_dir(QFileInfo(path).absolutePath());
            emit directoriesChanged();
        }

        emit previewChanged();
        emit importPreviewReady(m_pending_preview.is_valid, m_pending_preview.summary());
        if (!m_pending_preview.is_valid) {
            report_error(m_pending_preview.error_message);
        }
    }

    void ImportExportBridge::apply_import(int strategy_index) {
        if (!m_pending_preview.is_valid) {
            report_error(QStringLiteral("没有可用的导入预览，请先选择文件"));
            return;
        }
        if (!m_service) {
            report_error(QStringLiteral("课表服务尚未就绪"));
            return;
        }

        const ImportStrategy strategy = ImportManager::strategy_at(strategy_index);
        set_progress(20, QStringLiteral("正在导入…"));

        ScheduleSnapshot snapshot = m_service->snapshot();
        const ImportResult result = m_import_manager.apply(m_pending_preview, strategy, &snapshot);
        if (!result.success) {
            set_progress(0, QString());
            report_error(result.error_message);
            emit importFinished(false, result.error_message);
            return;
        }

        // 先让服务加载新数据（会触发 courses_changed，模型随之刷新），再落库
        m_service->load_snapshot(snapshot);
        set_progress(70, QStringLiteral("正在保存…"));

        QString save_error;
        if (m_repository && !m_repository->save_snapshot(snapshot, &save_error)) {
            set_progress(0, QString());
            m_last_import_summary = result.summary();
            emit importFinishedChanged();
            report_error(QStringLiteral("导入已生效，但保存失败：%1").arg(save_error));
            emit importFinished(false, m_last_import_summary);
            return;
        }

        // 记录导入来源（纯本地留痕：文件路径 + 格式 + 课程数）
        if (m_repository) {
            ImportSource source;
            source.file_path = m_pending_preview.source_path;
            source.format = format_to_string(m_pending_preview.format);
            source.course_count = result.imported_count + result.updated_count;
            source.note = strategy_display_name(strategy);
            m_repository->add_import_source(source);
        }

        m_last_import_summary = result.summary();
        m_pending_preview = ImportPreview();
        set_progress(100, QStringLiteral("导入完成"));

        emit previewChanged();
        emit importFinishedChanged();
        emit importFinished(true, m_last_import_summary);
    }

    void ImportExportBridge::cancel_import() {
        m_pending_preview = ImportPreview();
        emit previewChanged();
    }

    // -------------------------------------------------------------------- 导出

    void ImportExportBridge::export_schedule(int format_index, const QUrl& directory_url) {
        if (!m_service) {
            report_error(QStringLiteral("课表服务尚未就绪"));
            return;
        }
        if (!m_service->has_semester()) {
            report_error(QStringLiteral("尚未设置学期，无法导出课表"));
            return;
        }

        const QString directory = local_path_of(directory_url).isEmpty() ? default_export_dir() : local_path_of(directory_url);
        const ScheduleFormat format = (format_index >= 0 && format_index < EXPORT_FORMAT_COUNT)
                                          ? EXPORT_FORMATS[format_index]
                                          : ScheduleFormat::Json;

        set_progress(20, QStringLiteral("正在生成文件…"));
        const ExportResult result = m_export_manager.export_to_directory(directory, m_service->snapshot(), format);

        if (!result.success) {
            set_progress(0, QString());
            m_last_export_path.clear();
            m_last_export_summary = result.summary();
            emit exportFinishedChanged();
            report_error(result.error_message);
            emit exportFinished(false, m_last_export_summary, QString());
            return;
        }

        // 记住实际使用的目录，下次打开对话框时回到这里
        if (m_settings) {
            m_settings->set_last_export_dir(QFileInfo(result.file_path).absolutePath());
            emit directoriesChanged();
        }

        m_last_export_path = result.file_path;
        m_last_export_summary = result.summary();
        set_progress(100, QStringLiteral("导出完成"));

        emit exportFinishedChanged();
        emit exportFinished(true, m_last_export_summary, m_last_export_path);
    }

} // namespace Schedule
