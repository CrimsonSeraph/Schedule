#pragma once

#include "data/import_export/IScheduleExporter.h"

#include <memory>
#include <vector>

namespace Schedule {

    /**
     * @brief 导出编排器：决定**目录与文件名**，调用具体导出器写入。
     *
     * 关键约定（硬性要求）：
     *  - 导出目标目录由调用方给出（默认取 `AppSettings::default_export_dir()`，
     *    即 `QStandardPaths::DocumentsLocation + "/Schedule"`），本类只接收路径；
     *  - 目录不存在时自动创建；
     *  - 文件名规则：`Schedule_<学期>_<yyyyMMdd_HHmmss>.<ext>`；
     *  - 返回值 `ExportResult::file_path` 是**实际写入的绝对路径**，界面必须原样提示用户。
     *
     * 文件选择对话框属于 UI 层职责，本类不做任何交互。
     */
    class ExportManager {
    public:
        /** 构造并注册内置的 JSON / CSV / ICS 导出器。 */
        ExportManager();

        ~ExportManager();

        ExportManager(const ExportManager&) = delete;
        ExportManager& operator=(const ExportManager&) = delete;

        /** @brief 注册导出器（接管所有权）；同格式重复注册时后注册者生效。 */
        void register_exporter(std::unique_ptr<IScheduleExporter> exporter);

        /** @return 已注册的格式列表（注册顺序）。 */
        QList<ScheduleFormat> supported_formats() const;

        /** @return 指定格式的导出器；未注册返回 nullptr。 */
        const IScheduleExporter* exporter_for(ScheduleFormat format) const;

        /**
         * @brief 生成推荐文件名（不含目录）。
         *
         * 规则：`Schedule_<清洗后的学期名>_<yyyyMMdd_HHmmss>.<ext>`；
         * 学期名为空时退化为 `Schedule_<yyyyMMdd_HHmmss>.<ext>`。
         */
        static QString suggested_file_name(const QString& semester_name,
            ScheduleFormat format,
            const QDateTime& now = QDateTime::currentDateTime());

        /**
         * @brief 清洗文件名片段：去掉路径分隔符与 Windows 保留字符。
         *
         * 学期名常包含空格、斜杠（如“2024/2025 学年”），直接拼进路径会出问题。
         */
        static QString sanitize_file_component(const QString& text);

        /** @return 用于文件对话框的过滤器字符串（形如 `*.json *.csv *.ics`）。 */
        static QString file_dialog_filter();

        /**
         * @brief 序列化为字节流（不落盘），用于预览与测试。
         * @return 失败返回空字节串并写入 `error_message`
         */
        QByteArray serialize(const ScheduleSnapshot& snapshot, ScheduleFormat format, QString* error_message = nullptr) const;

        /**
         * @brief 导出到**指定文件路径**。
         * @param file_path 目标文件绝对路径（父目录不存在时自动创建）
         */
        ExportResult export_to_file(const QString& file_path, const ScheduleSnapshot& snapshot, ScheduleFormat format) const;

        /**
         * @brief 导出到**指定目录**，文件名自动生成。
         *
         * @param directory 目标目录（由 UI 层的目录选择对话框提供）
         * @param base_name 可选文件名主体；为空时按 `suggested_file_name()` 生成
         */
        ExportResult export_to_directory(const QString& directory,
            const ScheduleSnapshot& snapshot,
            ScheduleFormat format,
            const QString& base_name = QString()) const;

    private:
        /** 已注册的导出器。 */
        std::vector<std::unique_ptr<IScheduleExporter>> m_exporters;
    };

} // namespace Schedule
