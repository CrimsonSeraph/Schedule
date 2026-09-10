#pragma once

#include "core/model/Semester.h"
#include "core/model/TimeSlot.h"

#include <QDate>
#include <QDateTime>
#include <QString>
#include <QStringList>

namespace Schedule {

    /**
     * @brief 周次与日期换算工具。
     *
     * 课表里“第几周”“周几”“第几节”三者需要与真实日历互相换算，本类集中提供这些
     * 纯函数，避免各层各自实现导致口径不一致（例如一处把周一当 0、另一处当 1）。
     *
     * **统一口径**：
     *  - 星期取值 1..7，1 = 周一，7 = 周日（与 `QDate::dayOfWeek()` 的 Qt 取值一致）；
     *  - 周次取值 1..`Semester::total_weeks`，0 表示“不在学期内”；
     *  - 第 N 周从 `Semester::start_date + 7*(N-1)` 起算，`start_date` 不强制为周一。
     */
    class WeekCalculator {
    public:
        /** 一周的天数。 */
        static constexpr int DAYS_PER_WEEK = 7;

        /** 星期名称（中文，索引 0 未使用）。 */
        static const QStringList& day_names();

        /**
         * @brief 计算日期所属周次。
         * @return 1..total_weeks；不在学期内返回 0。
         */
        static int week_of(const Semester& semester, const QDate& date);

        /**
         * @brief 计算“当前周”（相对于给定日期，默认今天）。
         * @return 1..total_weeks；学期尚未开始或已结束返回 0。
         */
        static int current_week(const Semester& semester, const QDate& today = QDate::currentDate());

        /** @return 第 week 周的起始日期；越界返回无效 QDate。 */
        static QDate week_start_date(const Semester& semester, int week);

        /** @return 第 week 周的结束日期；越界返回无效 QDate。 */
        static QDate week_end_date(const Semester& semester, int week);

        /**
         * @brief 计算“第 week 周、星期 day_of_week”对应的日期。
         * @param day_of_week 1=周一 ... 7=周日
         * @return 日期；参数越界返回无效 QDate。
         */
        static QDate date_of(const Semester& semester, int week, int day_of_week);

        /** @return 某日期所在自然周的周一（用于把任意日期对齐到周视图行首）。 */
        static QDate monday_of(const QDate& date);

        /** @return 中文星期名（如“周二”）；越界返回空串。 */
        static QString day_name(int day_of_week);

        /** @return 简短中文星期名（如“二”）；越界返回空串。 */
        static QString short_day_name(int day_of_week);

        /**
         * @brief 解析 `HH:mm` / `H:mm` / `HH:mm:ss` 形式的时间文本。
         *
         * 导入 CSV / ICS 时时间格式不完全统一，这里统一做一次宽松解析。
         * @return 解析失败返回无效 QTime。
         */
        static QTime parse_time(const QString& text);

        /** @return 格式化为 `HH:mm`；无效时间返回空串。 */
        static QString format_time(const QTime& time);

        /**
         * @brief 计算某次课的开始时刻。
         *
         * @param semester    学期（提供起始日期）
         * @param slot        节次（提供开始时间）
         * @param week        周次
         * @param day_of_week 星期（1..7）
         * @return 本地日期时间；任一参数越界或节次无开始时间时返回无效 QDateTime。
         */
        static QDateTime session_start_datetime(const Semester& semester, const TimeSlot& slot, int week, int day_of_week);

        /**
         * @brief 解析形如 `1`、`一`、`周一`、`星期一`、`Monday`、`Mon` 的星期文本。
         * @return 1..7；无法识别返回 0。
         */
        static int parse_day_of_week(const QString& text);
    };

} // namespace Schedule
