#pragma once

#include "core/model/ScheduleSnapshot.h"

#include <QDate>
#include <QDateTime>
#include <QList>
#include <QString>

namespace Schedule {

    /**
     * @brief 一条待触发的课程提醒。
     *
     * 由 `ReminderScheduler` 从课表快照中**纯计算**得出：不涉及定时器、不涉及通知 API，
     * 因此可以在无 GUI 环境下单元测试。
     */
    struct Reminder {
        /** 所属课程 id。 */
        QString course_id;

        /** 课程名称（通知标题用）。 */
        QString course_name;

        /** 时间段 id（与 course_id、周次共同构成去重键）。 */
        QString session_id;

        /** 上课地点；可为空。 */
        QString location;

        /** 授课教师；可为空。 */
        QString teacher;

        /** 周次。 */
        int week = 0;

        /** 星期（1..7）。 */
        int day_of_week = 0;

        /** 上课开始时刻（本地时间）。 */
        QDateTime start;

        /** 应当提醒的时刻（= start - minutes_before 分钟）。 */
        QDateTime remind_at;

        /** 提前提醒分钟数。 */
        int minutes_before = 0;

        /** @return 提醒是否可用（课程名与两个时间都有效）。 */
        bool is_valid() const;

        /** @return 通知标题，如“10 分钟后上课：高等数学”。 */
        QString title() const;

        /** @return 通知正文，如“08:00 · 教一 101 · 张老师”。 */
        QString message() const;

        /**
         * @return 去重键（课程 + 时间段 + 周次 + 日期）。
         *
         * 用于避免同一条提醒在多个轮询周期内被重复弹出。
         */
        QString unique_key() const;
    };

    /**
     * @brief 提醒计算器：把课表换算成“什么时候该提醒哪门课”。
     *
     * **统一口径**：所有时间都是本地时间、无时区语义；提前提醒分钟数仅支持 5 / 10 / 15。
     *
     * 典型用法（由 `engine::NotificationService` 周期性调用）：
     *
     * ```cpp
     * const QList<Reminder> due = ReminderScheduler::upcoming(snapshot, QDateTime::currentDateTime(), 10, 24);
     * ```
     */
    class ReminderScheduler {
    public:
        /** 默认提前提醒分钟数。 */
        static constexpr int DEFAULT_MINUTES_BEFORE = 10;

        /** 支持的提前提醒分钟数（5 / 10 / 15）。 */
        static QList<int> supported_minutes();

        /** @return 是否为受支持的提前提醒分钟数。 */
        static bool is_supported_minutes(int minutes);

        /**
         * @brief 计算未来一段时间内**应当提醒**的条目。
         *
         * @param snapshot       课表快照（含学期与作息表）
         * @param now            当前时刻
         * @param minutes_before 提前提醒分钟数（非法值回退为默认值）
         * @param horizon_hours  向前看的小时数（默认 24 小时）
         * @return 按 `remind_at` 升序排列的提醒列表
         *
         * @note 只返回 `remind_at` 落在 `[now, now + horizon_hours]` 区间内的条目；
         *       已经过去的提醒不会被补发（避免打开应用时被历史通知淹没）。
         */
        static QList<Reminder> upcoming(const ScheduleSnapshot& snapshot,
            const QDateTime& now,
            int minutes_before,
            int horizon_hours = 24);

        /**
         * @brief 计算某一天的全部课程提醒（不按当前时刻过滤）。
         * @return 按上课时刻升序排列的提醒列表，用于“今日课程”列表展示
         */
        static QList<Reminder> for_date(const ScheduleSnapshot& snapshot, const QDate& date, int minutes_before);

        /**
         * @brief 取 `now` 之后最近的一条提醒。
         * @param out 输出参数
         * @return 是否找到
         */
        static bool next(const ScheduleSnapshot& snapshot, const QDateTime& now, int minutes_before, Reminder* out);

        /**
         * @brief 判断一条提醒当前是否已经到点。
         *
         * 到点条件：`remind_at <= now < start`（上课开始后不再提醒）。
         */
        static bool is_due(const Reminder& reminder, const QDateTime& now);
    };

} // namespace Schedule
