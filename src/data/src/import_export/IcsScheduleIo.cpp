#include "data/import_export/IcsScheduleIo.h"

#include "core/service/WeekCalculator.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QSaveFile>
#include <QStringConverter>
#include <QStringList>

#include <algorithm>

namespace Schedule {

    namespace {

        /** RFC 5545 建议的最大行长（字节），超出需折行。 */
        constexpr int ICS_FOLD_LIMIT = 75;

        /** @brief 统一设置错误。 */
        bool fail(QString* error_message, const QString& message) {
            if (error_message) {
                *error_message = message;
            }
            return false;
        }

        /** @return RFC 5545 文本转义结果。 */
        QString escape_text(const QString& text) {
            QString escaped;
            escaped.reserve(text.size() + 8);
            for (const QChar& character : text) {
                switch (character.unicode()) {
                case '\\':
                    escaped += QStringLiteral("\\\\");
                    break;
                case ';':
                    escaped += QStringLiteral("\\;");
                    break;
                case ',':
                    escaped += QStringLiteral("\\,");
                    break;
                case '\n':
                    escaped += QStringLiteral("\\n");
                    break;
                case '\r':
                    break; // 归一到 \n
                default:
                    escaped += character;
                    break;
                }
            }
            return escaped;
        }

        /** @return RFC 5545 文本反转义结果。 */
        QString unescape_text(const QString& text) {
            QString result;
            result.reserve(text.size());
            for (int i = 0; i < text.size(); ++i) {
                if (text.at(i) != QLatin1Char('\\') || i + 1 >= text.size()) {
                    result += text.at(i);
                    continue;
                }
                const QChar next = text.at(i + 1);
                ++i;
                if (next == QLatin1Char('n') || next == QLatin1Char('N')) {
                    result += QLatin1Char('\n');
                }
                else {
                    result += next;
                }
            }
            return result;
        }

        /**
         * @brief 按 75 字节折行，续行以单个空格开头。
         *
         * 注意不能在多字节 UTF-8 序列中间切断，否则接收方会解码失败。
         */
        QString fold_line(const QString& line) {
            const QByteArray utf8 = line.toUtf8();
            if (utf8.size() <= ICS_FOLD_LIMIT) {
                return line;
            }

            QStringList pieces;
            int offset = 0;
            int limit = ICS_FOLD_LIMIT;
            while (offset < utf8.size()) {
                int take = qMin(limit, utf8.size() - offset);
                while (take > 0 && offset + take < utf8.size() && (static_cast<unsigned char>(utf8.at(offset + take)) & 0xC0) == 0x80) {
                    --take;
                }
                if (take <= 0) {
                    break;
                }
                pieces.append(QString::fromUtf8(utf8.mid(offset, take)));
                offset += take;
                limit = ICS_FOLD_LIMIT - 1; // 续行前缀空格占 1 字节
            }
            return pieces.join(QStringLiteral("\r\n "));
        }

        /** @return `yyyyMMddTHHmmss` 形式的本地浮动时间。 */
        QString format_datetime(const QDateTime& datetime) {
            return datetime.toString(QStringLiteral("yyyyMMddTHHmmss"));
        }

        /** @return `yyyyMMdd` 形式的日期。 */
        QString format_date(const QDate& date) {
            return date.toString(QStringLiteral("yyyyMMdd"));
        }

        /**
         * @brief 解析 ICS 的 DATE / DATE-TIME 值。
         *
         * 支持 `20240902T080000`（本地浮动）、`20240902T000000Z`（UTC，转本地）、
         * `20240902`（全天，`is_date_only` 置真）。
         */
        QDateTime parse_datetime_value(const QString& raw_value, bool* is_date_only) {
            const QString value = raw_value.trimmed();
            if (is_date_only) {
                *is_date_only = false;
            }

            if (value.size() == 8 && !value.contains(QLatin1Char('T'))) {
                const QDate date = QDate::fromString(value, QStringLiteral("yyyyMMdd"));
                if (is_date_only) {
                    *is_date_only = true;
                }
                return date.isValid() ? QDateTime(date, QTime(0, 0)) : QDateTime();
            }

            const bool utc = value.endsWith(QLatin1Char('Z'), Qt::CaseInsensitive);
            const QString core = utc ? value.left(value.size() - 1) : value;
            QDateTime datetime = QDateTime::fromString(core, QStringLiteral("yyyyMMddTHHmmss"));
            if (!datetime.isValid()) {
                datetime = QDateTime::fromString(core, QStringLiteral("yyyyMMddTHHmm"));
            }
            if (!datetime.isValid()) {
                return QDateTime();
            }
            if (utc) {
                datetime.setTimeZone(QTimeZone::UTC);
                return datetime.toLocalTime();
            }
            return datetime;
        }

        /** @brief 解析后的 ICS 属性行：名称、参数、值。 */
        struct IcsProperty {
            QString name;
            QMap<QString, QString> parameters;
            QString value;
        };

        /** @brief 把 `NAME;PARAM=..:VALUE` 拆成结构化属性。 */
        IcsProperty parse_property(const QString& line) {
            IcsProperty property;
            const int colon = line.indexOf(QLatin1Char(':'));
            if (colon < 0) {
                return property;
            }

            const QString left = line.left(colon);
            property.value = line.mid(colon + 1);

            const QStringList parts = left.split(QLatin1Char(';'));
            if (parts.isEmpty()) {
                return property;
            }
            property.name = parts.first().trimmed().toUpper();
            for (int i = 1; i < parts.size(); ++i) {
                const int equals = parts.at(i).indexOf(QLatin1Char('='));
                if (equals <= 0) {
                    continue;
                }
                property.parameters.insert(parts.at(i).left(equals).trimmed().toUpper(), parts.at(i).mid(equals + 1).trimmed());
            }
            return property;
        }

        /** @brief 展开折行后的逻辑行列表。 */
        QStringList unfold_lines(const QString& text) {
            QString normalised = text;
            normalised.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
            normalised.replace(QLatin1Char('\r'), QLatin1Char('\n'));
            // 折行规则：续行以空格或水平制表符开头
            normalised.replace(QStringLiteral("\n "), QString());
            normalised.replace(QStringLiteral("\n\t"), QString());
            return normalised.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        }

        /** @brief 从一个 VEVENT 中抽取的原始信息。 */
        struct RawEvent {
            QString uid;
            QString summary;
            QString location;
            QString description;
            QDateTime start;
            QDateTime end;
            bool all_day = false;
            int slot = 0;
            int slot_count = 0;
            int day_of_week = 0;
            QString weeks_expression;
            int rrule_count = 0;
            int rrule_interval = 1;
            QList<QDateTime> rdates;
        };

        /** @return 从 RRULE 值中解析出的 (COUNT, INTERVAL)。 */
        QPair<int, int> parse_rrule(const QString& value) {
            int count = 0;
            int interval = 1;
            const QStringList parts = value.split(QLatin1Char(';'), Qt::SkipEmptyParts);
            for (const QString& part : parts) {
                const int equals = part.indexOf(QLatin1Char('='));
                if (equals <= 0) {
                    continue;
                }
                const QString key = part.left(equals).trimmed().toUpper();
                const QString text = part.mid(equals + 1).trimmed();
                if (key == QStringLiteral("COUNT")) {
                    count = text.toInt();
                }
                else if (key == QStringLiteral("INTERVAL")) {
                    interval = qMax(1, text.toInt());
                }
            }
            return {count, interval};
        }

        /** @return 周次是否为等差数列；是则输出首项、步长与项数。 */
        bool arithmetic_progression(const WeekMask& mask, int* first, int* step, int* count) {
            const QList<int> weeks = mask.weeks();
            if (weeks.isEmpty()) {
                return false;
            }
            const int stride = weeks.size() >= 2 ? weeks.at(1) - weeks.at(0) : 1;
            for (int i = 1; i < weeks.size(); ++i) {
                if (weeks.at(i) - weeks.at(i - 1) != stride) {
                    return false;
                }
            }
            if (first) {
                *first = weeks.first();
            }
            if (step) {
                *step = stride;
            }
            if (count) {
                *count = static_cast<int>(weeks.size());
            }
            return true;
        }

    } // namespace

    ScheduleFormat IcsScheduleIo::format() const {
        return ScheduleFormat::Ics;
    }

    QString IcsScheduleIo::display_name() const {
        return format_display_name(ScheduleFormat::Ics);
    }

    QStringList IcsScheduleIo::extensions() const {
        return QStringList{QStringLiteral(".ics"), QStringLiteral(".ical")};
    }

    bool IcsScheduleIo::can_import(const QString& file_path, QString* error_message) const {
        const ScheduleFormat by_extension = format_from_extension(file_path);
        if (by_extension != ScheduleFormat::Ics && by_extension != ScheduleFormat::Unknown) {
            return fail(error_message, QStringLiteral("不是 iCalendar 文件：%1").arg(file_path));
        }
        if (error_message) {
            error_message->clear();
        }
        return true;
    }

    QString IcsScheduleIo::extension() const {
        return file_extension(ScheduleFormat::Ics);
    }

    QByteArray IcsScheduleIo::serialize(const ScheduleSnapshot& snapshot, QString* error_message) const {
        QStringList lines;
        const auto add_line = [&lines](const QString& line) {
            lines.append(fold_line(line));
        };

        add_line(QStringLiteral("BEGIN:VCALENDAR"));
        add_line(QStringLiteral("VERSION:2.0"));
        add_line(QStringLiteral("PRODID:-//Schedule//Schedule App//CN"));
        add_line(QStringLiteral("CALSCALE:GREGORIAN"));
        add_line(QStringLiteral("METHOD:PUBLISH"));
        add_line(QStringLiteral("X-WR-CALNAME:%1").arg(escape_text(snapshot.semester.name)));
        add_line(QStringLiteral("X-SCHEDULE-SEMESTER-ID:%1").arg(snapshot.semester.id));
        add_line(QStringLiteral("X-SCHEDULE-SEMESTER-START:%1").arg(format_date(snapshot.semester.start_date)));
        add_line(QStringLiteral("X-SCHEDULE-SEMESTER-WEEKS:%1").arg(snapshot.semester.total_weeks));

        for (const Course& course : snapshot.courses) {
            for (const CourseSession& session : course.sessions) {
                const QDate first_date = WeekCalculator::date_of(snapshot.semester, session.weeks.first_week(), session.day_of_week);

                TimeSlot start_slot;
                TimeSlot end_slot;
                for (const TimeSlot& slot : snapshot.time_slots) {
                    if (slot.index == session.start_slot) {
                        start_slot = slot;
                    }
                    if (slot.index == session.end_slot()) {
                        end_slot = slot;
                    }
                }
                if (!first_date.isValid() || !start_slot.start_time.isValid()) {
                    // 缺少作息时间时无法生成合法的 DTSTART，跳过该时间段。
                    // 这类课程仍会出现在 JSON / CSV 导出中，只有 ICS 无法表达。
                    continue;
                }

                QDateTime begin(first_date, start_slot.start_time);
                const QTime end_time = end_slot.end_time.isValid() ? end_slot.end_time : start_slot.end_time;
                const QDate end_date = end_time.isValid() && end_time < start_slot.start_time ? first_date.addDays(1) : first_date;
                QDateTime finish(end_date, end_time.isValid() ? end_time : start_slot.end_time);

                const QString uid = (session.id.isEmpty() ? course.id : session.id) + QStringLiteral("@schedule");

                add_line(QStringLiteral("BEGIN:VEVENT"));
                add_line(QStringLiteral("UID:%1").arg(uid));
                add_line(QStringLiteral("DTSTAMP:%1Z").arg(QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMddTHHmmss"))));
                add_line(QStringLiteral("DTSTART:%1").arg(format_datetime(begin)));
                add_line(QStringLiteral("DTEND:%1").arg(format_datetime(finish)));

                int first_week = 1;
                int step = 1;
                int occurrences = 1;
                if (arithmetic_progression(session.weeks, &first_week, &step, &occurrences)) {
                    QString rrule = QStringLiteral("RRULE:FREQ=WEEKLY;COUNT=%1").arg(occurrences);
                    if (step > 1) {
                        rrule += QStringLiteral(";INTERVAL=%1").arg(step);
                    }
                    rrule += QStringLiteral(";BYDAY=%1").arg(QStringList{QString(), QStringLiteral("MO"), QStringLiteral("TU"), QStringLiteral("WE"), QStringLiteral("TH"), QStringLiteral("FR"), QStringLiteral("SA"), QStringLiteral("SU")}.value(session.day_of_week));
                    add_line(rrule);
                }
                else {
                    // 非等差周次（如 1-4,6,9-10）无法用单条 RRULE 表达，改用显式 RDATE 列表
                    QStringList rdates;
                    for (int week : session.weeks.weeks()) {
                        const QDate date = WeekCalculator::date_of(snapshot.semester, week, session.day_of_week);
                        if (date.isValid()) {
                            rdates.append(format_datetime(QDateTime(date, start_slot.start_time)));
                        }
                    }
                    if (!rdates.isEmpty()) {
                        add_line(QStringLiteral("RDATE:%1").arg(rdates.join(QLatin1Char(','))));
                    }
                }

                add_line(QStringLiteral("SUMMARY:%1").arg(escape_text(course.name)));
                const QString location = session.effective_location(course.location);
                if (!location.isEmpty()) {
                    add_line(QStringLiteral("LOCATION:%1").arg(escape_text(location)));
                }
                const QString teacher = session.effective_teacher(course.teacher);
                if (!teacher.isEmpty() || !course.notes.isEmpty()) {
                    QString description;
                    if (!teacher.isEmpty()) {
                        description = QStringLiteral("教师：%1").arg(teacher);
                    }
                    if (!course.notes.isEmpty()) {
                        description += (description.isEmpty() ? QString() : QStringLiteral("\n")) + course.notes;
                    }
                    add_line(QStringLiteral("DESCRIPTION:%1").arg(escape_text(description)));
                }

                // 扩展属性：让本应用自己导出的 ICS 能无损往返
                add_line(QStringLiteral("X-SCHEDULE-SLOT:%1").arg(session.start_slot));
                add_line(QStringLiteral("X-SCHEDULE-SLOT-COUNT:%1").arg(session.slot_count));
                add_line(QStringLiteral("X-SCHEDULE-DAY:%1").arg(session.day_of_week));
                add_line(QStringLiteral("X-SCHEDULE-WEEKS:%1").arg(session.weeks.to_expression()));
                if (!course.code.isEmpty()) {
                    add_line(QStringLiteral("X-SCHEDULE-COURSE-CODE:%1").arg(escape_text(course.code)));
                }
                add_line(QStringLiteral("END:VEVENT"));
            }
        }

        add_line(QStringLiteral("END:VCALENDAR"));

        if (error_message) {
            error_message->clear();
        }

        return (lines.join(QStringLiteral("\r\n")) + QStringLiteral("\r\n")).toUtf8();
    }

    bool IcsScheduleIo::write(const QString& file_path, const ScheduleSnapshot& snapshot, QString* error_message) const {
        if (file_path.isEmpty()) {
            return fail(error_message, QStringLiteral("导出路径为空"));
        }

        const QDir parent = QFileInfo(file_path).absoluteDir();
        if (!parent.exists() && !parent.mkpath(QStringLiteral("."))) {
            return fail(error_message, QStringLiteral("无法创建导出目录：%1").arg(parent.absolutePath()));
        }

        QSaveFile file(file_path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            return fail(error_message, QStringLiteral("无法写入文件 %1：%2").arg(file_path, file.errorString()));
        }
        const QByteArray content = serialize(snapshot);
        if (file.write(content) != content.size()) {
            file.cancelWriting();
            return fail(error_message, QStringLiteral("写入文件失败：%1").arg(file.errorString()));
        }
        if (!file.commit()) {
            return fail(error_message, QStringLiteral("保存文件失败：%1").arg(file.errorString()));
        }

        if (error_message) {
            error_message->clear();
        }
        return true;
    }

    bool IcsScheduleIo::parse(const QString& file_path, ScheduleSnapshot* out_snapshot, QString* error_message) const {
        QFile file(file_path);
        if (!file.exists()) {
            return fail(error_message, QStringLiteral("文件不存在：%1").arg(file_path));
        }
        if (!file.open(QIODevice::ReadOnly)) {
            return fail(error_message, QStringLiteral("无法读取文件 %1：%2").arg(file_path, file.errorString()));
        }
        return parse_data(file.readAll(), file_path, out_snapshot, error_message);
    }

    bool IcsScheduleIo::parse_data(const QByteArray& data,
        const QString& source_name,
        ScheduleSnapshot* out_snapshot,
        QString* error_message) const {
        if (!out_snapshot) {
            return fail(error_message, QStringLiteral("输出参数为空"));
        }

        QStringDecoder decoder(QStringDecoder::Utf8);
        const QString text = decoder(data);
        if (decoder.hasError()) {
            return fail(error_message, QStringLiteral("%1 不是 UTF-8 编码的 iCalendar 文本").arg(source_name));
        }

        const QStringList lines = unfold_lines(text);

        QList<RawEvent> events;
        RawEvent current;
        bool in_event = false;
        bool has_calendar = false;
        QString calendar_name;

        for (const QString& line : lines) {
            const QString trimmed = line.trimmed();
            if (trimmed.compare(QStringLiteral("BEGIN:VCALENDAR"), Qt::CaseInsensitive) == 0) {
                has_calendar = true;
                continue;
            }
            if (trimmed.compare(QStringLiteral("BEGIN:VEVENT"), Qt::CaseInsensitive) == 0) {
                in_event = true;
                current = RawEvent();
                continue;
            }
            if (trimmed.compare(QStringLiteral("END:VEVENT"), Qt::CaseInsensitive) == 0) {
                if (in_event && !current.summary.isEmpty() && current.start.isValid()) {
                    events.append(current);
                }
                in_event = false;
                continue;
            }
            if (!in_event) {
                if (trimmed.startsWith(QStringLiteral("X-WR-CALNAME"), Qt::CaseInsensitive)) {
                    calendar_name = unescape_text(parse_property(trimmed).value);
                }
                continue;
            }

            const IcsProperty property = parse_property(trimmed);
            if (property.name.isEmpty()) {
                continue;
            }

            if (property.name == QStringLiteral("UID")) {
                current.uid = property.value;
            }
            else if (property.name == QStringLiteral("SUMMARY")) {
                current.summary = unescape_text(property.value);
            }
            else if (property.name == QStringLiteral("LOCATION")) {
                current.location = unescape_text(property.value);
            }
            else if (property.name == QStringLiteral("DESCRIPTION")) {
                current.description = unescape_text(property.value);
            }
            else if (property.name == QStringLiteral("DTSTART")) {
                current.start = parse_datetime_value(property.value, &current.all_day);
            }
            else if (property.name == QStringLiteral("DTEND")) {
                current.end = parse_datetime_value(property.value, nullptr);
            }
            else if (property.name == QStringLiteral("RRULE")) {
                const QPair<int, int> parsed = parse_rrule(property.value);
                current.rrule_count = parsed.first;
                current.rrule_interval = parsed.second;
            }
            else if (property.name == QStringLiteral("RDATE")) {
                const QStringList values = property.value.split(QLatin1Char(','), Qt::SkipEmptyParts);
                for (const QString& value : values) {
                    const QDateTime datetime = parse_datetime_value(value, nullptr);
                    if (datetime.isValid()) {
                        current.rdates.append(datetime);
                    }
                }
            }
            else if (property.name == QStringLiteral("X-SCHEDULE-SLOT")) {
                current.slot = property.value.toInt();
            }
            else if (property.name == QStringLiteral("X-SCHEDULE-SLOT-COUNT")) {
                current.slot_count = property.value.toInt();
            }
            else if (property.name == QStringLiteral("X-SCHEDULE-DAY")) {
                current.day_of_week = property.value.toInt();
            }
            else if (property.name == QStringLiteral("X-SCHEDULE-WEEKS")) {
                current.weeks_expression = property.value;
            }
        }

        if (!has_calendar) {
            return fail(error_message, QStringLiteral("%1 不是 iCalendar 文件（缺少 BEGIN:VCALENDAR）").arg(source_name));
        }

        // 过滤全天事件：它们不表示具体上课时段
        QList<RawEvent> timed_events;
        int all_day_count = 0;
        for (const RawEvent& event : events) {
            if (event.all_day) {
                ++all_day_count;
                continue;
            }
            timed_events.append(event);
        }
        if (timed_events.isEmpty()) {
            return fail(error_message,
                QStringLiteral("%1 中没有可用的上课事件（全天事件 %2 条已忽略）").arg(source_name).arg(all_day_count));
        }

        // 用出现过的上课时间合成作息表：按开始时间升序编号
        QMap<QTime, QTime> time_pairs;
        for (const RawEvent& event : timed_events) {
            const QTime begin = event.start.time();
            const QTime end = event.end.isValid() && event.end.date() == event.start.date() ? event.end.time() : begin.addSecs(45 * 60);
            if (!time_pairs.contains(begin)) {
                time_pairs.insert(begin, end);
            }
        }
        QMap<QTime, int> slot_index_by_time;
        QList<TimeSlot> time_slots;
        {
            QList<QTime> starts = time_pairs.keys();
            std::sort(starts.begin(), starts.end());
            for (int i = 0; i < starts.size(); ++i) {
                TimeSlot slot;
                slot.index = i + 1;
                slot.label = QStringLiteral("第 %1 节").arg(slot.index);
                slot.start_time = starts.at(i);
                slot.end_time = time_pairs.value(starts.at(i));
                time_slots.append(slot);
                slot_index_by_time.insert(starts.at(i), slot.index);
            }
        }

        // 学期锚点：最早事件所在自然周的周一
        QDate earliest = timed_events.first().start.date();
        for (const RawEvent& event : timed_events) {
            if (event.start.date() < earliest) {
                earliest = event.start.date();
            }
        }

        ScheduleSnapshot snapshot;
        snapshot.time_slots = time_slots;
        snapshot.semester.name = calendar_name.isEmpty() ? QStringLiteral("ICS 导入 · %1").arg(source_name) : calendar_name;
        snapshot.semester.start_date = WeekCalculator::monday_of(earliest);
        snapshot.semester.total_weeks = 20;

        QMap<QString, int> course_index_by_name;
        int max_week = 1;

        for (const RawEvent& event : timed_events) {
            const int day_of_week = event.day_of_week > 0 ? event.day_of_week : event.start.date().dayOfWeek();

            int start_slot = event.slot;
            if (start_slot < 1) {
                start_slot = slot_index_by_time.value(event.start.time(), 1);
            }

            int slot_count = event.slot_count;
            if (slot_count < 1) {
                // 由 DTEND 覆盖的时长推断连续节次数
                slot_count = 1;
                const QTime end_time = event.end.isValid() && event.end.date() == event.start.date() ? event.end.time() : event.start.time();
                for (const TimeSlot& slot : time_slots) {
                    if (slot.index <= start_slot || !slot.start_time.isValid()) {
                        continue;
                    }
                    if (slot.start_time < end_time) {
                        slot_count = slot.index - start_slot + 1;
                    }
                }
            }

            WeekMask weeks;
            if (!event.weeks_expression.isEmpty()) {
                QString weeks_error;
                weeks = WeekMask::from_expression(event.weeks_expression, 0, &weeks_error);
                if (!weeks_error.isEmpty()) {
                    weeks = WeekMask();
                }
            }
            if (weeks.is_empty() && !event.rdates.isEmpty()) {
                QList<int> weeks_list;
                for (const QDateTime& datetime : event.rdates) {
                    const int week = snapshot.semester.week_of(datetime.date());
                    if (week > 0) {
                        weeks_list.append(week);
                    }
                }
                weeks = WeekMask::from_weeks(weeks_list);
            }
            if (weeks.is_empty()) {
                const int first_week = qMax(1, snapshot.semester.week_of(event.start.date()));
                const int count = event.rrule_count > 0 ? event.rrule_count : 1;
                const int step = qMax(1, event.rrule_interval);
                QList<int> weeks_list;
                for (int i = 0; i < count; ++i) {
                    const int week = first_week + i * step;
                    if (week <= WeekMask::MAX_WEEKS) {
                        weeks_list.append(week);
                    }
                }
                weeks = WeekMask::from_weeks(weeks_list);
            }
            if (weeks.is_empty()) {
                continue;
            }
            max_week = qMax(max_week, weeks.last_week());

            if (!course_index_by_name.contains(event.summary)) {
                Course course;
                course.name = event.summary;
                course.location = event.location;
                course.color = Course::default_color_for(event.summary);
                // DESCRIPTION 形如“教师：张三\n备注”，这里拆出教师
                if (event.description.startsWith(QStringLiteral("教师："))) {
                    const int newline = event.description.indexOf(QLatin1Char('\n'));
                    if (newline < 0) {
                        course.teacher = event.description.mid(QStringLiteral("教师：").size());
                    }
                    else {
                        course.teacher = event.description.mid(QStringLiteral("教师：").size(), newline - QStringLiteral("教师：").size());
                        course.notes = event.description.mid(newline + 1);
                    }
                }
                else {
                    course.notes = event.description;
                }
                snapshot.courses.append(course);
                course_index_by_name.insert(event.summary, snapshot.courses.size() - 1);
            }

            CourseSession session;
            session.day_of_week = day_of_week;
            session.start_slot = start_slot;
            session.slot_count = qMax(1, slot_count);
            session.weeks = weeks;
            session.location = event.location.isEmpty() ? QString() : event.location;
            if (!event.uid.isEmpty()) {
                // UID 形如 `<id>@schedule`：去掉域名部分作为时间段 id，便于重复导入时对账
                const int at = event.uid.indexOf(QLatin1Char('@'));
                session.id = at > 0 ? event.uid.left(at) : event.uid;
            }
            snapshot.courses[course_index_by_name.value(event.summary)].sessions.append(session);
        }

        if (snapshot.courses.isEmpty()) {
            return fail(error_message, QStringLiteral("%1 中没有解析到任何课程事件").arg(source_name));
        }

        snapshot.semester.total_weeks = qBound(1, qMax(20, max_week), WeekMask::MAX_WEEKS);
        for (Course& course : snapshot.courses) {
            course.ensure_session_ids();
        }

        *out_snapshot = snapshot;
        if (error_message) {
            error_message->clear();
        }
        return true;
    }

} // namespace Schedule
