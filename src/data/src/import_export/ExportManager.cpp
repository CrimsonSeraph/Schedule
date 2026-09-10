#include "data/import_export/ExportManager.h"

#include "data/AppSettings.h"
#include "data/import_export/CsvScheduleIo.h"
#include "data/import_export/IcsScheduleIo.h"
#include "data/import_export/JsonScheduleIo.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

namespace Schedule {

    namespace {

        /** @brief 统一设置错误。 */
        bool fail(QString* error_message, const QString& message) {
            if (error_message) {
                *error_message = message;
            }
            return false;
        }

    } // namespace

    ExportManager::ExportManager() {
        register_exporter(std::make_unique<JsonScheduleIo>());
        register_exporter(std::make_unique<CsvScheduleIo>());
        register_exporter(std::make_unique<IcsScheduleIo>());
    }

    ExportManager::~ExportManager() = default;

    void ExportManager::register_exporter(std::unique_ptr<IScheduleExporter> exporter) {
        if (!exporter) {
            return;
        }
        const ScheduleFormat format = exporter->format();
        for (auto& existing : m_exporters) {
            if (existing && existing->format() == format) {
                existing = std::move(exporter);
                return;
            }
        }
        m_exporters.push_back(std::move(exporter));
    }

    QList<ScheduleFormat> ExportManager::supported_formats() const {
        QList<ScheduleFormat> formats;
        for (const auto& exporter : m_exporters) {
            if (exporter) {
                formats.append(exporter->format());
            }
        }
        return formats;
    }

    const IScheduleExporter* ExportManager::exporter_for(ScheduleFormat format) const {
        for (const auto& exporter : m_exporters) {
            if (exporter && exporter->format() == format) {
                return exporter.get();
            }
        }
        return nullptr;
    }

    QString ExportManager::sanitize_file_component(const QString& text) {
        QString result = text.trimmed();

        // Windows 保留字符 + 路径分隔符 + 控制字符统一替换为下划线
        static const QRegularExpression invalid(QStringLiteral("[\\\\/:*?\"<>|\\x00-\\x1F]"));
        result.replace(invalid, QStringLiteral("_"));

        // 空白（含全角空格）压缩为单个下划线，避免文件名中出现连续空格
        static const QRegularExpression whitespace(QStringLiteral("[\\s\\x{3000}]+"));
        result.replace(whitespace, QStringLiteral("_"));

        // 去掉首尾的下划线与点：Windows 不允许文件名以点结尾
        while (result.startsWith(QLatin1Char('_')) || result.startsWith(QLatin1Char('.'))) {
            result.remove(0, 1);
        }
        while (result.endsWith(QLatin1Char('_')) || result.endsWith(QLatin1Char('.'))) {
            result.chop(1);
        }

        // 限制长度，避免超出文件系统上限（保留时间戳等其它片段的空间）
        if (result.size() > 60) {
            result = result.left(60);
        }
        return result;
    }

    QString ExportManager::suggested_file_name(const QString& semester_name, ScheduleFormat format, const QDateTime& now) {
        const QString extension = file_extension(format);
        const QString timestamp = now.toString(QStringLiteral("yyyyMMdd_HHmmss"));
        const QString component = sanitize_file_component(semester_name);

        if (extension.isEmpty()) {
            return QString();
        }
        if (component.isEmpty()) {
            return QStringLiteral("Schedule_%1.%2").arg(timestamp, extension);
        }
        return QStringLiteral("Schedule_%1_%2.%3").arg(component, timestamp, extension);
    }

    QString ExportManager::file_dialog_filter() {
        QStringList patterns;
        for (const QString& extension : supported_file_extensions()) {
            patterns.append(QStringLiteral("*") + extension);
        }
        return patterns.join(QLatin1Char(' '));
    }

    QByteArray ExportManager::serialize(const ScheduleSnapshot& snapshot, ScheduleFormat format, QString* error_message) const {
        const IScheduleExporter* exporter = exporter_for(format);
        if (!exporter) {
            fail(error_message, QStringLiteral("没有可用的 %1 导出器").arg(format_display_name(format)));
            return QByteArray();
        }
        return exporter->serialize(snapshot, error_message);
    }

    ExportResult ExportManager::export_to_file(const QString& file_path, const ScheduleSnapshot& snapshot, ScheduleFormat format) const {
        ExportResult result;
        result.format = format;
        result.course_count = static_cast<int>(snapshot.courses.size());

        if (file_path.isEmpty()) {
            result.error_message = QStringLiteral("导出路径为空");
            return result;
        }

        const IScheduleExporter* exporter = exporter_for(format);
        if (!exporter) {
            result.error_message = QStringLiteral("不支持导出为 %1").arg(format_display_name(format));
            return result;
        }

        QString error;
        if (!exporter->write(file_path, snapshot, &error)) {
            result.error_message = error;
            return result;
        }

        const QFileInfo info(file_path);
        result.success = true;
        result.file_path = info.absoluteFilePath();
        result.bytes = info.size();
        return result;
    }

    ExportResult ExportManager::export_to_directory(const QString& directory,
        const ScheduleSnapshot& snapshot,
        ScheduleFormat format,
        const QString& base_name) const {
        ExportResult result;
        result.format = format;

        if (directory.isEmpty()) {
            result.error_message = QStringLiteral("导出目录为空");
            return result;
        }

        // 目录可能尚不存在（用户首次导出），由数据层统一创建
        QString directory_error;
        const QString resolved_directory = AppSettings::ensure_directory(directory, &directory_error);
        if (resolved_directory.isEmpty()) {
            result.error_message = directory_error;
            return result;
        }

        const QString file_name = base_name.isEmpty()
                                      ? suggested_file_name(snapshot.semester.name, format)
                                      : base_name;
        if (file_name.isEmpty()) {
            result.error_message = QStringLiteral("无法为 %1 生成文件名").arg(format_display_name(format));
            return result;
        }

        return export_to_file(QDir(resolved_directory).filePath(file_name), snapshot, format);
    }

} // namespace Schedule
