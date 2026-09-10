#pragma once

#include <QMetaType>
#include <QString>

namespace Schedule {

    /**
     * @brief 冲突 / 校验问题：描述课表中一处需要用户关注的问题。
     *
     * 本结构既承载**两门课之间的时间冲突**，也承载**单门课的非法数据**（如周次越界、
     * 节次超出作息表）。统一的目的是让上层只需处理一种“问题”列表，
     * 用 `is_blocking()` 区分“必须修正”与“仅提示”。
     */
    struct Conflict {
        /** 冲突类型。 */
        enum class Type {
            /** 两门不同课程在同一星期的同一节次、且有公共周次。 */
            TimeOverlap,
            /** 同一课程内部的多个时间段互相重叠。 */
            SelfOverlap,
            /** 学期内存在名称或课程代码重复的课程。 */
            DuplicateCourse,
            /** 周次为空，或周次超出学期总周数。 */
            InvalidWeek,
            /** 节次超出作息表范围。 */
            OutOfRangeSlot,
            /** 课程没有任何有效的上课时间段。 */
            MissingSession,
            /** 星期 / 起始节次 / 连续节次数取值非法。 */
            InvalidSession,
        };

        /** 冲突类型。 */
        Type type = Type::TimeOverlap;

        /** 冲突涉及的第一个课程 id（单课程问题时即该课程）。 */
        QString course_id_a;

        /** 冲突涉及的第二个课程 id；单课程问题时为空。 */
        QString course_id_b;

        /** 冲突涉及的第一个时间段 id；无则为空。 */
        QString session_id_a;

        /** 冲突涉及的第二个时间段 id；无则为空。 */
        QString session_id_b;

        /** 相关星期（1..7）；不适用时为 0。 */
        int day_of_week = 0;

        /** 相关周次；不适用时为 0。 */
        int week = 0;

        /** 面向用户的中文描述（已包含课程名等上下文）。 */
        QString message;

        /** @return 类型的中文名称，用于分组展示。 */
        QString type_name() const;

        /**
         * @return 是否为阻断性问题。
         *
         * 时间重叠、非法周次等属于阻断性问题（会导致课表无法正确展示）；
         * 重复课程仅作提示（学生确实可能重修同名课程）。
         */
        bool is_blocking() const;

        /** @return 单行展示文本，如“[时间冲突] 高等数学 与 线性代数 在周二第 1-2 节（第 3 周）冲突”。 */
        QString to_display_string() const;
    };

} // namespace Schedule

/** 允许 Conflict 存入 QVariant（导入预览、QML 转发等场景）。 */
Q_DECLARE_METATYPE(Schedule::Conflict)
