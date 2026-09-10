#include "data/import_export/ImportExportTypes.h"

#include "core/service/ConflictDetector.h"

#include <QFileInfo>

namespace Schedule {

    namespace {

        /** 格式机器名与显示名的对应表；新增格式只需在此追加一行。 */
        struct FormatEntry {
            ScheduleFormat format;
            const char* machine_name;
            const char* display_name;
            const char* extension;
        };

        const FormatEntry FORMATS[] = {
            {ScheduleFormat::Json, "json", "课表 JSON", "json"},
            {ScheduleFormat::Csv, "csv", "表格 CSV", "csv"},
            {ScheduleFormat::Ics, "ics", "日历 ICS", "ics"},
        };

    } // namespace

    QString format_to_string(ScheduleFormat format) {
        for (const FormatEntry& entry : FORMATS) {
            if (entry.format == format) {
                return QString::fromLatin1(entry.machine_name);
            }
        }
        return QStringLiteral("unknown");
    }

    ScheduleFormat format_from_string(const QString& text) {
        const QString normalised = text.trimmed().toLower();
        for (const FormatEntry& entry : FORMATS) {
            if (normalised == QLatin1String(entry.machine_name)) {
                return entry.format;
            }
        }
        return ScheduleFormat::Unknown;
    }

    QString format_display_name(ScheduleFormat format) {
        for (const FormatEntry& entry : FORMATS) {
            if (entry.format == format) {
                return QString::fromUtf8(entry.display_name);
            }
        }
        return QStringLiteral("未知格式");
    }

    QString file_extension(ScheduleFormat format) {
        for (const FormatEntry& entry : FORMATS) {
            if (entry.format == format) {
                return QString::fromLatin1(entry.extension);
            }
        }
        return QString();
    }

    QStringList supported_file_extensions() {
        QStringList extensions;
        for (const FormatEntry& entry : FORMATS) {
            extensions.append(QStringLiteral(".") + QLatin1String(entry.extension));
        }
        return extensions;
    }

    ScheduleFormat format_from_extension(const QString& file_path) {
        const QString suffix = QFileInfo(file_path).suffix().toLower();
        if (suffix == QStringLiteral("json")) {
            return ScheduleFormat::Json;
        }
        if (suffix == QStringLiteral("csv") || suffix == QStringLiteral("txt")) {
            // .txt 常见于用户手工整理后另存的课表，内容嗅探还会再确认一次
            return ScheduleFormat::Csv;
        }
        if (suffix == QStringLiteral("ics") || suffix == QStringLiteral("ical") || suffix == QStringLiteral("ifb")) {
            return ScheduleFormat::Ics;
        }
        return ScheduleFormat::Unknown;
    }

    ScheduleFormat format_from_content(const QByteArray& data) {
        QByteArray trimmed = data;
        // 去掉 UTF-8 BOM 与行首空白后再判定
        const QByteArray bom = QByteArray::fromHex("EFBBBF");
        if (trimmed.startsWith(bom)) {
            trimmed.remove(0, bom.size());
        }
        while (!trimmed.isEmpty() && (trimmed.at(0) == ' ' || trimmed.at(0) == '\r' || trimmed.at(0) == '\n' || trimmed.at(0) == '\t')) {
            trimmed.remove(0, 1);
        }
        if (trimmed.isEmpty()) {
            return ScheduleFormat::Unknown;
        }

        if (trimmed.startsWith('{')) {
            return ScheduleFormat::Json;
        }

        const QByteArray upper = trimmed.left(4096).toUpper();
        if (upper.contains("BEGIN:VCALENDAR") || upper.contains("BEGIN:VEVENT")) {
            return ScheduleFormat::Ics;
        }

        // CSV：首个非空行包含分隔符或多个字段关键字
        const QByteArray first_line = trimmed.left(trimmed.indexOf('\n') < 0 ? trimmed.size() : trimmed.indexOf('\n'));
        const QString header = QString::fromUtf8(first_line);
        if (header.contains(QLatin1Char(',')) || header.contains(QLatin1Char(';')) || header.contains(QLatin1Char('\t'))) {
            return ScheduleFormat::Csv;
        }
        if (header.contains(QStringLiteral("课程")) || header.contains(QStringLiteral("星期")) || header.contains(QStringLiteral("周次"))) {
            return ScheduleFormat::Csv;
        }

        return ScheduleFormat::Unknown;
    }

    QString strategy_display_name(ImportStrategy strategy) {
        switch (strategy) {
        case ImportStrategy::Merge:
            return QStringLiteral("合并（同 id 更新，其余新增）");
        case ImportStrategy::SkipDuplicates:
            return QStringLiteral("合并并去重（重复课程跳过）");
        case ImportStrategy::Overwrite:
            return QStringLiteral("覆盖（清空后写入文件内容）");
        }
        return QStringLiteral("未知策略");
    }

    bool ImportPreview::has_blocking_conflict() const {
        return ConflictDetector::has_blocking_conflict(conflicts);
    }

    QString ImportPreview::summary() const {
        if (!is_valid) {
            return QStringLiteral("解析失败：%1").arg(error_message);
        }

        QString text = QStringLiteral("%1 · 共 %2 门课程").arg(format_display_name(format)).arg(courses.size());
        if (new_course_count > 0 || duplicate_count > 0) {
            text += QStringLiteral("（新增 %1，重复 %2）").arg(new_course_count).arg(duplicate_count);
        }
        if (!conflicts.isEmpty()) {
            text += QStringLiteral(" · %1 条冲突").arg(conflicts.size());
        }
        if (!warnings.isEmpty()) {
            text += QStringLiteral(" · %1 条提示").arg(warnings.size());
        }
        return text;
    }

    QString ImportResult::summary() const {
        if (!success) {
            return QStringLiteral("导入失败：%1").arg(error_message);
        }
        return QStringLiteral("已按“%1”导入：新增 %2 门，更新 %3 门，跳过 %4 门")
            .arg(strategy_display_name(strategy))
            .arg(imported_count)
            .arg(updated_count)
            .arg(skipped_count);
    }

    QString ExportResult::summary() const {
        if (!success) {
            return QStringLiteral("导出失败：%1").arg(error_message);
        }
        return QStringLiteral("已导出 %1 门课程到 %2").arg(course_count).arg(file_path);
    }

} // namespace Schedule
