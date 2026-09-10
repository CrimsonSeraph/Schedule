#include "core/service/ConflictDetector.h"

#include "core/service/WeekCalculator.h"

#include <algorithm>

namespace Schedule {

    namespace {

        /** @return 问题排序键：类型优先，其次星期、周次、课程 id，保证展示顺序稳定。 */
        bool conflict_less_than(const Conflict& left, const Conflict& right) {
            if (left.type != right.type) {
                return static_cast<int>(left.type) < static_cast<int>(right.type);
            }
            if (left.day_of_week != right.day_of_week) {
                return left.day_of_week < right.day_of_week;
            }
            if (left.week != right.week) {
                return left.week < right.week;
            }
            if (left.course_id_a != right.course_id_a) {
                return left.course_id_a < right.course_id_a;
            }
            return left.course_id_b < right.course_id_b;
        }

        /** @return 两个问题是否描述同一件事（用于去重）。 */
        bool same_conflict(const Conflict& left, const Conflict& right) {
            return left.type == right.type && left.course_id_a == right.course_id_a && left.course_id_b == right.course_id_b && left.session_id_a == right.session_id_a && left.session_id_b == right.session_id_b && left.day_of_week == right.day_of_week && left.week == right.week;
        }

    } // namespace

    QString ConflictDetector::describe_time(int day_of_week, int from_slot, int to_slot) {
        const QString day = WeekCalculator::day_name(day_of_week);
        if (from_slot == to_slot) {
            return QStringLiteral("%1第 %2 节").arg(day).arg(from_slot);
        }
        return QStringLiteral("%1第 %2-%3 节").arg(day).arg(from_slot).arg(to_slot);
    }

    int ConflictDetector::max_slot_index(const QList<TimeSlot>& time_slots) {
        int maximum = 0;
        for (const TimeSlot& slot : time_slots) {
            maximum = qMax(maximum, slot.index);
        }
        return maximum;
    }

    bool ConflictDetector::has_blocking_conflict(const QList<Conflict>& conflicts) {
        for (const Conflict& conflict : conflicts) {
            if (conflict.is_blocking()) {
                return true;
            }
        }
        return false;
    }

    bool ConflictDetector::sessions_conflict(const CourseSession& first,
        const CourseSession& second,
        const QString& first_name,
        const QString& second_name,
        Conflict* conflict) {
        int week = 0;
        if (!first.conflicts_with(second, &week)) {
            return false;
        }

        if (conflict) {
            Conflict result;
            result.type = Conflict::Type::TimeOverlap;
            result.session_id_a = first.id;
            result.session_id_b = second.id;
            result.day_of_week = first.day_of_week;
            result.week = week;
            result.message = QStringLiteral("%1 与 %2 在 %3（第 %4 周）时间冲突")
                                 .arg(first_name,
                                     second_name,
                                     describe_time(first.day_of_week, first.start_slot, first.end_slot()),
                                     QString::number(week));
            *conflict = result;
        }
        return true;
    }

    QList<Conflict> ConflictDetector::detect_in_course(const Course& course,
        const Semester& semester,
        const QList<TimeSlot>& time_slots,
        const Options& options) {
        QList<Conflict> conflicts;
        const int max_slot = max_slot_index(time_slots);

        if (course.name.trimmed().isEmpty()) {
            Conflict conflict;
            conflict.type = Conflict::Type::InvalidSession;
            conflict.course_id_a = course.id;
            conflict.message = QStringLiteral("课程名称为空（id: %1）").arg(course.id);
            conflicts.append(conflict);
            return conflicts;
        }

        if (course.sessions.isEmpty()) {
            Conflict conflict;
            conflict.type = Conflict::Type::MissingSession;
            conflict.course_id_a = course.id;
            conflict.message = QStringLiteral("课程“%1”没有任何上课时间").arg(course.name);
            conflicts.append(conflict);
        }

        for (const CourseSession& session : course.sessions) {
            QString error;
            if (!session.is_valid(options.check_slot_range ? max_slot : -1, &error)) {
                Conflict conflict;
                // 区分“节次越界”与其它结构性问题，便于界面用不同文案提示。
                conflict.type = (options.check_slot_range && max_slot > 0 && session.end_slot() > max_slot)
                                    ? Conflict::Type::OutOfRangeSlot
                                    : Conflict::Type::InvalidSession;
                conflict.course_id_a = course.id;
                conflict.session_id_a = session.id;
                conflict.day_of_week = session.day_of_week;
                conflict.message = QStringLiteral("课程“%1”：%2").arg(course.name, error);
                conflicts.append(conflict);
                continue;
            }

            if (options.check_week_range && semester.is_valid(nullptr)) {
                const int overflow = session.weeks.last_week() - semester.total_weeks;
                if (overflow > 0) {
                    Conflict conflict;
                    conflict.type = Conflict::Type::InvalidWeek;
                    conflict.course_id_a = course.id;
                    conflict.session_id_a = session.id;
                    conflict.day_of_week = session.day_of_week;
                    conflict.week = session.weeks.last_week();
                    conflict.message = QStringLiteral("课程“%1”的周次 %2 超出学期总周数 %3")
                                           .arg(course.name, session.weeks.to_expression())
                                           .arg(semester.total_weeks);
                    conflicts.append(conflict);
                }
            }
        }

        if (options.check_self_overlap) {
            for (int i = 0; i < course.sessions.size(); ++i) {
                for (int j = i + 1; j < course.sessions.size(); ++j) {
                    Conflict conflict;
                    if (sessions_conflict(course.sessions.at(i), course.sessions.at(j), course.name, course.name, &conflict)) {
                        conflict.type = Conflict::Type::SelfOverlap;
                        conflict.course_id_a = course.id;
                        conflict.course_id_b = course.id;
                        conflict.message = QStringLiteral("课程“%1”自身的两个上课时间段在 %2（第 %3 周）重叠")
                                               .arg(course.name,
                                                   describe_time(conflict.day_of_week,
                                                       course.sessions.at(i).start_slot,
                                                       course.sessions.at(i).end_slot()),
                                                   QString::number(conflict.week));
                        conflicts.append(conflict);
                    }
                }
            }
        }

        return conflicts;
    }

    QList<Conflict> ConflictDetector::detect_between(const Course& first,
        const Course& second,
        const Semester& semester,
        const Options& options) {
        Q_UNUSED(semester);
        QList<Conflict> conflicts;

        if (options.check_duplicate_courses && !first.id.isEmpty() && first.id != second.id) {
            const bool same_name = !first.name.isEmpty() && first.name == second.name;
            const bool same_code = !first.code.isEmpty() && first.code == second.code;
            if (same_name || same_code) {
                Conflict conflict;
                conflict.type = Conflict::Type::DuplicateCourse;
                conflict.course_id_a = first.id;
                conflict.course_id_b = second.id;
                conflict.message = same_code
                                       ? QStringLiteral("课程“%1”与“%2”使用了相同的课程代码 %3")
                                             .arg(first.name, second.name, first.code)
                                       : QStringLiteral("存在两门同名课程“%1”，请确认是否为重复录入").arg(first.name);
                conflicts.append(conflict);
            }
        }

        for (const CourseSession& session_a : first.sessions) {
            for (const CourseSession& session_b : second.sessions) {
                Conflict conflict;
                if (!sessions_conflict(session_a, session_b, first.name, second.name, &conflict)) {
                    continue;
                }
                conflict.course_id_a = first.id;
                conflict.course_id_b = second.id;
                conflicts.append(conflict);
            }
        }

        return conflicts;
    }

    QList<Conflict> ConflictDetector::detect(const QList<Course>& courses,
        const Semester& semester,
        const QList<TimeSlot>& time_slots,
        const Options& options) {
        QList<Conflict> conflicts;

        for (const Course& course : courses) {
            conflicts.append(detect_in_course(course, semester, time_slots, options));
        }

        for (int i = 0; i < courses.size(); ++i) {
            for (int j = i + 1; j < courses.size(); ++j) {
                conflicts.append(detect_between(courses.at(i), courses.at(j), semester, options));
            }
        }

        normalise(&conflicts);
        return conflicts;
    }

    void ConflictDetector::normalise(QList<Conflict>* conflicts) {
        if (!conflicts) {
            return;
        }
        std::stable_sort(conflicts->begin(), conflicts->end(), conflict_less_than);

        // 去重：同一对课程、同一时间段、同一天同一周的同一类问题只保留一条。
        // detect() 中“课程内部重叠”可能被 detect_in_course 与两两比较重复命中。
        QList<Conflict> unique;
        unique.reserve(conflicts->size());
        for (const Conflict& conflict : *conflicts) {
            bool duplicated = false;
            for (const Conflict& existing : unique) {
                if (same_conflict(existing, conflict)) {
                    duplicated = true;
                    break;
                }
            }
            if (!duplicated) {
                unique.append(conflict);
            }
        }
        *conflicts = unique;
    }

} // namespace Schedule
