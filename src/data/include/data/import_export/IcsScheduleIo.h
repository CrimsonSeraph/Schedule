#pragma once

#include "data/import_export/IScheduleExporter.h"
#include "data/import_export/IScheduleImporter.h"

namespace Schedule {

    /**
     * @brief 课表 iCalendar（`.ics`）格式的导入 / 导出实现。
     *
     * **导出映射**：一门课的每个上课时间段对应一个 `VEVENT`。
     *
     * | 课表字段 | ICS 属性 |
     * | -------- | -------- |
     * | 上课开始时刻 | `DTSTART`（本地浮动时间，不带时区） |
     * | 上课结束时刻 | `DTEND` |
     * | 重复周次 | `RRULE`（`FREQ=WEEKLY` / 含 `INTERVAL`）或 `RDATE` 列表 |
     * | 课程名称 | `SUMMARY` |
     * | 地点 | `LOCATION` |
     * | 教师 / 备注 | `DESCRIPTION` |
     * | 时间段的精确节次与周次 | 扩展属性 `X-SCHEDULE-*`（保证往返无损） |
     *
     * **导入映射**：优先使用 `X-SCHEDULE-*` 扩展属性；若不存在（例如来自其它日历应用），
     * 则按 `DTSTART` 的星期与时间推导星期与节次，并按 `RRULE` / `RDATE` 推导周次，
     * 同时用出现过的上课时间合成作息表。
     *
     * 其它约定：
     *  - 输出严格使用 `CRLF` 换行，并按 RFC 5545 的 75 字节规则折行；
     *  - 文本转义遵循 RFC 5545（`\\`、`\;`、`\,`、`\n`）；
     *  - 全天事件（`VALUE=DATE`）会被跳过，因为它们不表示具体上课时段。
     */
    class IcsScheduleIo : public IScheduleImporter, public IScheduleExporter {
    public:
        // ------------------------------------------------------- IScheduleImporter

        ScheduleFormat format() const override;
        QString display_name() const override;
        QStringList extensions() const override;
        bool can_import(const QString& file_path, QString* error_message = nullptr) const override;
        bool parse(const QString& file_path, ScheduleSnapshot* out_snapshot, QString* error_message = nullptr) const override;
        bool parse_data(const QByteArray& data,
            const QString& source_name,
            ScheduleSnapshot* out_snapshot,
            QString* error_message = nullptr) const override;

        // ------------------------------------------------------- IScheduleExporter

        QString extension() const override;
        QByteArray serialize(const ScheduleSnapshot& snapshot, QString* error_message = nullptr) const override;
        bool write(const QString& file_path, const ScheduleSnapshot& snapshot, QString* error_message = nullptr) const override;
    };

} // namespace Schedule
