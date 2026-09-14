#include "data/import_export/ImportExportTypes.h"

#include "core/service/ConflictDetector.h"
#include "data/import_export/academic_affairs/CharsetUtil.h"

#include <QFileInfo>

namespace Schedule {

    namespace {

        /** 格式机器名与显示名的对应表；新增格式只需在此追加一行。 */
        struct FormatEntry {
            ScheduleFormat format;
            const char* machine_name;
            const char* display_name;
            /** 逗号分隔的扩展名；**第一个是推荐扩展名**（导出文件名与 `file_extension()` 用它）。 */
            const char* extensions;
            /** 本应用能否导出该格式；只导入格式（教务系统导出页）为 false。 */
            bool exportable;
        };

        const FormatEntry FORMATS[] = {
            {ScheduleFormat::Json, "json", "课表 JSON", "json", true},
            {ScheduleFormat::Csv, "csv", "表格 CSV", "csv", true},
            {ScheduleFormat::Ics, "ics", "日历 ICS", "ics", true},
            {ScheduleFormat::ZhengfangHtml, "zhengfang-html", "正方教务课表", "xls", false},
            {ScheduleFormat::EcjtuTimetable, "ecjtu-timetable", "华东交大教务课表", "docx,doc", false},
        };

        /** 嗅探教务系统页面时扫描的最大字节数（标志串可能位于内嵌 <script>，比较靠后）。 */
        constexpr int MARKUP_SNIFF_LIMIT = 256 * 1024;

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
                // 推荐扩展名排在列表最前面
                return QString::fromLatin1(entry.extensions).section(QLatin1Char(','), 0, 0);
            }
        }
        return QString();
    }

    QStringList supported_file_extensions() {
        QStringList extensions;
        for (const FormatEntry& entry : FORMATS) {
            // 一个格式可以有多个可导入扩展名（如 .docx 与 .doc）
            for (const QString& name : QString::fromLatin1(entry.extensions).split(QLatin1Char(','), Qt::SkipEmptyParts)) {
                extensions.append(QStringLiteral(".") + name);
            }
        }
        return extensions;
    }

    QStringList export_file_extensions() {
        QStringList extensions;
        for (const FormatEntry& entry : FORMATS) {
            if (entry.exportable) {
                // 可导出格式只有一个扩展名，直接取整个字段即可
                extensions.append(QStringLiteral(".") + QLatin1String(entry.extensions));
            }
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
        if (suffix == QStringLiteral("xls") || suffix == QStringLiteral("html") || suffix == QStringLiteral("htm")) {
            // 教务系统「导出」的课表多半以 .xls 命名，内容其实是 HTML
            return ScheduleFormat::ZhengfangHtml;
        }
        if (suffix == QStringLiteral("docx") || suffix == QStringLiteral("doc")) {
            // 华东交大教务综合管理系统「导出」的课表是 Word 表格，扩展名是 .doc / .docx
            return ScheduleFormat::EcjtuTimetable;
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

        // 正方教务：Excel 兼容的 HTML 导出页。标志串位于内嵌 <script> 中，
        // 可能在文件偏后位置，因此在更长的前缀上做一次扫描。
        // 判定放在 CSV 之前：HTML 首行之外的逗号不应把页面误判成表格。
        const QByteArray sample = trimmed.left(MARKUP_SNIFF_LIMIT);
        const bool has_zhengfang_table = sample.contains("manualArrangeCourseTable") || sample.contains("courseTableForStd");
        const bool has_zhengfang_script = sample.contains("TaskActivity(") && sample.contains("CourseTable(");
        if (has_zhengfang_table || has_zhengfang_script) {
            return ScheduleFormat::ZhengfangHtml;
        }

        // OOXML 包（.docx / .xlsx / .pptx …）：zip 本地文件头魔数。
        // 具体是不是课表由导入器解析 word/document.xml 后判断，这里只认容器。
        if (trimmed.startsWith(QByteArray::fromHex("504B0304"))) {
            return ScheduleFormat::EcjtuTimetable;
        }

        // Word 版式 HTML（教务系统「导出为 .doc」的常见形态）与课表页面。
        // 这类内容可能是 GBK，所以先解码再找结构特征，而不是硬匹配字节。
        // 判定放在 CSV 之前：HTML 表格里的逗号不该把页面误判成表格文件；
        // 同时要求「节次」与两个以上具体星期名，避免把含「周一」列的 CSV 认成网页。
        if (sample.contains('<')) {
            const QString head = decode_html_bytes(sample, nullptr);
            const bool has_slot_header = head.contains(QStringLiteral("节次"));
            const bool has_day_headers = head.contains(QStringLiteral("星期一")) && (head.contains(QStringLiteral("星期二")) || head.contains(QStringLiteral("星期三")));
            if (has_slot_header && has_day_headers) {
                return ScheduleFormat::EcjtuTimetable;
            }
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
