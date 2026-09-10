#pragma once

#include <QList>
#include <QString>
#include <QTime>

namespace Schedule {

    /**
     * @brief 节次（作息时间表中的一节课）。
     *
     * 节次是**学期级**的作息定义：`index` 为 1 起的序号，`label` 是展示名（如“第 1 节”），
     * `start_time` / `end_time` 为该节的起止本地时间（不含时区）。
     *
     * `CourseSession` 只记录 `start_slot` / `slot_count`，通过 `index` 与本节次表关联，
     * 因此同一门课在不同学期可以使用不同的作息时间而不必修改课程数据。
     *
     * @note 本结构体只描述“第几节课在几点上下课”，不含星期信息——星期由
     *       `CourseSession::day_of_week` 承担。
     */
    struct TimeSlot {
        /** 节次序号，从 1 开始；用于与 `CourseSession::start_slot` 关联。 */
        int index = 0;

        /** 展示名，如“第 1 节”“早自习”；为空时上层可回退为 `第 N 节`。 */
        QString label;

        /** 本节开始时间（本地时间，无时区语义）。 */
        QTime start_time;

        /** 本节结束时间，必须晚于 start_time。 */
        QTime end_time;

        /** @return 序号、时间是否均合法（时间为空视为未设置，仅要求 index >= 1）。 */
        bool is_valid() const;

        /** @return 本节时长（分钟）；时间未设置或倒挂时返回 0。 */
        int duration_minutes() const;

        /** @return 展示名；label 为空时返回“第 N 节”。 */
        QString display_label() const;

        /** @return 默认作息表（上午 4 节 + 下午 4 节 + 晚上 3 节，共 11 节）。 */
        static QList<TimeSlot> default_slots();

        bool operator==(const TimeSlot& other) const;
        bool operator!=(const TimeSlot& other) const;
    };

} // namespace Schedule
