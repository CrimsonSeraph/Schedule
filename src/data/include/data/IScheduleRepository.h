#pragma once

#include "core/model/ScheduleSnapshot.h"
#include "core/model/Semester.h"

#include <QList>
#include <QMap>
#include <QString>

#include "data/ImportSource.h"

namespace Schedule {

    /**
     * @brief 课表仓库接口：定义持久化的**能力契约**，不规定实现介质。
     *
     * 上层（`engine` / `app`）只依赖本接口，因此：
     *  - 可以在测试中注入内存实现或 SQLite 内存库；
     *  - 将来替换存储介质（如换成文档数据库）不影响上层代码。
     *
     * 约定：
     *  - 所有方法都通过 `error_message` 输出**面向用户的中文**错误信息，
     *    失败时返回 false / 空结果；
     *  - 写操作要么整体成功、要么整体回滚（实现需使用事务）；
     *  - 本接口不包含任何 GUI 类型，文件选择对话框由 UI 层负责。
     */
    class IScheduleRepository {
    public:
        virtual ~IScheduleRepository() = default;

        // ------------------------------------------------------------ 生命周期

        /**
         * @brief 打开（并初始化）存储。
         *
         * 实现应在此完成建表与 schema 迁移；重复调用应当幂等。
         * @return 是否成功
         */
        virtual bool open(QString* error_message = nullptr) = 0;

        /** @brief 关闭存储并释放资源；未打开时应当是无操作。 */
        virtual void close() = 0;

        /** @return 是否已打开。 */
        virtual bool is_open() const = 0;

        /** @return 存储位置的可读描述（数据库文件路径 / "memory"）。 */
        virtual QString location() const = 0;

        /** @return 当前 schema 版本号（SQLite 的 `user_version`）。 */
        virtual int schema_version() const = 0;

        // ---------------------------------------------------------------- 学期

        /** @return 全部学期，按起始日期升序。 */
        virtual QList<Semester> load_semesters(QString* error_message = nullptr) const = 0;

        /** @brief 新增或整体更新一个学期（按 id upsert）。 */
        virtual bool save_semester(const Semester& semester, QString* error_message = nullptr) = 0;

        /** @brief 删除学期，并级联删除其作息表、课程与上课时间段。 */
        virtual bool remove_semester(const QString& semester_id, QString* error_message = nullptr) = 0;

        /**
         * @brief 把指定学期标记为“当前学期”。
         *
         * 实现必须保证同一时刻最多只有一个学期 `is_current == true`。
         */
        virtual bool set_current_semester(const QString& semester_id, QString* error_message = nullptr) = 0;

        /** @return 当前学期；没有时返回默认构造值，`found` 输出是否存在。 */
        virtual Semester current_semester(bool* found = nullptr) const = 0;

        // ---------------------------------------------------------------- 快照

        /**
         * @brief 读取某学期的完整快照（学期 + 作息表 + 课程 + 时间段）。
         * @return 是否成功；学期不存在时返回 false 并给出错误信息。
         */
        virtual bool load_snapshot(const QString& semester_id, ScheduleSnapshot* out_snapshot, QString* error_message = nullptr) const = 0;

        /**
         * @brief 整体写入一个快照。
         *
         * 语义为**替换**：该学期原有的作息表与课程会被快照内容取代。
         * 必须在单个事务中完成，避免中途失败留下半份数据。
         */
        virtual bool save_snapshot(const ScheduleSnapshot& snapshot, QString* error_message = nullptr) = 0;

        // ---------------------------------------------------------------- 设置

        /** @return 设置项的值；不存在时返回 `default_value`。 */
        virtual QString setting(const QString& key, const QString& default_value = QString()) const = 0;

        /** @brief 写入（或覆盖）一个设置项。 */
        virtual bool set_setting(const QString& key, const QString& value, QString* error_message = nullptr) = 0;

        /** @return 全部设置项（键 → 值）。 */
        virtual QMap<QString, QString> all_settings(QString* error_message = nullptr) const = 0;

        // ------------------------------------------------------------ 导入来源

        /** @return 导入来源记录，按导入时间倒序（最近的在最前）。 */
        virtual QList<ImportSource> load_import_sources(QString* error_message = nullptr) const = 0;

        /** @brief 追加一条导入来源记录。 */
        virtual bool add_import_source(const ImportSource& source, QString* error_message = nullptr) = 0;

        // ---------------------------------------------------------------- 维护

        /**
         * @brief 备份到指定文件。
         * @param target_path 目标文件绝对路径（父目录不存在时由实现创建）
         */
        virtual bool backup_to(const QString& target_path, QString* error_message = nullptr) const = 0;

        /**
         * @brief 从备份文件恢复。
         *
         * 恢复会**覆盖**当前存储内容；实现应在覆盖前自动备份现有数据，
         * 以便恢复过程失败时回退。
         */
        virtual bool restore_from(const QString& source_path, QString* error_message = nullptr) = 0;
    };

} // namespace Schedule
