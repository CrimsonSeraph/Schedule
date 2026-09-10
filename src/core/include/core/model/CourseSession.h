#pragma once

#include "core/model/WeekMask.h"

#include <QString>

namespace Schedule {

    /**
     * @brief 上课时间段：一门课在“周几、第几节起、连续几节、哪些周”的一次安排。
     *
     * 一门 `Course` 可以拥有多个 `CourseSession`（例如理论课周一 1-2 节、实验课周三 5-6 节）。
     * 冲突检测、周视图渲染都以本结构为最小单位。
     *
     * `location` / `teacher` 用于**覆盖**课程级默认值：留空表示沿用 `Course` 上的值，
     * 这样同一门课的不同时间段可以安排在不同教室或由不同教师授课。
     */
    struct CourseSession {
        /** 时间段唯一标识（UUID，无花括号）；新建时若为空由上层补齐。 */
        QString id;

        /** 星期：1=周一 ... 7=周日。 */
        int day_of_week = 1;

        /** 起始节次序号，对应 `TimeSlot::index`，从 1 开始。 */
        int start_slot = 1;

        /** 连续节次数，至少为 1。 */
        int slot_count = 1;

        /** 该时间段在第几周出现。 */
        WeekMask weeks;

        /** 上课地点；为空表示沿用课程级地点。 */
        QString location;

        /** 授课教师；为空表示沿用课程级教师。 */
        QString teacher;

        /** @return 结束节次序号（含）。 */
        int end_slot() const;

        /**
         * @brief 结构自检。
         * @param max_slot 可选作息表最大节次；> 0 时校验是否越界。
         * @param error_message 可选输出，失败时写入中文原因。
         * @return 是否合法。
         */
        bool is_valid(int max_slot = -1, QString* error_message = nullptr) const;

        /**
         * @brief 判断与另一时间段是否**在时间上**冲突。
         *
         * 冲突条件（三个条件同时满足）：
         *  1. 星期相同；
         *  2. 节次区间相交；
         *  3. 周次集合有交集。
         *
         * @param other 另一时间段
         * @param conflict_week 可选输出：第一个冲突周次（便于向用户定位问题）
         * @return 是否冲突
         */
        bool conflicts_with(const CourseSession& other, int* conflict_week = nullptr) const;

        /** @return 有效的展示地点（本时间段优先，回退到 fallback）。 */
        QString effective_location(const QString& fallback) const;

        /** @return 有效的展示教师（本时间段优先，回退到 fallback）。 */
        QString effective_teacher(const QString& fallback) const;

        bool operator==(const CourseSession& other) const;
        bool operator!=(const CourseSession& other) const;
    };

} // namespace Schedule
