#pragma once

#include "core/model/Conflict.h"
#include "core/model/Course.h"
#include "core/model/ScheduleSnapshot.h"
#include "core/model/Semester.h"
#include "core/model/TimeSlot.h"

#include <QList>
#include <QString>
#include <QStringList>

namespace Schedule {

    /**
     * @brief 课表文件格式。
     *
     * 新增格式时只需在 `format_to_string()` / `format_from_string()` /
     * `file_extension()` 中补齐映射，并实现 `IScheduleImporter` / `IScheduleExporter`。
     */
    enum class ScheduleFormat {
        Unknown, ///< 未识别
        Json,    ///< 本应用的 JSON 文档（无损，含作息表与周次位图）
        Csv,     ///< 表格（Excel 友好，一行一个上课时间段）
        Ics,     ///< iCalendar（可与系统日历互操作）
    };

    /** @return 格式的小写机器名（`json` / `csv` / `ics`），用于设置与日志。 */
    QString format_to_string(ScheduleFormat format);

    /** @return 由机器名解析格式；无法识别返回 `ScheduleFormat::Unknown`。 */
    ScheduleFormat format_from_string(const QString& text);

    /** @return 格式的展示名（中文）。 */
    QString format_display_name(ScheduleFormat format);

    /** @return 格式的推荐扩展名（不含点），如 `json`；未识别返回空串。 */
    QString file_extension(ScheduleFormat format);

    /** @return 全部支持格式的推荐扩展名（含点），用于文件对话框过滤器。 */
    QStringList supported_file_extensions();

    /**
     * @brief 由文件扩展名推断格式。
     * @return 无法识别返回 `ScheduleFormat::Unknown`
     */
    ScheduleFormat format_from_extension(const QString& file_path);

    /**
     * @brief 由文件内容嗅探格式（扩展名不可信时使用）。
     *
     * 判定顺序：JSON（首个非空白字符为 `{`）→ iCalendar（含 `BEGIN:VCALENDAR`）
     * → CSV（首行含分隔符或中文表头关键字）→ Unknown。
     */
    ScheduleFormat format_from_content(const QByteArray& data);

    /**
     * @brief 导入策略：把文件中的课程合入现有课表的方式。
     */
    enum class ImportStrategy {
        Merge,          ///< 合并：按 id 更新已存在的课程，其余新增（默认）
        SkipDuplicates, ///< 合并并去重：同名或同 id 的课程跳过，其余新增
        Overwrite,      ///< 覆盖：清空现有课程后写入文件内容
    };

    /** @return 策略的中文展示名。 */
    QString strategy_display_name(ImportStrategy strategy);

    /**
     * @brief 导入预览：解析外部文件后的**只读**结果，供界面展示后再决定是否落库。
     *
     * 预览阶段不修改任何数据；用户确认后才调用 `ImportManager::apply()`。
     */
    struct ImportPreview {
        /** 是否为有效预览（解析成功）。 */
        bool is_valid = false;

        /** 识别到的格式。 */
        ScheduleFormat format = ScheduleFormat::Unknown;

        /** 源文件路径；剪贴板 / 分享码导入时可为逻辑标识（如 `clipboard://`）。 */
        QString source_path;

        /** 文件中的学期（JSON 提供；CSV / ICS 为按内容合成的学期）。 */
        Semester semester;

        /** 文件中的作息表（可能为空，表示需要沿用当前作息表）。 */
        QList<TimeSlot> time_slots;

        /** 文件中的课程。 */
        QList<Course> courses;

        /** 与当前课表 / 文件自身有关的问题（冲突、越界等）。 */
        QList<Conflict> conflicts;

        /** 与现有课表重复（同 id 或同名同代码）的课程数量。 */
        int duplicate_count = 0;

        /** 预计新增的课程数量。 */
        int new_course_count = 0;

        /** 非阻断性提示（如“文件未包含作息表，将沿用当前作息表”）。 */
        QStringList warnings;

        /** 解析失败时的中文原因。 */
        QString error_message;

        /** @return 是否包含阻断性问题（需要用户确认或修正）。 */
        bool has_blocking_conflict() const;

        /** @return 摘要文本，形如“JSON · 12 门课程 · 2 条冲突”。 */
        QString summary() const;
    };

    /**
     * @brief 导入结果：`apply()` 之后的统计信息。
     */
    struct ImportResult {
        /** 是否成功。 */
        bool success = false;

        /** 实际使用的策略。 */
        ImportStrategy strategy = ImportStrategy::Merge;

        /** 新增课程数。 */
        int imported_count = 0;

        /** 更新（覆盖同 id 课程）数量。 */
        int updated_count = 0;

        /** 因重复而跳过的数量。 */
        int skipped_count = 0;

        /** 导入后课表中仍然存在的问题。 */
        QList<Conflict> conflicts;

        /** 导入所使用的学期。 */
        Semester semester;

        /** 失败时的中文原因。 */
        QString error_message;

        /** @return 面向用户的摘要文本。 */
        QString summary() const;
    };

    /**
     * @brief 导出结果：包含**实际写入路径**，供界面原样提示用户。
     */
    struct ExportResult {
        /** 是否成功。 */
        bool success = false;

        /** 实际写入的绝对路径；失败时为空。 */
        QString file_path;

        /** 使用的格式。 */
        ScheduleFormat format = ScheduleFormat::Unknown;

        /** 导出的课程数量。 */
        int course_count = 0;

        /** 文件字节数；失败时为 0。 */
        qint64 bytes = 0;

        /** 失败时的中文原因。 */
        QString error_message;

        /** @return 面向用户的摘要文本（含实际路径）。 */
        QString summary() const;
    };

} // namespace Schedule
