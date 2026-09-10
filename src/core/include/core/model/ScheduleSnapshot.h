#pragma once

#include "core/model/Conflict.h"
#include "core/model/Course.h"
#include "core/model/Semester.h"
#include "core/model/TimeSlot.h"

#include <QList>

namespace Schedule {

    /**
     * @brief 一个学期的完整课表数据快照。
     *
     * 这是 `data` 层仓库、导入导出器与 `engine` 层之间传递数据的**唯一聚合载体**：
     * 一次读写即可获得“学期 + 作息表 + 全部课程”，避免上层拼装多次调用。
     *
     * 设计为纯值类型，可自由拷贝、比较与序列化。
     */
    struct ScheduleSnapshot {
        /** 学期元数据（含名称、起始日期、总周数）。 */
        Semester semester;

        /** 该学期的作息表；为空时可按 `TimeSlot::default_slots()` 回退。 */
        QList<TimeSlot> time_slots;

        /** 该学期的全部课程（含各自的上课时间段）。 */
        QList<Course> courses;

        /** @return 快照是否可用（学期合法）。 */
        bool is_valid(QString* error_message = nullptr) const;

        /** @return 作息表的最大节次序号；为空时返回 0。 */
        int max_slot_index() const;

        /** @return 按 id 查找课程；未找到返回 false。 */
        bool find_course(const QString& course_id, Course* out_course) const;

        /** @return 按 id 查找课程下标；未找到返回 -1。 */
        int index_of_course(const QString& course_id) const;

        /** @return 该快照的全部冲突（委托 ConflictDetector）。 */
        QList<Conflict> detect_conflicts() const;

        /** 清空内容（保留一个默认构造的学期）。 */
        void clear();
    };

} // namespace Schedule
