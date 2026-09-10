#pragma once

#include "core/model/ScheduleSnapshot.h"
#include "data/import_export/ImportExportTypes.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace Schedule {

    /**
     * @brief 课表导入器接口。
     *
     * 实现者只需把一种外部格式解析成 `ScheduleSnapshot`，**不负责**合并策略、
     * 冲突检测与落库——这些由 `ImportManager` 统一处理。
     *
     * 扩展方式：
     *  - 新增文件格式：实现本接口并注册到 `ImportManager::register_importer()`；
     *  - 剪贴板 / 分享码等非文件来源：实现本接口并通过 `parse_data()` 提供解析能力，
     *    由调用方传入原始文本（`ImportManager::preview_data()`）。
     */
    class IScheduleImporter {
    public:
        virtual ~IScheduleImporter() = default;

        /** @return 本导入器处理的格式。 */
        virtual ScheduleFormat format() const = 0;

        /** @return 展示名（中文），用于错误信息与界面。 */
        virtual QString display_name() const = 0;

        /** @return 可处理的文件扩展名（含点、小写），如 `{ ".ics", ".ical" }`。 */
        virtual QStringList extensions() const = 0;

        /**
         * @brief 判断本导入器能否处理该文件。
         *
         * 默认实现应同时参考扩展名与内容嗅探，避免用户把 `.txt` 改成 `.json` 后误判。
         * @return 能否处理；不能处理时通过 `error_message` 说明原因
         */
        virtual bool can_import(const QString& file_path, QString* error_message = nullptr) const = 0;

        /**
         * @brief 解析文件。
         * @param file_path  源文件绝对路径
         * @param out_snapshot 输出快照；其中 `semester` 可能是按内容合成的
         * @param error_message 失败时的中文原因
         */
        virtual bool parse(const QString& file_path, ScheduleSnapshot* out_snapshot, QString* error_message = nullptr) const = 0;

        /**
         * @brief 解析内存中的原始数据（剪贴板、分享码、网络响应等）。
         * @param data        原始字节（UTF-8 文本）
         * @param source_name 逻辑来源名，用于错误信息（如 `clipboard://`）
         */
        virtual bool parse_data(const QByteArray& data,
            const QString& source_name,
            ScheduleSnapshot* out_snapshot,
            QString* error_message = nullptr) const = 0;
    };

} // namespace Schedule
