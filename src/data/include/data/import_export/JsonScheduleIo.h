#pragma once

#include "data/import_export/IScheduleExporter.h"
#include "data/import_export/IScheduleImporter.h"

namespace Schedule {

    /**
     * @brief 课表 JSON 格式的导入 / 导出实现。
     *
     * 直接复用 `ScheduleJson` 的映射，因此是三种格式中**唯一无损**的：
     * 保留课程颜色、学分、备注、时间段的地点 / 教师覆盖，以及精确的周次位图。
     *
     * 文件扩展名：`.json`
     */
    class JsonScheduleIo : public IScheduleImporter, public IScheduleExporter {
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
