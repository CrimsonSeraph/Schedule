#pragma once

#include "core/model/CourseSession.h"
#include "core/model/WeekMask.h"

#include <QList>
#include <QString>

namespace Schedule {

    /**
     * @brief 课程：某学期中的一门课及其全部上课时间段。
     *
     * `teacher` / `location` 为**课程级默认值**，可被 `CourseSession` 上的同名字段覆盖。
     * `color` 为周视图中课卡的颜色，采用 `#RRGGBB` 形式；为空时由 `default_color_for()`
     * 按课程名稳定地派生一个颜色，保证同一门课在多次渲染中颜色一致。
     */
    struct Course {
        /** 课程唯一标识（UUID，无花括号）。 */
        QString id;

        /** 所属学期 id，对应 `Semester::id`。 */
        QString semester_id;

        /** 课程名称，如“高等数学 A”。 */
        QString name;

        /** 课程代码 / 教学班号，可为空。 */
        QString code;

        /** 默认授课教师；可被时间段覆盖。 */
        QString teacher;

        /** 默认上课地点；可被时间段覆盖。 */
        QString location;

        /** 课卡颜色 `#RRGGBB`；为空时使用派生颜色。 */
        QString color;

        /** 学分；0 表示未填写。 */
        double credits = 0.0;

        /** 备注。 */
        QString notes;

        /** 全部上课时间段；至少应有一个才会出现在课表上。 */
        QList<CourseSession> sessions;

        /** @return 是否至少有一个非空的上课时间段。 */
        bool has_sessions() const;

        /** @return 所有时间段周次的并集。 */
        WeekMask total_weeks() const;

        /** @return 该课程是否在指定的“星期 + 周次”出现。 */
        bool occurs_on(int day_of_week, int week) const;

        /** @return 出现在指定星期的全部时间段。 */
        QList<CourseSession> sessions_on_day(int day_of_week) const;

        /**
         * @brief 课程级自检（不涉及其它课程）。
         * @param max_slot 作息表最大节次；> 0 时校验时间段是否越界。
         * @param error_message 可选输出，失败时写入中文原因。
         */
        bool is_valid(int max_slot = -1, QString* error_message = nullptr) const;

        /** 为缺失 id 的时间段补齐 UUID（导入旧数据后调用）。 */
        void ensure_session_ids();

        /** @return 展示颜色：`color` 有效时原样返回，否则按课程名派生。 */
        QString display_color() const;

        /**
         * @brief 按课程标识稳定派生一个颜色。
         *
         * 使用简单散列而非随机数，保证同一课程名在任意次渲染中颜色一致
         * （避免课卡颜色在列表刷新后跳变）。
         */
        static QString default_color_for(const QString& seed);

        bool operator==(const Course& other) const;
        bool operator!=(const Course& other) const;
    };

} // namespace Schedule
