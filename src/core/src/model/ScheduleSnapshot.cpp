#include "core/model/ScheduleSnapshot.h"

#include "core/service/ConflictDetector.h"

namespace Schedule {

    bool ScheduleSnapshot::is_valid(QString* error_message) const {
        if (!semester.is_valid(error_message)) {
            return false;
        }
        for (const Course& course : courses) {
            if (!course.is_valid(-1, error_message)) {
                return false;
            }
        }
        if (error_message) {
            error_message->clear();
        }
        return true;
    }

    int ScheduleSnapshot::max_slot_index() const {
        int maximum = 0;
        for (const TimeSlot& slot : time_slots) {
            maximum = qMax(maximum, slot.index);
        }
        return maximum;
    }

    int ScheduleSnapshot::index_of_course(const QString& course_id) const {
        for (int i = 0; i < courses.size(); ++i) {
            if (courses.at(i).id == course_id) {
                return i;
            }
        }
        return -1;
    }

    bool ScheduleSnapshot::find_course(const QString& course_id, Course* out_course) const {
        const int index = index_of_course(course_id);
        if (index < 0) {
            return false;
        }
        if (out_course) {
            *out_course = courses.at(index);
        }
        return true;
    }

    QList<Conflict> ScheduleSnapshot::detect_conflicts() const {
        return ConflictDetector::detect(courses, semester, time_slots);
    }

    void ScheduleSnapshot::clear() {
        semester = Semester();
        time_slots.clear();
        courses.clear();
    }

} // namespace Schedule
