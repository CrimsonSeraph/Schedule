#include "core/model/TimeSlot.h"

namespace Schedule {

    namespace {

        /**
         * @brief 构造一条节次记录。
         * @param index 节次序号（1 起）
         * @param start "HH:mm" 起始时间
         * @param end   "HH:mm" 结束时间
         */
        TimeSlot make_slot(int index, const char* start, const char* end) {
            TimeSlot slot;
            slot.index = index;
            slot.label = QStringLiteral("第 %1 节").arg(index);
            slot.start_time = QTime::fromString(QString::fromLatin1(start), QStringLiteral("HH:mm"));
            slot.end_time = QTime::fromString(QString::fromLatin1(end), QStringLiteral("HH:mm"));
            return slot;
        }

    } // namespace

    bool TimeSlot::is_valid() const {
        if (index < 1) {
            return false;
        }
        // 允许“只定义序号、未定义作息时间”的宽松场景（例如从其它学校导入的课表），
        // 但只要填写了时间，就必须成对且不倒挂。
        if (start_time.isValid() != end_time.isValid()) {
            return false;
        }
        if (start_time.isValid() && end_time <= start_time) {
            return false;
        }
        return true;
    }

    int TimeSlot::duration_minutes() const {
        if (!start_time.isValid() || !end_time.isValid() || end_time <= start_time) {
            return 0;
        }
        return static_cast<int>(start_time.secsTo(end_time) / 60);
    }

    QString TimeSlot::display_label() const {
        if (!label.isEmpty()) {
            return label;
        }
        return QStringLiteral("第 %1 节").arg(index);
    }

    QList<TimeSlot> TimeSlot::default_slots() {
        return QList<TimeSlot>{
            make_slot(1, "08:00", "08:45"),
            make_slot(2, "08:55", "09:40"),
            make_slot(3, "10:00", "10:45"),
            make_slot(4, "10:55", "11:40"),
            make_slot(5, "14:00", "14:45"),
            make_slot(6, "14:55", "15:40"),
            make_slot(7, "16:00", "16:45"),
            make_slot(8, "16:55", "17:40"),
            make_slot(9, "19:00", "19:45"),
            make_slot(10, "19:55", "20:40"),
            make_slot(11, "20:50", "21:35"),
        };
    }

    bool TimeSlot::operator==(const TimeSlot& other) const {
        return index == other.index && label == other.label && start_time == other.start_time && end_time == other.end_time;
    }

    bool TimeSlot::operator!=(const TimeSlot& other) const {
        return !(*this == other);
    }

} // namespace Schedule
