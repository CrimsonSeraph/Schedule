#include "core/service/ReminderScheduler.h"

#include "core/service/WeekCalculator.h"

#include <algorithm>

namespace Schedule {

    namespace {

        /** @return 归一化后的提前提醒分钟数（非法值回退默认值）。 */
        int normalise_minutes(int minutes_before) {
            return ReminderScheduler::is_supported_minutes(minutes_before) ? minutes_before
                                                                           : ReminderScheduler::DEFAULT_MINUTES_BEFORE;
        }

        /** @return 从快照的作息表中查找指定序号的节次；未找到返回默认构造值。 */
        TimeSlot find_slot(const ScheduleSnapshot& snapshot, int index) {
            for (const TimeSlot& slot : snapshot.time_slots) {
                if (slot.index == index) {
                    return slot;
                }
            }
            return TimeSlot();
        }

        /** @return 两个提醒是否按时间先后排序。 */
        bool reminder_less_than(const Reminder& left, const Reminder& right) {
            if (left.remind_at != right.remind_at) {
                return left.remind_at < right.remind_at;
            }
            if (left.course_name != right.course_name) {
                return left.course_name < right.course_name;
            }
            return left.session_id < right.session_id;
        }

        /** @brief 构造一条提醒；任一时间不可计算时返回 false。 */
        bool build_reminder(const ScheduleSnapshot& snapshot,
            const Course& course,
            const CourseSession& session,
            int week,
            int minutes_before,
            Reminder* out) {
            const TimeSlot start_slot = find_slot(snapshot, session.start_slot);
            const QDateTime start = WeekCalculator::session_start_datetime(snapshot.semester, start_slot, week, session.day_of_week);
            if (!start.isValid()) {
                return false;
            }

            Reminder reminder;
            reminder.course_id = course.id;
            reminder.course_name = course.name;
            reminder.session_id = session.id;
            reminder.location = session.effective_location(course.location);
            reminder.teacher = session.effective_teacher(course.teacher);
            reminder.week = week;
            reminder.day_of_week = session.day_of_week;
            reminder.start = start;
            reminder.remind_at = start.addSecs(-60 * minutes_before);
            reminder.minutes_before = minutes_before;

            *out = reminder;
            return true;
        }

    } // namespace

    bool Reminder::is_valid() const {
        return !course_name.isEmpty() && start.isValid() && remind_at.isValid();
    }

    QString Reminder::title() const {
        return QStringLiteral("%1 分钟后上课：%2").arg(minutes_before).arg(course_name);
    }

    QString Reminder::message() const {
        QStringList parts;
        parts.append(start.toString(QStringLiteral("HH:mm")));
        if (!location.isEmpty()) {
            parts.append(location);
        }
        if (!teacher.isEmpty()) {
            parts.append(teacher);
        }
        parts.append(QStringLiteral("第 %1 周 %2").arg(week).arg(WeekCalculator::day_name(day_of_week)));
        return parts.join(QStringLiteral(" · "));
    }

    QString Reminder::unique_key() const {
        return QStringLiteral("%1|%2|%3|%4")
            .arg(course_id, session_id)
            .arg(week)
            .arg(start.toString(Qt::ISODate));
    }

    QList<int> ReminderScheduler::supported_minutes() {
        return QList<int>{5, 10, 15};
    }

    bool ReminderScheduler::is_supported_minutes(int minutes) {
        return supported_minutes().contains(minutes);
    }

    QList<Reminder> ReminderScheduler::upcoming(const ScheduleSnapshot& snapshot,
        const QDateTime& now,
        int minutes_before,
        int horizon_hours) {
        QList<Reminder> reminders;
        if (!snapshot.semester.is_valid(nullptr) || !now.isValid()) {
            return reminders;
        }

        const int minutes = normalise_minutes(minutes_before);
        const QDateTime until = now.addSecs(3600LL * qMax(1, horizon_hours));
        const int days = qMax(1, horizon_hours / 24 + 2);

        // 逐日扫描：一学期最多 64 周，但实际只需要覆盖 now 起的若干天，避免整学期展开。
        for (int offset = 0; offset < days; ++offset) {
            const QDate date = now.date().addDays(offset);
            const int week = snapshot.semester.week_of(date);
            if (week <= 0) {
                continue;
            }
            const int day_of_week = static_cast<int>(snapshot.semester.week_start_date(week).daysTo(date)) + 1;

            for (const Course& course : snapshot.courses) {
                for (const CourseSession& session : course.sessions) {
                    if (session.day_of_week != day_of_week || !session.weeks.contains(week)) {
                        continue;
                    }
                    Reminder reminder;
                    if (!build_reminder(snapshot, course, session, week, minutes, &reminder)) {
                        continue;
                    }
                    if (reminder.remind_at < now || reminder.remind_at > until) {
                        continue;
                    }
                    reminders.append(reminder);
                }
            }
        }

        std::stable_sort(reminders.begin(), reminders.end(), reminder_less_than);
        return reminders;
    }

    QList<Reminder> ReminderScheduler::for_date(const ScheduleSnapshot& snapshot, const QDate& date, int minutes_before) {
        QList<Reminder> reminders;
        if (!snapshot.semester.is_valid(nullptr) || !date.isValid()) {
            return reminders;
        }

        const int week = snapshot.semester.week_of(date);
        if (week <= 0) {
            return reminders;
        }
        const int minutes = normalise_minutes(minutes_before);
        const int day_of_week = static_cast<int>(snapshot.semester.week_start_date(week).daysTo(date)) + 1;

        for (const Course& course : snapshot.courses) {
            for (const CourseSession& session : course.sessions) {
                if (session.day_of_week != day_of_week || !session.weeks.contains(week)) {
                    continue;
                }
                Reminder reminder;
                if (build_reminder(snapshot, course, session, week, minutes, &reminder)) {
                    reminders.append(reminder);
                }
            }
        }

        std::stable_sort(reminders.begin(), reminders.end(), reminder_less_than);
        return reminders;
    }

    bool ReminderScheduler::next(const ScheduleSnapshot& snapshot, const QDateTime& now, int minutes_before, Reminder* out) {
        // 向前看一整周即可覆盖“下一次上课”；学期内任意两节课间隔不会超过 7 天。
        const QList<Reminder> reminders = upcoming(snapshot, now, minutes_before, 24 * 7);
        if (reminders.isEmpty()) {
            return false;
        }
        if (out) {
            *out = reminders.first();
        }
        return true;
    }

    bool ReminderScheduler::is_due(const Reminder& reminder, const QDateTime& now) {
        if (!reminder.is_valid() || !now.isValid()) {
            return false;
        }
        return reminder.remind_at <= now && now < reminder.start;
    }

} // namespace Schedule
