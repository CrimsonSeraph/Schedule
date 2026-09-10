#pragma once

#include "data/import_export/IScheduleExporter.h"
#include "data/import_export/IScheduleImporter.h"

namespace Schedule {

    /**
     * @brief 课表 CSV 格式的导入 / 导出实现。
     *
     * **表结构**：一行代表一个“上课时间段”（同门课的多个时间段会有多行），
     * 首行为表头。列名支持中文与英文别名，顺序无关，多余列被忽略：
     *
     * | 列 | 必需 | 别名 |
     * | -- | ---- | ---- |
     * | 课程名称 | 是 | 课程 / 名称 / name / course |
     * | 课程代码 | 否 | 代码 / code |
     * | 教师 | 否 | 老师 / 授课教师 / teacher |
     * | 地点 | 否 | 教室 / 上课地点 / location |
     * | 星期 | 是 | 周几 / day / weekday（支持“周一”“一”“1”“Monday”“MO”） |
     * | 开始节次 | 是 | 起始节次 / 节次 / start_slot |
     * | 节次数量 | 否 | 连续节次 / slot_count（默认 1） |
     * | 周次 | 是 | 上课周次 / weeks（表达式，如 `1-16`、`1-16/2`） |
     * | 学分 | 否 | credits |
     * | 备注 | 否 | notes |
     * | 开始时间 | 否 | start（`HH:mm`，用于补充作息表） |
     * | 结束时间 | 否 | end（`HH:mm`） |
     *
     * **编码**：导出为 UTF-8 with BOM（Excel 双击可正确识别中文）；
     * 导入时自动跳过 BOM，非 UTF-8（例如 GBK）会给出明确提示，
     * 因为 Qt 6 默认不再内置 GBK 编解码器。
     *
     * 文件扩展名：`.csv`（也接受 `.txt`）。
     */
    class CsvScheduleIo : public IScheduleImporter, public IScheduleExporter {
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
