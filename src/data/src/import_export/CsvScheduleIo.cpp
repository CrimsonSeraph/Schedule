#include "data/import_export/CsvScheduleIo.h"

#include "core/service/WeekCalculator.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QSaveFile>
#include <QStringConverter>
#include <QStringList>

namespace Schedule {

    namespace {

        /** UTF-8 BOM：导出时写入，便于 Excel 识别中文。 */
        const QByteArray UTF8_BOM = QByteArray::fromHex("EFBBBF");

        /** 逻辑列标识。 */
        enum class Column {
            Name,
            Code,
            Teacher,
            Location,
            Day,
            StartSlot,
            SlotCount,
            Weeks,
            Credits,
            Notes,
            StartTime,
            EndTime,
            Unknown,
        };

        /**
         * @brief 列名别名表。
         *
         * 导入时按“规范化后的表头文本”查表，因此列顺序、大小写、空格都不敏感。
         */
        const QList<QPair<QString, Column>>& column_aliases() {
            static const QList<QPair<QString, Column>> aliases = {
                {QStringLiteral("课程名称"), Column::Name},
                {QStringLiteral("课程"), Column::Name},
                {QStringLiteral("名称"), Column::Name},
                {QStringLiteral("name"), Column::Name},
                {QStringLiteral("course"), Column::Name},
                {QStringLiteral("课程代码"), Column::Code},
                {QStringLiteral("代码"), Column::Code},
                {QStringLiteral("code"), Column::Code},
                {QStringLiteral("教师"), Column::Teacher},
                {QStringLiteral("老师"), Column::Teacher},
                {QStringLiteral("授课教师"), Column::Teacher},
                {QStringLiteral("teacher"), Column::Teacher},
                {QStringLiteral("地点"), Column::Location},
                {QStringLiteral("教室"), Column::Location},
                {QStringLiteral("上课地点"), Column::Location},
                {QStringLiteral("location"), Column::Location},
                {QStringLiteral("星期"), Column::Day},
                {QStringLiteral("周几"), Column::Day},
                {QStringLiteral("day"), Column::Day},
                {QStringLiteral("weekday"), Column::Day},
                {QStringLiteral("开始节次"), Column::StartSlot},
                {QStringLiteral("起始节次"), Column::StartSlot},
                {QStringLiteral("节次"), Column::StartSlot},
                {QStringLiteral("start_slot"), Column::StartSlot},
                {QStringLiteral("节次数量"), Column::SlotCount},
                {QStringLiteral("连续节次"), Column::SlotCount},
                {QStringLiteral("slot_count"), Column::SlotCount},
                {QStringLiteral("周次"), Column::Weeks},
                {QStringLiteral("上课周次"), Column::Weeks},
                {QStringLiteral("weeks"), Column::Weeks},
                {QStringLiteral("学分"), Column::Credits},
                {QStringLiteral("credits"), Column::Credits},
                {QStringLiteral("备注"), Column::Notes},
                {QStringLiteral("notes"), Column::Notes},
                {QStringLiteral("开始时间"), Column::StartTime},
                {QStringLiteral("start"), Column::StartTime},
                {QStringLiteral("结束时间"), Column::EndTime},
                {QStringLiteral("end"), Column::EndTime},
            };
            return aliases;
        }

        /** 导出表头，顺序即列顺序。 */
        QStringList export_header() {
            return QStringList{
                QStringLiteral("课程名称"),
                QStringLiteral("课程代码"),
                QStringLiteral("教师"),
                QStringLiteral("地点"),
                QStringLiteral("星期"),
                QStringLiteral("开始节次"),
                QStringLiteral("节次数量"),
                QStringLiteral("周次"),
                QStringLiteral("学分"),
                QStringLiteral("备注"),
                QStringLiteral("开始时间"),
                QStringLiteral("结束时间"),
            };
        }

        /** @return 去掉空格、全角空格并转小写的表头文本。 */
        QString normalise_header(const QString& text) {
            QString result = text.trimmed().toLower();
            result.remove(QLatin1Char(' '));
            result.remove(QChar(0x3000)); // 全角空格
            result.remove(QLatin1Char('\t'));
            return result;
        }

        /** @return 列名对应的逻辑列；未知列返回 Column::Unknown。 */
        Column column_for(const QString& header) {
            const QString normalised = normalise_header(header);
            for (const auto& alias : column_aliases()) {
                if (normalised == alias.first) {
                    return alias.second;
                }
            }
            return Column::Unknown;
        }

        /**
         * @brief 解析 CSV 文本为二维字段表。
         *
         * 使用状态机而非 `split(',')`，以正确处理：
         *  - 双引号包裹的字段（其中可包含逗号、换行）；
         *  - 字段内的 `""` 转义（还原为单个双引号）；
         *  - `\r\n` 与 `\n` 两种换行。
         */
        QList<QStringList> parse_csv_rows(const QString& text) {
            QList<QStringList> rows;
            QStringList current_row;
            QString field;
            bool in_quotes = false;

            for (int i = 0; i < text.size(); ++i) {
                const QChar character = text.at(i);

                if (in_quotes) {
                    if (character == QLatin1Char('"')) {
                        if (i + 1 < text.size() && text.at(i + 1) == QLatin1Char('"')) {
                            field.append(QLatin1Char('"'));
                            ++i;
                        }
                        else {
                            in_quotes = false;
                        }
                    }
                    else {
                        field.append(character);
                    }
                    continue;
                }

                if (character == QLatin1Char('"')) {
                    in_quotes = true;
                }
                else if (character == QLatin1Char(',')) {
                    current_row.append(field);
                    field.clear();
                }
                else if (character == QLatin1Char('\n')) {
                    current_row.append(field);
                    field.clear();
                    rows.append(current_row);
                    current_row.clear();
                }
                else if (character == QLatin1Char('\r')) {
                    // 交给 '\n' 处理，单独出现的 '\r' 直接忽略
                }
                else {
                    field.append(character);
                }
            }

            if (!field.isEmpty() || !current_row.isEmpty()) {
                current_row.append(field);
                rows.append(current_row);
            }
            return rows;
        }

        /** @return 按 CSV 规则转义后的字段。 */
        QString escape_field(const QString& value) {
            if (!value.contains(QLatin1Char(',')) && !value.contains(QLatin1Char('"')) && !value.contains(QLatin1Char('\n')) && !value.contains(QLatin1Char('\r'))) {
                return value;
            }
            QString escaped = value;
            escaped.replace(QLatin1Char('"'), QStringLiteral("\"\""));
            return QStringLiteral("\"%1\"").arg(escaped);
        }

        /** @brief 统一设置错误。 */
        bool fail(QString* error_message, const QString& message) {
            if (error_message) {
                *error_message = message;
            }
            return false;
        }

        /** @return 去掉 UTF-8 BOM 后的字节串。 */
        QByteArray strip_bom(const QByteArray& data) {
            return data.startsWith(UTF8_BOM) ? data.mid(UTF8_BOM.size()) : data;
        }

    } // namespace

    ScheduleFormat CsvScheduleIo::format() const {
        return ScheduleFormat::Csv;
    }

    QString CsvScheduleIo::display_name() const {
        return format_display_name(ScheduleFormat::Csv);
    }

    QStringList CsvScheduleIo::extensions() const {
        return QStringList{QStringLiteral(".csv"), QStringLiteral(".txt")};
    }

    bool CsvScheduleIo::can_import(const QString& file_path, QString* error_message) const {
        const ScheduleFormat by_extension = format_from_extension(file_path);
        if (by_extension != ScheduleFormat::Csv && by_extension != ScheduleFormat::Unknown) {
            return fail(error_message, QStringLiteral("不是 CSV 文件：%1").arg(file_path));
        }
        if (error_message) {
            error_message->clear();
        }
        return true;
    }

    QString CsvScheduleIo::extension() const {
        return file_extension(ScheduleFormat::Csv);
    }

    QByteArray CsvScheduleIo::serialize(const ScheduleSnapshot& snapshot, QString* error_message) const {
        // 每行 = 一个上课时间段；没有时间段的课程也会输出一行，保证课程本身不丢
        QString text;
        QStringList header = export_header();
        for (int i = 0; i < header.size(); ++i) {
            header[i] = escape_field(header.at(i));
        }
        text += header.join(QLatin1Char(',')) + QStringLiteral("\r\n");

        for (const Course& course : snapshot.courses) {
            const QList<CourseSession> sessions = course.sessions.isEmpty() ? QList<CourseSession>{CourseSession()} : course.sessions;
            const bool placeholder = course.sessions.isEmpty();

            for (const CourseSession& session : sessions) {
                const TimeSlot start_slot = [&snapshot, &session]() {
                    for (const TimeSlot& slot : snapshot.time_slots) {
                        if (slot.index == session.start_slot) {
                            return slot;
                        }
                    }
                    return TimeSlot();
                }();
                const TimeSlot end_slot = [&snapshot, &session]() {
                    for (const TimeSlot& slot : snapshot.time_slots) {
                        if (slot.index == session.end_slot()) {
                            return slot;
                        }
                    }
                    return TimeSlot();
                }();

                QStringList fields;
                fields << escape_field(course.name);
                fields << escape_field(course.code);
                fields << escape_field(session.effective_teacher(course.teacher));
                fields << escape_field(session.effective_location(course.location));
                fields << escape_field(placeholder ? QString() : WeekCalculator::day_name(session.day_of_week));
                fields << escape_field(placeholder ? QString() : QString::number(session.start_slot));
                fields << escape_field(placeholder ? QString() : QString::number(session.slot_count));
                fields << escape_field(placeholder ? QString() : session.weeks.to_expression());
                fields << escape_field(QString::number(course.credits, 'g', 4));
                fields << escape_field(course.notes);
                fields << escape_field(WeekCalculator::format_time(start_slot.start_time));
                fields << escape_field(WeekCalculator::format_time(end_slot.end_time));
                text += fields.join(QLatin1Char(',')) + QStringLiteral("\r\n");
            }
        }

        Q_UNUSED(error_message);
        return UTF8_BOM + text.toUtf8();
    }

    bool CsvScheduleIo::write(const QString& file_path, const ScheduleSnapshot& snapshot, QString* error_message) const {
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

    bool CsvScheduleIo::parse(const QString& file_path, ScheduleSnapshot* out_snapshot, QString* error_message) const {
        QFile file(file_path);
        if (!file.exists()) {
            return fail(error_message, QStringLiteral("文件不存在：%1").arg(file_path));
        }
        if (!file.open(QIODevice::ReadOnly)) {
            return fail(error_message, QStringLiteral("无法读取文件 %1：%2").arg(file_path, file.errorString()));
        }
        return parse_data(file.readAll(), file_path, out_snapshot, error_message);
    }

    bool CsvScheduleIo::parse_data(const QByteArray& data,
        const QString& source_name,
        ScheduleSnapshot* out_snapshot,
        QString* error_message) const {
        if (!out_snapshot) {
            return fail(error_message, QStringLiteral("输出参数为空"));
        }

        // 先按 UTF-8 严格解码：失败通常意味着文件是 GBK 等本地编码
        QStringDecoder decoder(QStringDecoder::Utf8);
        const QString text = decoder(strip_bom(data));
        if (decoder.hasError()) {
            return fail(error_message,
                QStringLiteral("%1 不是 UTF-8 编码的文本。请用记事本 / Excel 另存为“UTF-8”后重试。")
                    .arg(source_name));
        }

        const QList<QStringList> rows = parse_csv_rows(text);
        if (rows.isEmpty()) {
            return fail(error_message, QStringLiteral("%1 内容为空").arg(source_name));
        }

        // 表头 → 列下标
        QMap<Column, int> column_index;
        const QStringList header = rows.first();
        for (int i = 0; i < header.size(); ++i) {
            const Column column = column_for(header.at(i));
            if (column != Column::Unknown && !column_index.contains(column)) {
                column_index.insert(column, i);
            }
        }

        const QStringList required = {QStringLiteral("课程名称"), QStringLiteral("星期"),
            QStringLiteral("开始节次"), QStringLiteral("周次")};
        const QList<Column> required_columns = {Column::Name, Column::Day, Column::StartSlot, Column::Weeks};
        for (int i = 0; i < required_columns.size(); ++i) {
            if (!column_index.contains(required_columns.at(i))) {
                return fail(error_message,
                    QStringLiteral("%1 缺少必需的列“%2”，首行应为表头").arg(source_name, required.at(i)));
            }
        }

        const auto value_of = [&column_index](const QStringList& row, Column column) -> QString {
            const int index = column_index.value(column, -1);
            return (index >= 0 && index < row.size()) ? row.at(index).trimmed() : QString();
        };

        // 同一门课（名称 + 代码）的多个时间段合并为一个 Course
        QMap<QString, int> course_index_by_key;
        QList<Course> courses;
        QMap<int, QPair<QTime, QTime>> observed_times;
        int max_week = 0;
        int skipped_rows = 0;

        for (int row_index = 1; row_index < rows.size(); ++row_index) {
            const QStringList& row = rows.at(row_index);
            const QString name = value_of(row, Column::Name);
            if (name.isEmpty()) {
                continue; // 跳过空行
            }

            const int day_of_week = WeekCalculator::parse_day_of_week(value_of(row, Column::Day));
            if (day_of_week == 0) {
                ++skipped_rows;
                continue;
            }

            bool slot_ok = false;
            const int start_slot = value_of(row, Column::StartSlot).toInt(&slot_ok);
            if (!slot_ok || start_slot < 1) {
                ++skipped_rows;
                continue;
            }

            bool count_ok = false;
            const int parsed_count = value_of(row, Column::SlotCount).toInt(&count_ok);
            const int slot_count = (count_ok && parsed_count >= 1) ? parsed_count : 1;

            QString weeks_error;
            const QString weeks_text = value_of(row, Column::Weeks);
            const WeekMask weeks = WeekMask::from_expression(weeks_text, 0, &weeks_error);
            if (!weeks_error.isEmpty() || weeks.is_empty()) {
                if (error_message) {
                    *error_message = QStringLiteral("%1 第 %2 行的周次无法解析：%3")
                                         .arg(source_name)
                                         .arg(row_index + 1)
                                         .arg(weeks_error.isEmpty() ? QStringLiteral("周次为空") : weeks_error);
                }
                return false;
            }
            max_week = qMax(max_week, weeks.last_week());

            const QString code = value_of(row, Column::Code);
            const QString key = name + QLatin1Char('\u0000') + code;
            if (!course_index_by_key.contains(key)) {
                Course course;
                course.name = name;
                course.code = code;
                course.teacher = value_of(row, Column::Teacher);
                course.location = value_of(row, Column::Location);
                course.notes = value_of(row, Column::Notes);
                bool credits_ok = false;
                const double credits = value_of(row, Column::Credits).toDouble(&credits_ok);
                course.credits = credits_ok ? credits : 0.0;
                course.color = Course::default_color_for(name);
                courses.append(course);
                course_index_by_key.insert(key, courses.size() - 1);
            }
            Course& course = courses[course_index_by_key.value(key)];

            CourseSession session;
            session.day_of_week = day_of_week;
            session.start_slot = start_slot;
            session.slot_count = slot_count;
            session.weeks = weeks;
            session.location = value_of(row, Column::Location);
            session.teacher = value_of(row, Column::Teacher);
            course.sessions.append(session);

            // 若提供了时间列，则用它补全作息表
            const QTime begin = WeekCalculator::parse_time(value_of(row, Column::StartTime));
            const QTime end = WeekCalculator::parse_time(value_of(row, Column::EndTime));
            if (begin.isValid()) {
                observed_times.insert(start_slot, {begin, end});
            }
        }

        if (courses.isEmpty()) {
            return fail(error_message,
                QStringLiteral("%1 中没有解析到任何课程（已跳过 %2 行无效数据）").arg(source_name).arg(skipped_rows));
        }

        ScheduleSnapshot snapshot;
        snapshot.semester.name = QStringLiteral("CSV 导入 · %1").arg(QFileInfo(source_name).completeBaseName());
        snapshot.semester.start_date = WeekCalculator::monday_of(QDate::currentDate());
        // CSV 不含学期信息：按内容推断一个不早于 20 周的学期，随后由 ImportManager
        // 在存在“当前学期”时替换为真实学期。
        snapshot.semester.total_weeks = qBound(1, qMax(20, max_week), WeekMask::MAX_WEEKS);

        if (observed_times.isEmpty()) {
            snapshot.time_slots = TimeSlot::default_slots();
        }
        else {
            int max_slot = 0;
            for (auto iterator = observed_times.constBegin(); iterator != observed_times.constEnd(); ++iterator) {
                max_slot = qMax(max_slot, iterator.key());
            }
            for (int index = 1; index <= max_slot; ++index) {
                TimeSlot slot;
                slot.index = index;
                slot.label = QStringLiteral("第 %1 节").arg(index);
                if (observed_times.contains(index)) {
                    slot.start_time = observed_times.value(index).first;
                    slot.end_time = observed_times.value(index).second;
                }
                snapshot.time_slots.append(slot);
            }
        }

        for (Course& course : courses) {
            course.semester_id = snapshot.semester.id;
            course.ensure_session_ids();
        }
        snapshot.courses = courses;

        *out_snapshot = snapshot;
        if (error_message) {
            error_message->clear();
        }
        return true;
    }

} // namespace Schedule
