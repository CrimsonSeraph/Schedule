#include "core/model/CourseSession.h"

namespace Schedule {

    int CourseSession::end_slot() const {
        return start_slot + qMax(1, slot_count) - 1;
    }

    bool CourseSession::is_valid(int max_slot, QString* error_message) const {
        const auto fail = [error_message](const QString& message) {
            if (error_message) {
                *error_message = message;
            }
            return false;
        };

        if (day_of_week < 1 || day_of_week > 7) {
            return fail(QStringLiteral("星期必须在 1（周一）到 7（周日）之间，当前为 %1").arg(day_of_week));
        }
        if (start_slot < 1) {
            return fail(QStringLiteral("起始节次必须大于 0，当前为 %1").arg(start_slot));
        }
        if (slot_count < 1) {
            return fail(QStringLiteral("连续节次数必须大于 0，当前为 %1").arg(slot_count));
        }
        if (weeks.is_empty()) {
            return fail(QStringLiteral("上课周次不能为空"));
        }
        if (max_slot > 0 && end_slot() > max_slot) {
            return fail(QStringLiteral("节次超出作息表范围：结束节次 %1，作息表共 %2 节").arg(end_slot()).arg(max_slot));
        }
        if (error_message) {
            error_message->clear();
        }
        return true;
    }

    bool CourseSession::conflicts_with(const CourseSession& other, int* conflict_week) const {
        if (day_of_week != other.day_of_week) {
            return false;
        }
        // 节次区间相交：闭区间重叠判定
        if (start_slot > other.end_slot() || other.start_slot > end_slot()) {
            return false;
        }
        const WeekMask common = weeks.intersected(other.weeks);
        if (common.is_empty()) {
            return false;
        }
        if (conflict_week) {
            *conflict_week = common.first_week();
        }
        return true;
    }

    QString CourseSession::effective_location(const QString& fallback) const {
        return location.isEmpty() ? fallback : location;
    }

    QString CourseSession::effective_teacher(const QString& fallback) const {
        return teacher.isEmpty() ? fallback : teacher;
    }

    bool CourseSession::operator==(const CourseSession& other) const {
        return id == other.id && day_of_week == other.day_of_week && start_slot == other.start_slot && slot_count == other.slot_count && weeks == other.weeks && location == other.location && teacher == other.teacher;
    }

    bool CourseSession::operator!=(const CourseSession& other) const {
        return !(*this == other);
    }

} // namespace Schedule
