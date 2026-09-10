#include "data/ScheduleJson.h"

#include "core/service/WeekCalculator.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonValue>
#include <QSaveFile>
#include <QUuid>

namespace Schedule {

    namespace {

        /** @return 新建对象的 UUID 字符串。 */
        QString new_id() {
            return QUuid::createUuid().toString(QUuid::WithoutBraces);
        }

        /** @return 日期序列化为 ISO `yyyy-MM-dd`；无效日期返回空串。 */
        QString date_to_text(const QDate& date) {
            return date.isValid() ? date.toString(Qt::ISODate) : QString();
        }

        /** @return 时间序列化为 `HH:mm`；无效时间返回空串。 */
        QString time_to_text(const QTime& time) {
            return time.isValid() ? time.toString(QStringLiteral("HH:mm")) : QString();
        }

        /** @return 日期时间序列化为 ISO 扩展格式；无效值返回空串。 */
        QString datetime_to_text(const QDateTime& datetime) {
            return datetime.isValid() ? datetime.toString(Qt::ISODate) : QString();
        }

        /** @return JSON 文本字段；字段缺失或类型不符时返回 fallback。 */
        QString string_value(const QJsonObject& object, const QString& key, const QString& fallback = QString()) {
            const QJsonValue value = object.value(key);
            return value.isString() ? value.toString() : fallback;
        }

        /** @return JSON 整数字段；字段缺失或类型不符时返回 fallback。 */
        int int_value(const QJsonObject& object, const QString& key, int fallback = 0) {
            const QJsonValue value = object.value(key);
            return value.isDouble() ? value.toInt(fallback) : fallback;
        }

        /** @return JSON 浮点字段；字段缺失或类型不符时返回 fallback。 */
        double double_value(const QJsonObject& object, const QString& key, double fallback = 0.0) {
            const QJsonValue value = object.value(key);
            return value.isDouble() ? value.toDouble(fallback) : fallback;
        }

        /** @return JSON 布尔字段，兼容 "1"/"0" 字符串写法。 */
        bool bool_value(const QJsonObject& object, const QString& key, bool fallback = false) {
            const QJsonValue value = object.value(key);
            if (value.isBool()) {
                return value.toBool();
            }
            if (value.isString()) {
                const QString text = value.toString().trimmed();
                return text == QStringLiteral("1") || text.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0;
            }
            if (value.isDouble()) {
                return value.toInt() != 0;
            }
            return fallback;
        }

        /** @return 解析 ISO 日期；失败返回无效 QDate。 */
        QDate parse_date(const QString& text) {
            return QDate::fromString(text.trimmed(), Qt::ISODate);
        }

        /** @return 解析 ISO 日期时间；失败返回无效 QDateTime。 */
        QDateTime parse_datetime(const QString& text) {
            return QDateTime::fromString(text.trimmed(), Qt::ISODate);
        }

        /** @brief 统一设置错误信息辅助。 */
        bool fail(QString* error_message, const QString& message) {
            if (error_message) {
                *error_message = message;
            }
            return false;
        }

    } // namespace

    QString ScheduleJson::format_name() {
        return QStringLiteral("schedule");
    }

    // --------------------------------------------------------- 领域对象 → JSON

    QJsonObject ScheduleJson::to_json(const Semester& semester) {
        QJsonObject object;
        object.insert(QStringLiteral("id"), semester.id);
        object.insert(QStringLiteral("name"), semester.name);
        object.insert(QStringLiteral("start_date"), date_to_text(semester.start_date));
        object.insert(QStringLiteral("total_weeks"), semester.total_weeks);
        object.insert(QStringLiteral("is_current"), semester.is_current);
        object.insert(QStringLiteral("created_at"), datetime_to_text(semester.created_at));
        object.insert(QStringLiteral("updated_at"), datetime_to_text(semester.updated_at));
        return object;
    }

    QJsonObject ScheduleJson::to_json(const TimeSlot& time_slot) {
        QJsonObject object;
        object.insert(QStringLiteral("index"), time_slot.index);
        object.insert(QStringLiteral("label"), time_slot.label);
        object.insert(QStringLiteral("start"), time_to_text(time_slot.start_time));
        object.insert(QStringLiteral("end"), time_to_text(time_slot.end_time));
        return object;
    }

    QJsonObject ScheduleJson::to_json(const CourseSession& session) {
        QJsonObject object;
        object.insert(QStringLiteral("id"), session.id);
        object.insert(QStringLiteral("day_of_week"), session.day_of_week);
        object.insert(QStringLiteral("start_slot"), session.start_slot);
        object.insert(QStringLiteral("slot_count"), session.slot_count);
        // `weeks` 便于人工阅读与编辑，`week_bits` 才是权威数据
        object.insert(QStringLiteral("weeks"), session.weeks.to_expression());
        object.insert(QStringLiteral("week_bits"), QString::number(session.weeks.bits()));
        object.insert(QStringLiteral("location"), session.location);
        object.insert(QStringLiteral("teacher"), session.teacher);
        return object;
    }

    QJsonObject ScheduleJson::to_json(const Course& course) {
        QJsonObject object;
        object.insert(QStringLiteral("id"), course.id);
        object.insert(QStringLiteral("semester_id"), course.semester_id);
        object.insert(QStringLiteral("name"), course.name);
        object.insert(QStringLiteral("code"), course.code);
        object.insert(QStringLiteral("teacher"), course.teacher);
        object.insert(QStringLiteral("location"), course.location);
        object.insert(QStringLiteral("color"), course.color);
        object.insert(QStringLiteral("credits"), course.credits);
        object.insert(QStringLiteral("notes"), course.notes);

        QJsonArray sessions;
        for (const CourseSession& session : course.sessions) {
            sessions.append(to_json(session));
        }
        object.insert(QStringLiteral("sessions"), sessions);
        return object;
    }

    QJsonObject ScheduleJson::to_json(const ScheduleSnapshot& snapshot) {
        QJsonObject object;
        object.insert(QStringLiteral("semester"), to_json(snapshot.semester));

        QJsonArray time_slots;
        for (const TimeSlot& time_slot : snapshot.time_slots) {
            time_slots.append(to_json(time_slot));
        }
        object.insert(QStringLiteral("time_slots"), time_slots);

        QJsonArray courses;
        for (const Course& course : snapshot.courses) {
            courses.append(to_json(course));
        }
        object.insert(QStringLiteral("courses"), courses);
        return object;
    }

    // --------------------------------------------------------- JSON → 领域对象

    bool ScheduleJson::semester_from_json(const QJsonObject& object, Semester* out_semester, QString* error_message) {
        if (!out_semester) {
            return fail(error_message, QStringLiteral("输出参数为空"));
        }

        Semester semester;
        semester.id = string_value(object, QStringLiteral("id"));
        if (semester.id.isEmpty()) {
            semester.id = new_id();
        }
        semester.name = string_value(object, QStringLiteral("name"));
        semester.start_date = parse_date(string_value(object, QStringLiteral("start_date")));
        semester.total_weeks = int_value(object, QStringLiteral("total_weeks"), 20);
        semester.is_current = bool_value(object, QStringLiteral("is_current"));
        semester.created_at = parse_datetime(string_value(object, QStringLiteral("created_at")));
        semester.updated_at = parse_datetime(string_value(object, QStringLiteral("updated_at")));

        if (!semester.start_date.isValid()) {
            return fail(error_message, QStringLiteral("学期 \"%1\" 的起始日期缺失或格式非法（应为 yyyy-MM-dd）").arg(semester.name));
        }
        if (!semester.is_valid(error_message)) {
            return false;
        }

        *out_semester = semester;
        if (error_message) {
            error_message->clear();
        }
        return true;
    }

    bool ScheduleJson::time_slot_from_json(const QJsonObject& object, TimeSlot* out_slot, QString* error_message) {
        if (!out_slot) {
            return fail(error_message, QStringLiteral("输出参数为空"));
        }

        TimeSlot time_slot;
        time_slot.index = int_value(object, QStringLiteral("index"));
        time_slot.label = string_value(object, QStringLiteral("label"));
        // 同时接受 `start`/`end` 与 `start_time`/`end_time` 两种命名，便于兼容手工编辑的文件
        time_slot.start_time = WeekCalculator::parse_time(
            object.contains(QStringLiteral("start")) ? string_value(object, QStringLiteral("start"))
                                                     : string_value(object, QStringLiteral("start_time")));
        time_slot.end_time = WeekCalculator::parse_time(
            object.contains(QStringLiteral("end")) ? string_value(object, QStringLiteral("end"))
                                                   : string_value(object, QStringLiteral("end_time")));

        if (!time_slot.is_valid()) {
            return fail(error_message, QStringLiteral("节次 %1 的定义非法（时间需成对出现且结束晚于开始）").arg(time_slot.index));
        }

        *out_slot = time_slot;
        if (error_message) {
            error_message->clear();
        }
        return true;
    }

    bool ScheduleJson::session_from_json(const QJsonObject& object, CourseSession* out_session, QString* error_message) {
        if (!out_session) {
            return fail(error_message, QStringLiteral("输出参数为空"));
        }

        CourseSession session;
        session.id = string_value(object, QStringLiteral("id"));
        if (session.id.isEmpty()) {
            session.id = new_id();
        }
        session.day_of_week = int_value(object, QStringLiteral("day_of_week"), 1);
        session.start_slot = int_value(object, QStringLiteral("start_slot"), 1);
        session.slot_count = int_value(object, QStringLiteral("slot_count"), 1);
        session.location = string_value(object, QStringLiteral("location"));
        session.teacher = string_value(object, QStringLiteral("teacher"));

        const QString bits_text = string_value(object, QStringLiteral("week_bits"));
        bool bits_ok = false;
        if (!bits_text.isEmpty()) {
            const quint64 bits = bits_text.toULongLong(&bits_ok);
            if (bits_ok) {
                session.weeks = WeekMask::from_bits(bits);
            }
        }
        if (!bits_ok) {
            // 回退：按可读表达式解析。这里不限制学期周数（学期可能后于课程解析），
            // 越界问题交由 ConflictDetector 统一报告。
            const QString expression = string_value(object, QStringLiteral("weeks"));
            session.weeks = WeekMask::from_expression(expression, 0, error_message);
            if (error_message && !error_message->isEmpty()) {
                return false;
            }
        }

        if (!session.is_valid(-1, error_message)) {
            return false;
        }

        *out_session = session;
        if (error_message) {
            error_message->clear();
        }
        return true;
    }

    bool ScheduleJson::course_from_json(const QJsonObject& object, Course* out_course, QString* error_message) {
        if (!out_course) {
            return fail(error_message, QStringLiteral("输出参数为空"));
        }

        Course course;
        course.id = string_value(object, QStringLiteral("id"));
        if (course.id.isEmpty()) {
            course.id = new_id();
        }
        course.semester_id = string_value(object, QStringLiteral("semester_id"));
        course.name = string_value(object, QStringLiteral("name"));
        course.code = string_value(object, QStringLiteral("code"));
        course.teacher = string_value(object, QStringLiteral("teacher"));
        course.location = string_value(object, QStringLiteral("location"));
        course.color = string_value(object, QStringLiteral("color"));
        course.credits = double_value(object, QStringLiteral("credits"));
        course.notes = string_value(object, QStringLiteral("notes"));

        const QJsonValue sessions_value = object.value(QStringLiteral("sessions"));
        if (sessions_value.isArray()) {
            const QJsonArray sessions = sessions_value.toArray();
            for (const QJsonValue& entry : sessions) {
                if (!entry.isObject()) {
                    continue;
                }
                CourseSession session;
                if (!session_from_json(entry.toObject(), &session, error_message)) {
                    if (error_message) {
                        *error_message = QStringLiteral("课程 \"%1\"：%2").arg(course.name, *error_message);
                    }
                    return false;
                }
                course.sessions.append(session);
            }
        }

        if (!course.is_valid(-1, error_message)) {
            return false;
        }

        *out_course = course;
        if (error_message) {
            error_message->clear();
        }
        return true;
    }

    bool ScheduleJson::snapshot_from_json(const QJsonObject& object, ScheduleSnapshot* out_snapshot, QString* error_message) {
        if (!out_snapshot) {
            return fail(error_message, QStringLiteral("输出参数为空"));
        }

        const QString format = string_value(object, QStringLiteral("format"));
        if (!format.isEmpty() && format != format_name()) {
            return fail(error_message, QStringLiteral("不是课表 JSON 文件（format = \"%1\"）").arg(format));
        }
        const int version = int_value(object, QStringLiteral("version"), FORMAT_VERSION);
        if (version > FORMAT_VERSION) {
            return fail(error_message,
                QStringLiteral("文件格式版本 %1 高于当前应用支持的 %2，请升级应用").arg(version).arg(FORMAT_VERSION));
        }

        ScheduleSnapshot snapshot;
        if (!semester_from_json(object.value(QStringLiteral("semester")).toObject(), &snapshot.semester, error_message)) {
            return false;
        }

        const QJsonValue time_slots_value = object.value(QStringLiteral("time_slots"));
        if (time_slots_value.isArray()) {
            const QJsonArray time_slots = time_slots_value.toArray();
            for (const QJsonValue& entry : time_slots) {
                if (!entry.isObject()) {
                    continue;
                }
                TimeSlot time_slot;
                if (!time_slot_from_json(entry.toObject(), &time_slot, error_message)) {
                    return false;
                }
                snapshot.time_slots.append(time_slot);
            }
        }
        if (snapshot.time_slots.isEmpty()) {
            // 缺失作息表时回退默认值，保证导入结果可用
            snapshot.time_slots = TimeSlot::default_slots();
        }

        const QJsonValue courses_value = object.value(QStringLiteral("courses"));
        if (courses_value.isArray()) {
            const QJsonArray courses = courses_value.toArray();
            for (const QJsonValue& entry : courses) {
                if (!entry.isObject()) {
                    continue;
                }
                Course course;
                if (!course_from_json(entry.toObject(), &course, error_message)) {
                    return false;
                }
                if (course.semester_id.isEmpty()) {
                    course.semester_id = snapshot.semester.id;
                }
                snapshot.courses.append(course);
            }
        }

        *out_snapshot = snapshot;
        if (error_message) {
            error_message->clear();
        }
        return true;
    }

    // ---------------------------------------------------------------- 文档读写

    QByteArray ScheduleJson::to_document(const ScheduleSnapshot& snapshot, bool pretty) {
        QJsonObject root = to_json(snapshot);
        root.insert(QStringLiteral("format"), format_name());
        root.insert(QStringLiteral("version"), FORMAT_VERSION);
        root.insert(QStringLiteral("exported_at"), datetime_to_text(QDateTime::currentDateTime()));

        const QJsonDocument document(root);
        return pretty ? document.toJson(QJsonDocument::Indented) : document.toJson(QJsonDocument::Compact);
    }

    bool ScheduleJson::from_document(const QByteArray& data, ScheduleSnapshot* out_snapshot, QString* error_message) {
        QJsonParseError parse_error{};
        const QJsonDocument document = QJsonDocument::fromJson(data, &parse_error);
        if (document.isNull()) {
            return fail(error_message, QStringLiteral("JSON 解析失败：%1（偏移 %2）").arg(parse_error.errorString()).arg(parse_error.offset));
        }
        if (!document.isObject()) {
            return fail(error_message, QStringLiteral("JSON 顶层必须是对象"));
        }
        return snapshot_from_json(document.object(), out_snapshot, error_message);
    }

    bool ScheduleJson::write_file(const QString& file_path, const ScheduleSnapshot& snapshot, QString* error_message) {
        if (file_path.isEmpty()) {
            return fail(error_message, QStringLiteral("导出路径为空"));
        }

        // 父目录可能不存在（用户首次导出），先补建
        const QFileInfo info(file_path);
        const QDir parent = info.absoluteDir();
        if (!parent.exists() && !parent.mkpath(QStringLiteral("."))) {
            return fail(error_message, QStringLiteral("无法创建导出目录：%1").arg(parent.absolutePath()));
        }

        // QSaveFile：写入临时文件后原子替换，避免中途失败留下半个文件
        QSaveFile file(file_path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            return fail(error_message, QStringLiteral("无法写入文件 %1：%2").arg(file_path, file.errorString()));
        }
        const QByteArray document = to_document(snapshot, true);
        if (file.write(document) != document.size()) {
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

    bool ScheduleJson::read_file(const QString& file_path, ScheduleSnapshot* out_snapshot, QString* error_message) {
        QFile file(file_path);
        if (!file.exists()) {
            return fail(error_message, QStringLiteral("文件不存在：%1").arg(file_path));
        }
        if (!file.open(QIODevice::ReadOnly)) {
            return fail(error_message, QStringLiteral("无法读取文件 %1：%2").arg(file_path, file.errorString()));
        }
        return from_document(file.readAll(), out_snapshot, error_message);
    }

} // namespace Schedule
