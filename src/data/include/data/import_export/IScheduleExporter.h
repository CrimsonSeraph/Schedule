#pragma once

#include "core/model/ScheduleSnapshot.h"
#include "data/import_export/ImportExportTypes.h"

#include <QByteArray>
#include <QString>

namespace Schedule {

    /**
     * @brief 课表导出器接口。
     *
     * 实现者负责把 `ScheduleSnapshot` 序列化并写入**给定路径**；
     * “写到哪个目录、叫什么文件名”由 `ExportManager` 决定。
     *
     * 所有实现必须使用 `QSaveFile` 之类的原子写入手段，避免中途失败留下半个文件。
     */
    class IScheduleExporter {
    public:
        virtual ~IScheduleExporter() = default;

        /** @return 本导出器生成的格式。 */
        virtual ScheduleFormat format() const = 0;

        /** @return 展示名（中文）。 */
        virtual QString display_name() const = 0;

        /** @return 推荐扩展名（不含点），如 `json`。 */
        virtual QString extension() const = 0;

        /**
         * @brief 序列化为字节流（不落盘），便于预览与测试。
         * @param error_message 失败时的中文原因
         */
        virtual QByteArray serialize(const ScheduleSnapshot& snapshot, QString* error_message = nullptr) const = 0;

        /**
         * @brief 写入指定文件（父目录不存在时应自动创建）。
         * @param file_path 目标文件绝对路径
         */
        virtual bool write(const QString& file_path, const ScheduleSnapshot& snapshot, QString* error_message = nullptr) const = 0;
    };

} // namespace Schedule
