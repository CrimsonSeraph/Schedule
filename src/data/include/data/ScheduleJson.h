#pragma once

#include "core/model/Course.h"
#include "core/model/ScheduleSnapshot.h"
#include "core/model/Semester.h"
#include "core/model/TimeSlot.h"

#include <QByteArray>
#include <QJsonObject>
#include <QString>

namespace Schedule {

    /**
     * @brief 领域模型与 JSON 之间的映射（**唯一**的 JSON 格式权威）。
     *
     * 数据库、JSON 导入 / 导出都复用这里的映射，保证“写出去的 JSON 一定能读回来”。
     *
     * 文档结构（`version` 为 1）：
     *
     * ```json
     * {
     *   "format": "schedule",
     *   "version": 1,
     *   "exported_at": "2024-09-10T12:00:00",
     *   "semester": { "id": "...", "name": "...", "start_date": "2024-09-02", "total_weeks": 16 },
     *   "time_slots": [ { "index": 1, "label": "第 1 节", "start": "08:00", "end": "08:45" } ],
     *   "courses": [
     *     {
     *       "id": "...", "name": "高等数学", "code": "MATH101",
     *       "teacher": "张老师", "location": "教一 101", "color": "#4C8DFF",
     *       "credits": 4.0, "notes": "",
     *       "sessions": [
     *         { "id": "...", "day_of_week": 1, "start_slot": 1, "slot_count": 2,
     *           "weeks": "1-16", "week_bits": "65535", "location": "", "teacher": "" }
     *       ]
     *     }
     *   ]
     * }
     * ```
     *
     * 兼容性策略：
     *  - `weeks` 为人类可读的周次表达式，`week_bits` 为权威的 64 位位图；
     *    读取时优先使用 `week_bits`，缺失时回退解析 `weeks`；
     *  - 未知字段被忽略，缺失字段取默认值，便于向后兼容与手工编辑。
     */
    class ScheduleJson {
    public:
        /** 当前 JSON 文档格式版本。 */
        static constexpr int FORMAT_VERSION = 1;

        /** 文档格式标识，用于拒绝明显不相关的文件。 */
        static QString format_name();

        // ------------------------------------------------------- 领域对象 → JSON

        /** @return 学期对象的 JSON 表示。 */
        static QJsonObject to_json(const Semester& semester);

        /** @return 节次对象的 JSON 表示。 */
        static QJsonObject to_json(const TimeSlot& time_slot);

        /** @return 上课时间段的 JSON 表示。 */
        static QJsonObject to_json(const CourseSession& session);

        /** @return 课程的 JSON 表示。 */
        static QJsonObject to_json(const Course& course);

        /** @return 完整快照的 JSON 表示（不含 format/version 信封字段）。 */
        static QJsonObject to_json(const ScheduleSnapshot& snapshot);

        // ------------------------------------------------------- JSON → 领域对象

        /** @brief 从 JSON 解析学期；缺失字段取默认值，`id` 缺失时自动生成。 */
        static bool semester_from_json(const QJsonObject& object, Semester* out_semester, QString* error_message = nullptr);

        /** @brief 从 JSON 解析节次。 */
        static bool time_slot_from_json(const QJsonObject& object, TimeSlot* out_slot, QString* error_message = nullptr);

        /** @brief 从 JSON 解析上课时间段。 */
        static bool session_from_json(const QJsonObject& object, CourseSession* out_session, QString* error_message = nullptr);

        /** @brief 从 JSON 解析课程（含全部时间段）。 */
        static bool course_from_json(const QJsonObject& object, Course* out_course, QString* error_message = nullptr);

        /**
         * @brief 从 JSON 解析完整快照。
         *
         * 会校验 `format` 与 `version`：格式不符或版本高于 FORMAT_VERSION 时失败。
         */
        static bool snapshot_from_json(const QJsonObject& object, ScheduleSnapshot* out_snapshot, QString* error_message = nullptr);

        // ------------------------------------------------------------ 文档读写

        /** @return 带信封字段的完整文档文本；`pretty` 控制是否缩进。 */
        static QByteArray to_document(const ScheduleSnapshot& snapshot, bool pretty = true);

        /** @brief 解析完整文档。 */
        static bool from_document(const QByteArray& data, ScheduleSnapshot* out_snapshot, QString* error_message = nullptr);

        /** @brief 原子写入文档到指定路径（使用 QSaveFile）。 */
        static bool write_file(const QString& file_path, const ScheduleSnapshot& snapshot, QString* error_message = nullptr);

        /** @brief 从指定路径读取文档。 */
        static bool read_file(const QString& file_path, ScheduleSnapshot* out_snapshot, QString* error_message = nullptr);
    };

} // namespace Schedule
