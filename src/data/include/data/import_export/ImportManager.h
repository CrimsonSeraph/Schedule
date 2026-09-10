#pragma once

#include "core/service/ConflictDetector.h"
#include "data/import_export/IScheduleImporter.h"

#include <memory>
#include <vector>

namespace Schedule {

    /**
     * @brief 导入编排器：格式识别 → 解析 → 预览 → 冲突检测 → 合并 / 覆盖。
     *
     * 使用流程（界面侧）：
     *  1. 用户在**界面层**选择文件，把绝对路径交给本类；
     *  2. `preview()` 解析并生成 `ImportPreview`（含冲突、重复统计、提示），此时**不落库**；
     *  3. 用户确认策略后调用 `apply()`，得到 `ImportResult`，再由上层写入仓库。
     *
     * 内置 JSON / CSV / ICS 三种导入器；新增来源（剪贴板分享码、教务适配器）
     * 只需实现 `IScheduleImporter` 并调用 `register_importer()` 注册。
     */
    class ImportManager {
    public:
        /** 构造并注册内置的 JSON / CSV / ICS 导入器。 */
        ImportManager();

        ~ImportManager();

        ImportManager(const ImportManager&) = delete;
        ImportManager& operator=(const ImportManager&) = delete;

        /**
         * @brief 注册一个导入器（接管所有权）。
         *
         * 同格式重复注册时**后注册者生效**，便于在测试中替换实现。
         */
        void register_importer(std::unique_ptr<IScheduleImporter> importer);

        /** @return 已注册的格式列表（注册顺序）。 */
        QList<ScheduleFormat> supported_formats() const;

        /** @return 指定格式的导入器；未注册返回 nullptr。 */
        const IScheduleImporter* importer_for_format(ScheduleFormat format) const;

        /** @return 按扩展名 + 内容嗅探挑选的导入器；无法识别返回 nullptr。 */
        const IScheduleImporter* importer_for_content(const QByteArray& data) const;

        /** @brief 设置冲突检测开关（导入外部课表时可放宽校验）。 */
        void set_conflict_options(const ConflictDetector::Options& options);

        /** @return 当前冲突检测开关。 */
        ConflictDetector::Options conflict_options() const;

        /**
         * @brief 解析文件并生成预览。
         *
         * @param file_path 源文件绝对路径（由 UI 层的文件对话框提供）
         * @param current   当前课表快照，用于统计重复与“新引入的冲突”
         * @return 预览结果；`is_valid == false` 时 `error_message` 为中文原因
         *
         * @note 本方法**不修改任何数据**，可安全地反复调用。
         */
        ImportPreview preview(const QString& file_path, const ScheduleSnapshot& current) const;

        /**
         * @brief 从内存数据生成预览（剪贴板、分享码、网络响应等预留入口）。
         * @param data        原始字节（UTF-8 文本）
         * @param source_name 逻辑来源名，仅用于错误信息与记录
         */
        ImportPreview preview_data(const QByteArray& data, const QString& source_name, const ScheduleSnapshot& current) const;

        /**
         * @brief 从**已解析的快照**生成预览（教务适配器等外部来源使用）。
         *
         * 与 `preview()` / `preview_data()` 走同一套冲突检测与重复统计逻辑，
         * 因此适配器导入与文件导入的用户体验完全一致。
         *
         * @param parsed      已经解析好的课表快照
         * @param format      内容格式（用于提示文本；未知可传 `ScheduleFormat::Unknown`）
         * @param source_name 逻辑来源名（如适配器名称或接口地址）
         * @param current     当前课表快照
         */
        ImportPreview preview_snapshot(const ScheduleSnapshot& parsed,
            ScheduleFormat format,
            const QString& source_name,
            const ScheduleSnapshot& current) const;

        /**
         * @brief 按指定策略把预览内容应用到快照。
         *
         * @param preview           由 `preview()` 生成的结果
         * @param strategy          合并 / 去重合并 / 覆盖
         * @param in_out_snapshot   就地修改的快照（调用方负责持久化）
         * @return 导入统计结果；失败时 `error_message` 为中文原因
         */
        ImportResult apply(const ImportPreview& preview, ImportStrategy strategy, ScheduleSnapshot* in_out_snapshot) const;

        /** @return 全部导入策略的展示名（顺序与 `strategy_at()` 一致）。 */
        static QStringList strategy_names();

        /** @return 第 index 个策略；越界返回 `ImportStrategy::Merge`。 */
        static ImportStrategy strategy_at(int index);

        /** @return 策略在 `strategy_names()` 中的下标。 */
        static int index_of_strategy(ImportStrategy strategy);

    private:
        /** @brief 由解析结果构造预览（补充冲突、重复统计与提示）。 */
        ImportPreview build_preview(const ScheduleSnapshot& parsed,
            ScheduleFormat format,
            const QString& source_name,
            const ScheduleSnapshot& current) const;

        /** @return 同名同代码（或同 id）的重复课程数量。 */
        static int count_duplicates(const QList<Course>& incoming, const QList<Course>& existing, bool* matched_by_id);

        /** 已注册的导入器；顺序即优先级。 */
        std::vector<std::unique_ptr<IScheduleImporter>> m_importers;

        /** 冲突检测开关。 */
        ConflictDetector::Options m_options;
    };

} // namespace Schedule
