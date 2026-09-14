#include "data/import_export/academic_affairs/EcjtuTimetableIo.h"

#include "core/model/Course.h"
#include "core/model/CourseSession.h"
#include "core/model/Semester.h"
#include "core/model/TimeSlot.h"
#include "core/model/WeekMask.h"
#include "core/service/WeekCalculator.h"
#include "data/import_export/ImportExportTypes.h"
#include "data/import_export/academic_affairs/DocConvertUtil.h"
#include "data/import_export/academic_affairs/WordTableReader.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QList>
#include <QMap>
#include <QRegularExpression>
#include <QStringList>
#include <QTemporaryDir>

#include <algorithm>

namespace Schedule {

    namespace {

        /** 未识别到节次区间时的默认节次数（该校作息为 12 节）。 */
        constexpr int DEFAULT_UNIT_COUNT = 12;

        /** 一次导入允许出现的“无法解析的条目”上限：超过就整体失败，避免只报一小部分。 */
        constexpr int MAX_REPORTED_PROBLEMS = 5;

        /** 星期名 → 1..7。 */
        const QHash<QString, int>& day_names() {
            static const QHash<QString, int> names = {
                {QStringLiteral("星期一"), 1},
                {QStringLiteral("周一"), 1},
                {QStringLiteral("星期二"), 2},
                {QStringLiteral("周二"), 2},
                {QStringLiteral("星期三"), 3},
                {QStringLiteral("周三"), 3},
                {QStringLiteral("星期四"), 4},
                {QStringLiteral("周四"), 4},
                {QStringLiteral("星期五"), 5},
                {QStringLiteral("周五"), 5},
                {QStringLiteral("星期六"), 6},
                {QStringLiteral("周六"), 6},
                {QStringLiteral("星期日"), 7},
                {QStringLiteral("星期天"), 7},
                {QStringLiteral("周日"), 7},
                {QStringLiteral("周天"), 7},
            };
            return names;
        }

        /** 一条从单元格里解析出来的课程安排（合并前）。 */
        struct ParsedEntry {
            /** 星期（1..7）。 */
            int day_of_week = 0;

            /** 课程名。 */
            QString course_name;

            /** 教师，可能为空。 */
            QString teacher;

            /** 教室，可能为空。 */
            QString room;

            /** 上课周次（1 起，已展开）。 */
            QList<int> weeks;

            /** 该活动占用的节次（1 起）。 */
            QList<int> units;

            /** @return 用于跨行去重的键：同一个活动会在多个节次行里各出现一次。 */
            QString dedupe_key() const {
                QStringList week_text;
                for (const int week : weeks) {
                    week_text.append(QString::number(week));
                }
                QStringList unit_text;
                for (const int unit : units) {
                    unit_text.append(QString::number(unit));
                }
                return QStringLiteral("%1\x1f%2\x1f%3\x1f%4\x1f%5\x1f%6")
                    .arg(day_of_week)
                    .arg(course_name, teacher, room, week_text.join(QLatin1Char(',')), unit_text.join(QLatin1Char(',')));
            }
        };

        /** @brief 统一设置错误。 */
        bool fail(QString* error_message, const QString& message) {
            if (error_message) {
                *error_message = message;
            }
            return false;
        }

        /** @return 把全角逗号、全角括号、各种破折号统一成半角，便于后续解析。 */
        QString normalise_punctuation(const QString& text) {
            QString result = text;
            result.replace(QChar(0xFF0C), QLatin1Char(',')); // ，
            result.replace(QChar(0xFF08), QLatin1Char('('));
            result.replace(QChar(0xFF09), QLatin1Char(')'));
            result.replace(QChar(0x2013), QLatin1Char('-')); // –
            result.replace(QChar(0x2014), QLatin1Char('-')); // —
            result.replace(QChar(0x2015), QLatin1Char('-')); // ―
            result.replace(QChar(0xFF5E), QLatin1Char('-')); // ～
            result.replace(QChar(0x301C), QLatin1Char('-')); // 〜
            return result.simplified();
        }

        /**
         * @brief 解析周次表达式。
         *
         * 支持 `4-19`、`9`、`1-8,10-16`、`1-16周`、`10-17(单)`、`10-17(双)`、
         * `1-8(单周)`、`1-8（双周）`；解析不出任何周次时返回 false。
         */
        bool parse_weeks(const QString& raw_text, QList<int>* out_weeks) {
            QString text = normalise_punctuation(raw_text);
            text.remove(QLatin1Char(' '));
            // 去掉可能的“第”“周”前后缀与括号里的单双周标记
            text.remove(QRegularExpression(QStringLiteral(R"(^第)")));
            text.remove(QRegularExpression(QStringLiteral(R"(周$)")));

            bool odd_only = false;
            bool even_only = false;
            const QRegularExpression parity(QStringLiteral(R"(\((单|双)周?\))"));
            const QRegularExpressionMatch parity_match = parity.match(text);
            if (parity_match.hasMatch()) {
                odd_only = parity_match.captured(1) == QStringLiteral("单");
                even_only = !odd_only;
                text.remove(parity);
            }
            if (text.isEmpty()) {
                return false;
            }

            QList<int> weeks;
            const QStringList tokens = text.split(QLatin1Char(','), Qt::SkipEmptyParts);
            for (const QString& token : tokens) {
                const QString trimmed = token.trimmed();
                if (trimmed.isEmpty()) {
                    continue;
                }

                const QRegularExpression range(QStringLiteral(R"(^(\d+)\s*-\s*(\d+)$)"));
                const QRegularExpressionMatch range_match = range.match(trimmed);
                if (range_match.hasMatch()) {
                    const int first = range_match.captured(1).toInt();
                    const int last = range_match.captured(2).toInt();
                    if (first <= 0 || last < first) {
                        return false;
                    }
                    for (int week = first; week <= last; ++week) {
                        weeks.append(week);
                    }
                    continue;
                }

                if (const QRegularExpression single(QStringLiteral(R"(^\d+$)")); single.match(trimmed).hasMatch()) {
                    const int week = trimmed.toInt();
                    if (week <= 0) {
                        return false;
                    }
                    weeks.append(week);
                    continue;
                }
                return false;
            }

            if (weeks.isEmpty()) {
                return false;
            }

            QList<int> filtered;
            for (const int week : weeks) {
                if (odd_only && week % 2 == 0) {
                    continue;
                }
                if (even_only && week % 2 != 0) {
                    continue;
                }
                if (week <= WeekMask::MAX_WEEKS) {
                    filtered.append(week);
                }
            }
            if (filtered.isEmpty()) {
                return false;
            }

            std::sort(filtered.begin(), filtered.end());
            filtered.erase(std::unique(filtered.begin(), filtered.end()), filtered.end());
            *out_weeks = filtered;
            return true;
        }

        /** @brief 解析节次列表（`1,2`、`9,10,11`）；解析不出返回 false。 */
        bool parse_units(const QString& raw_text, QList<int>* out_units) {
            const QString text = normalise_punctuation(raw_text);
            if (text.isEmpty()) {
                return false;
            }

            QList<int> units;
            const QStringList tokens = text.split(QLatin1Char(','), Qt::SkipEmptyParts);
            for (const QString& token : tokens) {
                bool ok = false;
                const int unit = token.trimmed().toInt(&ok);
                // 上限 30 是防御性的：作息表不会有更多节次，超出说明认错了字段
                if (!ok || unit <= 0 || unit > 30) {
                    return false;
                }
                units.append(unit);
            }
            if (units.isEmpty()) {
                return false;
            }

            std::sort(units.begin(), units.end());
            units.erase(std::unique(units.begin(), units.end()), units.end());
            *out_units = units;
            return true;
        }

        /** @brief 解析左侧节次列（`1-2节` / `1-2` / `第1-2节`）为节次区间。 */
        bool parse_slot_label(const QString& raw_text, QList<int>* out_units) {
            QString text = normalise_punctuation(raw_text);
            text.remove(QRegularExpression(QStringLiteral(R"(^第)")));
            text.remove(QRegularExpression(QStringLiteral(R"(节$)")));

            static const QRegularExpression range(QStringLiteral(R"(^(\d+)\s*-\s*(\d+)$)"));
            const QRegularExpressionMatch range_match = range.match(text);
            if (range_match.hasMatch()) {
                const int first = range_match.captured(1).toInt();
                const int last = range_match.captured(2).toInt();
                if (first <= 0 || last < first || last > 30) {
                    return false;
                }
                QList<int> units;
                for (int unit = first; unit <= last; ++unit) {
                    units.append(unit);
                }
                *out_units = units;
                return true;
            }
            return parse_units(text, out_units);
        }

        /** @brief 解析 `教师 @教室`；没有 `@` 时整行都当教师。 */
        void parse_teacher_and_room(const QString& line, QString* out_teacher, QString* out_room) {
            const int at = line.indexOf(QLatin1Char('@'));
            if (at < 0) {
                *out_teacher = line.trimmed();
                out_room->clear();
                return;
            }
            *out_teacher = line.left(at).trimmed();
            *out_room = line.mid(at + 1).trimmed();
        }

        /**
         * @brief 解析一个单元格里的全部课程条目。
         *
         * 用「含 `@` 的行」当锚点：它的上一行是课程名，下一行是「周次 节次」。
         * 这样即使课程名或教师名里出现空格、括号，也不会错位。
         *
         * @param fallback_units 条目的第三行没有节次字段时退回使用的节次区间（取自左侧节次列）
         * @param problems       无法解析的条目描述，交由调用方决定是否终止导入
         */
        void parse_cell_entries(const QString& cell_text,
            int day_of_week,
            const QList<int>& fallback_units,
            QList<ParsedEntry>* out_entries,
            QStringList* problems,
            const QString& location) {
            const QStringList lines = cell_text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);

            const auto report = [&](const QString& detail) {
                if (problems) {
                    problems->append(QStringLiteral("%1：%2").arg(location, detail));
                }
            };

            for (int index = 1; index + 1 < lines.size(); ++index) {
                if (!lines.at(index).contains(QLatin1Char('@'))) {
                    continue;
                }

                ParsedEntry entry;
                entry.day_of_week = day_of_week;
                entry.course_name = lines.at(index - 1).trimmed();
                if (entry.course_name.isEmpty()) {
                    report(QStringLiteral("教师行 %1 之前没有课程名").arg(lines.at(index)));
                    continue;
                }
                parse_teacher_and_room(lines.at(index), &entry.teacher, &entry.room);

                // 第三行形如 `10-17 1,2`；拆不出节次字段时退回左侧节次列
                const QString schedule_line = normalise_punctuation(lines.at(index + 1));
                const QStringList fields = schedule_line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
                if (fields.isEmpty()) {
                    report(QStringLiteral("课程“%1”缺少周次").arg(entry.course_name));
                    continue;
                }
                if (!parse_weeks(fields.first(), &entry.weeks)) {
                    report(QStringLiteral("课程“%1”的周次“%2”无法识别").arg(entry.course_name, fields.first()));
                    continue;
                }
                if (fields.size() >= 2) {
                    if (!parse_units(fields.at(1), &entry.units)) {
                        report(QStringLiteral("课程“%1”的节次“%2”无法识别").arg(entry.course_name, fields.at(1)));
                        continue;
                    }
                }
                else {
                    entry.units = fallback_units;
                }
                if (entry.units.isEmpty()) {
                    report(QStringLiteral("课程“%1”没有可用的节次（左侧节次列也无法解析）").arg(entry.course_name));
                    continue;
                }

                out_entries->append(entry);
            }
        }

        /**
         * @brief 把一个活动的节次列表合并成上课时间段。
         *
         * 连续节次（`1,2`）合并为一段；不连续（`1,3`）拆成两段。
         */
        QList<CourseSession> build_sessions(const ParsedEntry& entry) {
            const WeekMask weeks = WeekMask::from_weeks(entry.weeks);

            QList<CourseSession> sessions;
            const auto make_session = [&](int first_unit, int last_unit) {
                CourseSession session;
                session.day_of_week = entry.day_of_week;
                session.start_slot = first_unit;
                session.slot_count = last_unit - first_unit + 1;
                session.weeks = weeks;
                session.location = entry.room;
                session.teacher = entry.teacher;
                sessions.append(session);
            };

            int run_start = entry.units.first();
            int previous = run_start;
            for (int index = 1; index < entry.units.size(); ++index) {
                const int unit = entry.units.at(index);
                if (unit == previous + 1) {
                    previous = unit;
                    continue;
                }
                make_session(run_start, previous);
                run_start = unit;
                previous = unit;
            }
            make_session(run_start, previous);
            return sessions;
        }

        /** @return 覆盖 1..unit_count 的作息表；已有默认时间则一并带入。 */
        QList<TimeSlot> build_time_slots(int unit_count) {
            QHash<int, TimeSlot> defaults;
            for (const TimeSlot& slot : TimeSlot::default_slots()) {
                defaults.insert(slot.index, slot);
            }

            // 注意：变量不能命名为 slots —— Qt 把 slots 定义为宏（qobjectdefs.h）
            QList<TimeSlot> table;
            for (int index = 1; index <= unit_count; ++index) {
                TimeSlot slot;
                slot.index = index;
                slot.label = QStringLiteral("第 %1 节").arg(index);
                if (defaults.contains(index)) {
                    slot.start_time = defaults.value(index).start_time;
                    slot.end_time = defaults.value(index).end_time;
                }
                table.append(slot);
            }
            return table;
        }

        /** @return 取正文里的“20xx-20xx [学年]第X学期”当作学期名。 */
        QString extract_semester_name(const QString& context_text, const QString& source_name) {
            static const QRegularExpression pattern(
                QStringLiteral(R"((\d{4}\s*[-—–]\s*\d{4}\s*(?:学年)?\s*第\s*[一二三四]\s*学期))"));
            const QRegularExpressionMatch match = pattern.match(context_text);
            if (match.hasMatch()) {
                return normalise_punctuation(match.captured(1)).remove(QLatin1Char(' '));
            }

            const QString base_name = QFileInfo(source_name).completeBaseName();
            return base_name.isEmpty() ? QStringLiteral("华东交大教务导入")
                                       : QStringLiteral("华东交大教务导入 · %1").arg(base_name);
        }

        /** @brief 由网格构造快照。网格已经过 `WordTableReader` 规范化（合并格已展开）。 */
        bool build_snapshot(const WordTable& table,
            const QString& source_name,
            ScheduleSnapshot* out_snapshot,
            QString* error_message) {
            // 1) 找表头行：第一格是“节次”，其后若干格是星期几
            int header_row = -1;
            // 用 QMap 而不是 QHash：QHash 的迭代顺序带进程级随机种子，
            // 会让课程的排列顺序在两次运行之间变化
            QMap<int, int> day_by_column;
            for (int row = 0; row < table.rows.size() && header_row < 0; ++row) {
                const QStringList& cells = table.rows.at(row);
                if (cells.isEmpty() || !cells.first().contains(QStringLiteral("节次"))) {
                    continue;
                }
                QMap<int, int> candidate;
                for (int column = 1; column < cells.size(); ++column) {
                    const QString label = cells.at(column).trimmed();
                    const auto found = day_names().constFind(label);
                    if (found != day_names().constEnd()) {
                        candidate.insert(column, found.value());
                    }
                }
                if (candidate.size() >= 5) {
                    header_row = row;
                    day_by_column = candidate;
                }
            }
            if (header_row < 0) {
                return fail(error_message,
                    QStringLiteral("%1 的表头不是课表样式（需要“节次”列与“星期一”~“星期日”）："
                                   "请确认导入的是华东交大教务综合管理系统导出的课表")
                        .arg(source_name));
            }

            // 2) 逐行读课程安排
            QList<ParsedEntry> entries;
            QStringList problems;
            QHash<QString, int> seen; // 去重键 -> entries 下标
            int max_unit = 0;

            for (int row = header_row + 1; row < table.rows.size(); ++row) {
                const QStringList& cells = table.rows.at(row);
                if (cells.isEmpty()) {
                    continue;
                }

                QList<int> row_units;
                if (!parse_slot_label(cells.first(), &row_units)) {
                    continue; // 不是节次行（可能是标题行、空行）
                }
                for (const int unit : row_units) {
                    max_unit = qMax(max_unit, unit);
                }

                for (auto column = day_by_column.constBegin(); column != day_by_column.constEnd(); ++column) {
                    if (column.key() >= cells.size()) {
                        continue;
                    }
                    const QString cell = cells.at(column.key());
                    if (cell.trimmed().isEmpty()) {
                        continue;
                    }

                    const QString location = QStringLiteral("第 %1 行 第 %2 列").arg(row + 1).arg(column.key() + 1);
                    QList<ParsedEntry> cell_entries;
                    parse_cell_entries(cell, column.value(), row_units, &cell_entries, &problems, location);

                    for (const ParsedEntry& entry : cell_entries) {
                        for (const int unit : entry.units) {
                            max_unit = qMax(max_unit, unit);
                        }
                        // 同一个活动会出现在它覆盖的每个节次行里，按内容去重
                        const QString key = entry.dedupe_key();
                        if (seen.contains(key)) {
                            continue;
                        }
                        seen.insert(key, entries.size());
                        entries.append(entry);
                    }
                }
            }

            if (!problems.isEmpty()) {
                // 与其静默丢掉几条安排，不如整体失败并说清哪里看不懂
                const QStringList sample = problems.mid(0, MAX_REPORTED_PROBLEMS);
                return fail(error_message,
                    QStringLiteral("%1 里有 %2 条安排无法识别：%3%4；"
                                   "请确认导入的是华东交大教务综合管理系统导出的个人课表")
                        .arg(source_name)
                        .arg(problems.size())
                        .arg(sample.join(QStringLiteral("；")))
                        .arg(problems.size() > sample.size() ? QStringLiteral(" …") : QString()));
            }
            if (entries.isEmpty()) {
                return fail(error_message,
                    QStringLiteral("%1 中未解析到任何课程安排，请确认导出的是“个人课表”").arg(source_name));
            }

            // 3) 按课程名聚合（该系统导出不含课程代码）
            QList<Course> courses;
            QHash<QString, int> course_index;
            int max_week = 0;

            for (const ParsedEntry& entry : entries) {
                const QList<CourseSession> sessions = build_sessions(entry);
                if (sessions.isEmpty()) {
                    continue;
                }
                for (const CourseSession& session : sessions) {
                    max_week = qMax(max_week, session.weeks.last_week());
                }

                int index = course_index.value(entry.course_name, -1);
                if (index < 0) {
                    Course course;
                    course.name = entry.course_name;
                    courses.append(course);
                    index = courses.size() - 1;
                    course_index.insert(entry.course_name, index);
                }
                courses[index].sessions.append(sessions);
            }

            if (courses.isEmpty()) {
                return fail(error_message,
                    QStringLiteral("%1 中未解析到任何课程安排，请确认导出的是“个人课表”").arg(source_name));
            }

            // 课程级字段取第一个时间段的值作为默认，与之相同的按时间段不再重复存储
            for (Course& course : courses) {
                const CourseSession& first = course.sessions.first();
                course.teacher = first.teacher;
                course.location = first.location;
                for (CourseSession& session : course.sessions) {
                    if (session.teacher == course.teacher) {
                        session.teacher.clear();
                    }
                    if (session.location == course.location) {
                        session.location.clear();
                    }
                }
                course.ensure_session_ids();
            }

            ScheduleSnapshot snapshot;
            snapshot.semester.name = extract_semester_name(table.context_text, source_name);
            // 导出文件不含学期起始日期：与 CSV / 正方导入一致，先按本周一合成，
            // 若应用中已存在当前学期，ImportManager 会改用真实学期。
            snapshot.semester.start_date = WeekCalculator::monday_of(QDate::currentDate());
            snapshot.semester.total_weeks = qBound(1, qMax(20, max_week), WeekMask::MAX_WEEKS);
            snapshot.time_slots = build_time_slots(qMax(DEFAULT_UNIT_COUNT, max_unit));
            snapshot.courses = courses;

            for (Course& course : snapshot.courses) {
                course.semester_id = snapshot.semester.id;
            }

            *out_snapshot = snapshot;
            if (error_message) {
                error_message->clear();
            }
            return true;
        }

        /** @brief 从已经读好的表格构造快照。 */
        bool build_from_table(const WordTable& table, const QString& source_name, ScheduleSnapshot* out_snapshot, QString* error_message) {
            if (table.is_empty()) {
                return fail(error_message, QStringLiteral("%1 里没有读到表格内容").arg(source_name));
            }
            return build_snapshot(table, source_name, out_snapshot, error_message);
        }

        /** @return 文件头几个字节是否是 OLE 复合文档（Word 97-2003 的 .doc）。 */
        bool looks_like_binary_doc(const QByteArray& head) {
            return head.startsWith(QByteArray::fromHex("D0CF11E0A1B11AE1"));
        }

        /** @return 内容是否是 Word 版式 HTML / 课表页面。 */
        bool looks_like_word_html(const QByteArray& data) {
            return format_from_content(data) == ScheduleFormat::EcjtuTimetable;
        }

    } // namespace

    ScheduleFormat EcjtuTimetableIo::format() const {
        return ScheduleFormat::EcjtuTimetable;
    }

    QString EcjtuTimetableIo::display_name() const {
        return QStringLiteral("华东交大教务课表");
    }

    QStringList EcjtuTimetableIo::extensions() const {
        // `extensions()` 只用于界面提示：该校导出实际就是这两种。
        // `.html/.htm` 不列在这里——它们与正方教务页面共用后缀，
        // 由 format_from_content() 按内容判定，免得提示语里出现重复。
        return QStringList{QStringLiteral(".doc"), QStringLiteral(".docx")};
    }

    bool EcjtuTimetableIo::can_import(const QString& file_path, QString* error_message) const {
        QFile file(file_path);
        if (!file.exists()) {
            return fail(error_message, QStringLiteral("文件不存在：%1").arg(file_path));
        }
        if (!file.open(QIODevice::ReadOnly)) {
            return fail(error_message, QStringLiteral("无法读取文件 %1：%2").arg(file_path, file.errorString()));
        }
        const QByteArray head = file.read(256 * 1024);
        file.close();

        if (looks_like_binary_doc(head) || DocConvertUtil::is_ooxml_package(file_path)) {
            // 这两种形态要看全文才能判断，交给 parse() 给出准确结论
            if (error_message) {
                error_message->clear();
            }
            return true;
        }
        if (!looks_like_word_html(head)) {
            return fail(error_message,
                QStringLiteral("不是华东交大教务课表（未找到“节次 + 星期一~星期日”表头）"));
        }
        if (error_message) {
            error_message->clear();
        }
        return true;
    }

    bool EcjtuTimetableIo::parse(const QString& file_path, ScheduleSnapshot* out_snapshot, QString* error_message) const {
        QFile file(file_path);
        if (!file.exists()) {
            return fail(error_message, QStringLiteral("文件不存在：%1").arg(file_path));
        }
        if (!file.open(QIODevice::ReadOnly)) {
            return fail(error_message, QStringLiteral("无法读取文件 %1：%2").arg(file_path, file.errorString()));
        }
        const QByteArray head = file.read(256 * 1024);
        file.close();

        WordTable table;

        // 形态一：真正的 .docx —— 直接读 word/document.xml
        if (DocConvertUtil::is_ooxml_package(file_path)) {
            if (!WordTableReader::read_docx(file_path, &table, error_message)) {
                return false;
            }
            return build_from_table(table, file_path, out_snapshot, error_message);
        }

        // 形态二：Word 版式 HTML（扩展名常常就是 .doc）
        if (!looks_like_binary_doc(head)) {
            // 这里必须读全文：上面读到的 head 只是用于嗅探的前缀
            if (!file.open(QIODevice::ReadOnly)) {
                return fail(error_message, QStringLiteral("无法读取文件 %1：%2").arg(file_path, file.errorString()));
            }
            const QByteArray data = file.readAll();
            file.close();

            if (!WordTableReader::read_word_html(data, &table, error_message)) {
                return false;
            }
            return build_from_table(table, file_path, out_snapshot, error_message);
        }

        // 形态三：Word 97-2003 二进制 .doc —— 先转成 .docx
        QTemporaryDir scratch;
        if (!scratch.isValid()) {
            return fail(error_message, QStringLiteral("无法创建临时目录用于转换 %1").arg(file_path));
        }
        const DocConvertUtil::Conversion conversion = DocConvertUtil::convert_to_docx(file_path, scratch.path());
        if (!conversion.success) {
            return fail(error_message, conversion.error_message);
        }
        if (!WordTableReader::read_docx(conversion.output_path, &table, error_message)) {
            return false;
        }
        return build_from_table(table, file_path, out_snapshot, error_message);
    }

    bool EcjtuTimetableIo::parse_data(const QByteArray& data,
        const QString& source_name,
        ScheduleSnapshot* out_snapshot,
        QString* error_message) const {
        if (!out_snapshot) {
            return fail(error_message, QStringLiteral("输出快照为空"));
        }
        if (data.isEmpty()) {
            return fail(error_message, QStringLiteral("内容为空"));
        }

        WordTable table;

        // 内存里拿到的多半是抓取的页面原文；理论上也可能是 docx，写临时文件后按 zip 读
        if (data.startsWith(QByteArray::fromHex("504B0304"))) {
            QTemporaryDir scratch;
            if (!scratch.isValid()) {
                return fail(error_message, QStringLiteral("无法创建临时目录用于读取 %1").arg(source_name));
            }
            const QString path = QDir(scratch.path()).filePath(QStringLiteral("captured.docx"));
            QFile file(path);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                return fail(error_message, QStringLiteral("无法写入临时文件以读取 %1").arg(source_name));
            }
            file.write(data);
            file.close();

            if (!WordTableReader::read_docx(path, &table, error_message)) {
                return false;
            }
            return build_from_table(table, source_name, out_snapshot, error_message);
        }

        if (!WordTableReader::read_word_html(data, &table, error_message)) {
            return false;
        }
        return build_from_table(table, source_name, out_snapshot, error_message);
    }

} // namespace Schedule
