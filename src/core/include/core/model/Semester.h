#pragma once

#include <QDate>
#include <QDateTime>
#include <QString>

namespace Schedule {

    /**
     * @brief 学期：课表的时间基准。
     *
     * 关键约定：周次按 `week_start_day`（默认周一）对齐，第 1 周是包含 `start_date`
     * 的那个周：第 N 周 = `week_start_day 对齐后的 start_date + 7 * (N - 1)` 起连续
     * 7 天。`start_date` 通常就是第 1 周的起始日，但不强制。
     * 所有周次计算都以本字段为锚点，因此“当前周”无需在数据库中维护，可随时由日期推导。
     */
    struct Semester {
        /** 学期唯一标识（UUID，无花括号）。 */
        QString id;

        /** 学期名称，如“2024-2025 学年第一学期”。 */
        QString name;

        /** 第 1 周的起始日期。 */
        QDate start_date;

        /** 学期总周数，通常为 16~20；合法范围 1..64（与 WeekMask 上限一致）。 */
        int total_weeks = 20;

        /**
         * @brief 每周起始日：1=周一，7=周日（与 QDate::dayOfWeek() 一致）。
         *
         * 第 1 周从包含 `start_date` 的那个“每周起始日”开始，之后每 7 天一周。
         * 默认周一，保持与旧数据、导入格式兼容。
         */
        int week_start_day = 1;

        /** 是否为当前使用的学期；同一时刻应只有一个学期为 true，由仓库层保证。 */
        bool is_current = false;

        /** 创建时间（本地时间）。 */
        QDateTime created_at;

        /** 最近一次修改时间（本地时间）。 */
        QDateTime updated_at;

        /**
         * @brief 学期结构自检。
         * @param error_message 可选输出，失败时写入中文原因。
         */
        bool is_valid(QString* error_message = nullptr) const;

        /** @return 第 week 周的起始日期；week 越界时返回无效 QDate。 */
        QDate week_start_date(int week) const;

        /** @return 第 week 周的结束日期（起始日 + 6 天）。 */
        QDate week_end_date(int week) const;

        /** @return 学期最后一天的日期。 */
        QDate end_date() const;

        /**
         * @brief 计算某日期落在第几周。
         * @return 1..total_weeks；不在学期范围内返回 0。
         */
        int week_of(const QDate& date) const;

        /** @return 日期是否落在学期范围内。 */
        bool contains(const QDate& date) const;

        /** @return 距今（不含时区）是否仍在进行中。 */
        bool is_active_on(const QDate& today) const;

        /**
         * @brief 创建一个新学期。
         *
         * 自动生成 UUID、填充创建 / 修改时间；`total_weeks` 会被夹取到 1..64。
         */
        static Semester create(const QString& name, const QDate& start_date, int total_weeks,
            int week_start_day = 1);

        bool operator==(const Semester& other) const;
        bool operator!=(const Semester& other) const;
    };

} // namespace Schedule
