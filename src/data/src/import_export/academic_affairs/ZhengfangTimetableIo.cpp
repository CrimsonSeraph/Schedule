#include "data/import_export/academic_affairs/ZhengfangTimetableIo.h"

#include "core/model/Course.h"
#include "core/model/CourseSession.h"
#include "core/model/Semester.h"
#include "core/model/TimeSlot.h"
#include "core/model/WeekMask.h"
#include "core/service/WeekCalculator.h"
#include "data/import_export/academic_affairs/CharsetUtil.h"
#include "data/import_export/ImportExportTypes.h"

#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QList>
#include <QMap>
#include <QPair>
#include <QRegularExpression>
#include <QStringList>

#include <algorithm>

namespace Schedule {

    namespace {

        /** 未声明 `unitCount` 时的默认节次数（正方教务常规作息为 12 节）。 */
        constexpr int DEFAULT_UNIT_COUNT = 12;

        /** 周次位图的最小辨识长度：短于此长度的 0/1 串不认为是周次位图。 */
        constexpr int WEEK_BITMAP_MIN_LENGTH = 30;

        /** 星期数：周一至周日。 */
        constexpr int DAYS_PER_WEEK = 7;

        /** 一条 `TaskActivity` 的解析结果（合并前）。 */
        struct Activity {
            /** 课程代码，如 `04311190.02`。 */
            QString course_code;

            /** 课程名称，如 `劳动教育实践——蛋糕面包制作`。 */
            QString course_name;

            /** 教师名，多个教师以`,`连接；可能为空。 */
            QString teacher;

            /** 教室名，如 `5J310(东校区)`。 */
            QString room;

            /** 上课周次（1 起）。 */
            QList<int> weeks;

            /** 命中的单元格：`(星期, 节次)`，二者均从 0 开始。 */
            QList<QPair<int, int>> cells;
        };

        /** `actTeachers = [{id:1,name:"张三"}]` 的位置与解析出的姓名。 */
        struct TeacherBlock {
            /** 在整段文本中的起始下标，用于“取紧邻其上的那一块”。 */
            int position = 0;

            /** 以`,`连接的教师姓名。 */
            QString names;
        };

        /** @brief 统一设置错误。 */
        bool fail(QString* error_message, const QString& message) {
            if (error_message) {
                *error_message = message;
            }
            return false;
        }

        /** @return 判断该字符串是否是一段周次位图（足够长且仅由 0/1 组成）。 */
        bool is_week_bitmap(const QString& text) {
            if (text.size() < WEEK_BITMAP_MIN_LENGTH) {
                return false;
            }
            for (const QChar character : text) {
                if (character != QLatin1Char('0') && character != QLatin1Char('1')) {
                    return false;
                }
            }
            return true;
        }

        /** @brief 反转义 HTML 实体中常见的几种写法（课表标题里的 `&nbsp;` 等）。 */
        QString unescape_html(const QString& text) {
            QString result = text;
            result.replace(QLatin1String("&nbsp;"), QStringLiteral(" "));
            result.replace(QLatin1String("&lt;"), QStringLiteral("<"));
            result.replace(QLatin1String("&gt;"), QStringLiteral(">"));
            result.replace(QLatin1String("&quot;"), QStringLiteral("\""));
            result.replace(QLatin1String("&#39;"), QStringLiteral("'"));
            result.replace(QLatin1String("&amp;"), QStringLiteral("&")); // 必须最后处理
            return result.simplified();
        }

        /** @return 去掉课程名尾部的 `(课程代码)`，保留纯名称。 */
        QString strip_trailing_code(const QString& raw_name) {
            static const QRegularExpression trailing(QStringLiteral(R"(\s*\([^()]*\)\s*$)"));
            QString name = raw_name;
            name.remove(trailing);
            return name.trimmed();
        }

        /** @return 取出 `19626(04311190.02)` 中括号内的课程代码；无括号时原样返回。 */
        QString extract_course_code(const QString& raw_code) {
            static const QRegularExpression bracketed(QStringLiteral(R"(\(([^()]*)\)\s*$)"));
            const QRegularExpressionMatch match = bracketed.match(raw_code.trimmed());
            if (match.hasMatch()) {
                return match.captured(1).trimmed();
            }
            return raw_code.trimmed();
        }

        /** @return 由 `<h3>` 或“20xx-20xx 学年”片段推断出的学期名。 */
        QString extract_semester_name(const QString& text, const QString& source_name) {
            static const QRegularExpression heading(
                QStringLiteral(R"(<h3[^>]*>([^<]*)</h3>)"),
                QRegularExpression::CaseInsensitiveOption);
            QRegularExpressionMatchIterator headings = heading.globalMatch(text);
            while (headings.hasNext()) {
                const QString candidate = unescape_html(headings.next().captured(1));
                if (!candidate.isEmpty()) {
                    return candidate;
                }
            }

            static const QRegularExpression academic_year(QStringLiteral(R"((\d{4}\s*-\s*\d{4}\s*学年[^<\r\n]{0,16}))"));
            const QRegularExpressionMatch year_match = academic_year.match(text);
            if (year_match.hasMatch()) {
                return unescape_html(year_match.captured(1));
            }

            const QString base_name = QFileInfo(source_name).completeBaseName();
            return base_name.isEmpty() ? QStringLiteral("正方教务导入")
                                       : QStringLiteral("正方教务导入 · %1").arg(base_name);
        }

        /** @return 页面中声明的节次总数（`var unitCount = 12;`）。 */
        int extract_unit_count(const QString& text) {
            static const QRegularExpression pattern(QStringLiteral(R"(unitCount\s*=\s*(\d+)\s*;)"));
            const QRegularExpressionMatch match = pattern.match(text);
            if (match.hasMatch()) {
                const int value = match.captured(1).toInt();
                if (value > 0 && value <= 30) {
                    return value;
                }
            }
            return DEFAULT_UNIT_COUNT;
        }

        /** @brief 收集全部 `actTeachers = [...]`（或退化的 `teachers = [...]`）块。 */
        QList<TeacherBlock> collect_teacher_blocks(const QString& text) {
            // 模式中含 `)"`，必须使用自定义分隔符 RX，否则原始字符串会提前结束
            static const QRegularExpression name_pattern(QStringLiteral(R"RX(name\s*:\s*"([^"]*)")RX"));

            // name_pattern 是函数内静态对象，无需被 lambda 捕获
            const auto collect = [](const QString& body, int position) {
                TeacherBlock block;
                block.position = position;
                QStringList names;
                QRegularExpressionMatchIterator matches = name_pattern.globalMatch(body);
                while (matches.hasNext()) {
                    const QString name = matches.next().captured(1).trimmed();
                    if (!name.isEmpty()) {
                        names.append(name);
                    }
                }
                block.names = names.join(QStringLiteral(","));
                return block;
            };

            QList<TeacherBlock> blocks;

            // 主路径：actTeachers 是参与排课的教师（已剔除实验助理）
            static const QRegularExpression act_teachers(
                QStringLiteral(R"(actTeachers\s*=\s*\[(.*?)\]\s*;)"),
                QRegularExpression::DotMatchesEverythingOption);
            QRegularExpressionMatchIterator matches = act_teachers.globalMatch(text);
            while (matches.hasNext()) {
                const QRegularExpressionMatch match = matches.next();
                blocks.append(collect(match.captured(1), static_cast<int>(match.capturedStart())));
            }
            if (!blocks.isEmpty()) {
                return blocks;
            }

            // 兜底：部分版本只输出 teachers（可能含实验助理，仅用于显示教师名）
            static const QRegularExpression plain_teachers(
                QStringLiteral(R"(\bteachers\s*=\s*\[(.*?)\]\s*;)"),
                QRegularExpression::DotMatchesEverythingOption);
            matches = plain_teachers.globalMatch(text);
            while (matches.hasNext()) {
                const QRegularExpressionMatch match = matches.next();
                blocks.append(collect(match.captured(1), static_cast<int>(match.capturedStart())));
            }
            return blocks;
        }

        /** @return 位于 position 之前且最近的一个教师块；没有则返回 nullptr。 */
        const TeacherBlock* nearest_teacher_block(const QList<TeacherBlock>& blocks, int position) {
            const TeacherBlock* found = nullptr;
            for (const TeacherBlock& block : blocks) {
                if (block.position >= position) {
                    break;
                }
                found = &block;
            }
            return found;
        }

        /** @brief 逐个解析 `TaskActivity` 及其 `index` 归属。 */
        QList<Activity> parse_activities(const QString& text, int unit_count) {
            // 参数里的 join(',') 会污染「双引号字符串」的抽取，先整体剔除
            static const QRegularExpression activity_pattern(
                QStringLiteral(R"(new\s+TaskActivity\s*\((.*?)\)\s*;)"),
                QRegularExpression::DotMatchesEverythingOption);
            static const QRegularExpression index_pattern(
                QStringLiteral(R"(index\s*=\s*(\d+)\s*\*\s*unitCount\s*\+\s*(\d+)\s*;)"));
            // 同上：模式本身就以 `"` 结尾，需自定义分隔符
            static const QRegularExpression quoted(QStringLiteral(R"RX("([^"]*)")RX"));

            const QList<TeacherBlock> teacher_blocks = collect_teacher_blocks(text);

            // 把 TaskActivity 与 index 赋值按出现顺序合并成一条事件流：
            // 一个 TaskActivity 之后可能跟随多个 index 赋值，全部归属该活动。
            struct Event {
                int position = 0;
                bool is_activity = false;
                int day = 0;
                int unit = 0;
                Activity activity;
            };

            QList<Event> events;

            QRegularExpressionMatchIterator activities = activity_pattern.globalMatch(text);
            while (activities.hasNext()) {
                const QRegularExpressionMatch match = activities.next();

                QString arguments = match.captured(1);
                arguments.remove(QStringLiteral("join(',')"));
                arguments.remove(QStringLiteral("join(\",\")"));

                QStringList parameters;
                QRegularExpressionMatchIterator literals = quoted.globalMatch(arguments);
                while (literals.hasNext()) {
                    parameters.append(literals.next().captured(1));
                }

                // 以「周次位图」为锚点定位其余字段：这样即使不同版本在
                // TaskActivity 前置了数量不定的教师参数，也能取到正确的字段。
                int bitmap_index = -1;
                for (int i = 0; i < parameters.size(); ++i) {
                    if (is_week_bitmap(parameters.at(i))) {
                        bitmap_index = i;
                        break;
                    }
                }
                if (bitmap_index < 4) {
                    continue; // 结构不符，跳过该条
                }

                Event event;
                event.position = static_cast<int>(match.capturedStart());
                event.is_activity = true;
                event.activity.course_code = extract_course_code(parameters.at(bitmap_index - 4));
                event.activity.course_name = strip_trailing_code(parameters.at(bitmap_index - 3));
                event.activity.room = parameters.at(bitmap_index - 1).trimmed();

                const QString bitmap = parameters.at(bitmap_index);
                for (int i = 0; i < bitmap.size(); ++i) {
                    // 下标 i 即第 i 周；下标 0 不表示任何周，直接跳过
                    if (i >= 1 && bitmap.at(i) == QLatin1Char('1')) {
                        event.activity.weeks.append(i);
                    }
                }

                if (const TeacherBlock* block = nearest_teacher_block(teacher_blocks, event.position)) {
                    event.activity.teacher = block->names;
                }

                events.append(event);
            }

            QRegularExpressionMatchIterator indices = index_pattern.globalMatch(text);
            while (indices.hasNext()) {
                const QRegularExpressionMatch match = indices.next();
                Event event;
                event.position = static_cast<int>(match.capturedStart());
                event.is_activity = false;
                event.day = match.captured(1).toInt();
                event.unit = match.captured(2).toInt();
                events.append(event);
            }

            std::stable_sort(events.begin(), events.end(), [](const Event& left, const Event& right) {
                return left.position < right.position;
            });

            QList<Activity> result;
            for (const Event& event : events) {
                if (event.is_activity) {
                    result.append(event.activity);
                    continue;
                }
                if (result.isEmpty()) {
                    continue; // index 出现在任何 TaskActivity 之前，无法归属
                }
                if (event.day < 0 || event.day >= DAYS_PER_WEEK || event.unit < 0 || event.unit >= unit_count) {
                    continue; // 越界单元格直接丢弃，避免污染课表
                }
                result.last().cells.append(qMakePair(event.day, event.unit));
            }

            return result;
        }

        /**
         * @brief 把一个活动的单元格合并为上课时间段。
         *
         * 同一活动可能被挂到多个连续节次上（如周一 1-4 节），此处按
         * 「同一天 + 节次连续」合并；不连续则拆成多个时间段。
         */
        QList<CourseSession> build_sessions(const Activity& activity) {
            QMap<int, QList<int>> units_by_day;
            for (const QPair<int, int>& cell : activity.cells) {
                units_by_day[cell.first].append(cell.second);
            }

            const WeekMask weeks = WeekMask::from_weeks(activity.weeks);

            QList<CourseSession> sessions;
            for (auto iterator = units_by_day.constBegin(); iterator != units_by_day.constEnd(); ++iterator) {
                QList<int> units = iterator.value();
                std::sort(units.begin(), units.end());
                units.erase(std::unique(units.begin(), units.end()), units.end());
                if (units.isEmpty()) {
                    continue;
                }

                const auto make_session = [&](int first_unit, int last_unit) {
                    CourseSession session;
                    session.day_of_week = iterator.key() + 1; // 模型从 1 开始
                    session.start_slot = first_unit + 1;
                    session.slot_count = last_unit - first_unit + 1;
                    session.weeks = weeks;
                    session.location = activity.room;
                    session.teacher = activity.teacher;
                    sessions.append(session);
                };

                int run_start = units.first();
                int previous = units.first();
                for (int i = 1; i < units.size(); ++i) {
                    if (units.at(i) == previous + 1) {
                        previous = units.at(i);
                        continue;
                    }
                    make_session(run_start, previous);
                    run_start = units.at(i);
                    previous = units.at(i);
                }
                make_session(run_start, previous);
            }

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

    } // namespace

    ScheduleFormat ZhengfangTimetableIo::format() const {
        return ScheduleFormat::ZhengfangHtml;
    }

    QString ZhengfangTimetableIo::display_name() const {
        return QStringLiteral("正方教务课表");
    }

    QStringList ZhengfangTimetableIo::extensions() const {
        // .xls 是教务系统「导出」时的实际命名；.html/.htm 是页面另存后的常见后缀
        return QStringList{QStringLiteral(".xls"), QStringLiteral(".html"), QStringLiteral(".htm")};
    }

    bool ZhengfangTimetableIo::can_import(const QString& file_path, QString* error_message) const {
        QFile file(file_path);
        if (!file.exists()) {
            return fail(error_message, QStringLiteral("文件不存在：%1").arg(file_path));
        }
        if (!file.open(QIODevice::ReadOnly)) {
            return fail(error_message, QStringLiteral("无法读取文件 %1：%2").arg(file_path, file.errorString()));
        }
        // 只需要头部即可完成嗅探：正方课表的标志串都出现在页面靠前的位置
        const QByteArray head = file.read(64 * 1024);
        file.close();

        // 内容嗅探复用全局的 format_from_content，避免标志串在两处各写一份而产生漂移
        if (format_from_content(head) != ScheduleFormat::ZhengfangHtml) {
            return fail(error_message, QStringLiteral("不是正方教务课表页面（未找到 TaskActivity / 课表表格结构）"));
        }
        if (error_message) {
            error_message->clear();
        }
        return true;
    }

    bool ZhengfangTimetableIo::parse(const QString& file_path, ScheduleSnapshot* out_snapshot, QString* error_message) const {
        QFile file(file_path);
        if (!file.exists()) {
            return fail(error_message, QStringLiteral("文件不存在：%1").arg(file_path));
        }
        if (!file.open(QIODevice::ReadOnly)) {
            return fail(error_message, QStringLiteral("无法读取文件 %1：%2").arg(file_path, file.errorString()));
        }
        const QByteArray data = file.readAll();
        file.close();

        return parse_data(data, file_path, out_snapshot, error_message);
    }

    bool ZhengfangTimetableIo::parse_data(const QByteArray& data,
        const QString& source_name,
        ScheduleSnapshot* out_snapshot,
        QString* error_message) const {
        if (!out_snapshot) {
            return fail(error_message, QStringLiteral("输出快照为空"));
        }
        if (data.isEmpty()) {
            return fail(error_message, QStringLiteral("内容为空"));
        }
        if (format_from_content(data) != ScheduleFormat::ZhengfangHtml) {
            return fail(error_message,
                QStringLiteral("%1 不是正方教务课表页面（未找到 TaskActivity / 课表表格结构）").arg(source_name));
        }
        // GBK 是正方导出的默认编码；decode_html_bytes 会先看 <meta charset>
        const QString text = decode_html_bytes(data, nullptr);

        const int unit_count = extract_unit_count(text);
        const QList<Activity> activities = parse_activities(text, unit_count);
        if (activities.isEmpty()) {
            return fail(error_message,
                QStringLiteral("%1 中未解析到任何课程安排，请确认导出的是“个人课程表”页面").arg(source_name));
        }

        // 按课程代码聚合：同一门课的多个时间段（不同教室 / 不同周次）合并为一门课程
        QList<Course> courses;
        QHash<QString, int> course_index;
        int max_week = 0;

        for (const Activity& activity : activities) {
            const QList<CourseSession> sessions = build_sessions(activity);
            if (sessions.isEmpty()) {
                continue;
            }
            for (const CourseSession& session : sessions) {
                max_week = qMax(max_week, session.weeks.last_week());
            }

            const QString key = activity.course_code.isEmpty() ? activity.course_name : activity.course_code;
            int index = course_index.value(key, -1);
            if (index < 0) {
                Course course;
                course.name = activity.course_name;
                course.code = activity.course_code;
                courses.append(course);
                index = courses.size() - 1;
                course_index.insert(key, index);
            }
            courses[index].sessions.append(sessions);
        }

        if (courses.isEmpty()) {
            return fail(error_message,
                QStringLiteral("%1 中未解析到任何课程安排，请确认导出的是“个人课程表”页面").arg(source_name));
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
        snapshot.semester.name = extract_semester_name(text, source_name);
        // 正方课表页面不含学期起始日期：与 CSV 导入一致，先按本周一合成，
        // 若应用中已存在当前学期，ImportManager 会改用真实学期。
        snapshot.semester.start_date = WeekCalculator::monday_of(QDate::currentDate());
        snapshot.semester.total_weeks = qBound(1, qMax(20, max_week), WeekMask::MAX_WEEKS);
        snapshot.time_slots = build_time_slots(unit_count);
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

} // namespace Schedule
