#pragma once

#include "data/IScheduleRepository.h"

#include <QSqlDatabase>
#include <QString>

namespace Schedule {

    /**
     * @brief 基于 SQLite 的课表仓库实现。
     *
     * 数据库文件默认位于 `QStandardPaths::AppDataLocation/schedule.db`（见
     * `default_database_path()`），也可以传入 `":memory:"` 用于单元测试。
     *
     * **表结构**（`user_version` = 1）：
     *
     * | 表 | 用途 |
     * | -- | ---- |
     * | `semesters` | 学期元数据 |
     * | `time_slots` | 学期作息表（节次） |
     * | `courses` | 课程 |
     * | `course_sessions` | 课程的上课时间段 |
     * | `settings` | 键值设置 |
     * | `import_sources` | 导入来源留痕（本地，不含任何账号信息） |
     *
     * **迁移策略**：所有结构变更都以 `PRAGMA user_version` 递增的迁移步骤表达，
     * 见 `migration_steps()`。升级时逐版本执行，整体包在事务里，失败即回滚。
     *
     * **连接管理**：每个实例使用独立的具名连接，避免测试中多实例互相干扰；
     * `close()` 会移除连接。
     *
     * @note 本类只做“搬运”，不做业务规则判定——冲突检测等在 `core` 层完成。
     */
    class SqliteScheduleRepository : public IScheduleRepository {
    public:
        /**
         * @param database_path   数据库文件路径，或 `":memory:"`
         * @param connection_name 可选连接名；为空时自动生成唯一名字
         */
        explicit SqliteScheduleRepository(QString database_path, QString connection_name = QString());

        ~SqliteScheduleRepository() override;

        // ------------------------------------------------------------ 生命周期

        bool open(QString* error_message = nullptr) override;

        void close() override;

        bool is_open() const override;

        QString location() const override;

        int schema_version() const override;

        // ---------------------------------------------------------------- 学期

        QList<Semester> load_semesters(QString* error_message = nullptr) const override;

        bool save_semester(const Semester& semester, QString* error_message = nullptr) override;

        bool remove_semester(const QString& semester_id, QString* error_message = nullptr) override;

        bool set_current_semester(const QString& semester_id, QString* error_message = nullptr) override;

        Semester current_semester(bool* found = nullptr) const override;

        // ---------------------------------------------------------------- 快照

        bool load_snapshot(const QString& semester_id, ScheduleSnapshot* out_snapshot, QString* error_message = nullptr) const override;

        bool save_snapshot(const ScheduleSnapshot& snapshot, QString* error_message = nullptr) override;

        // ---------------------------------------------------------------- 设置

        QString setting(const QString& key, const QString& default_value = QString()) const override;

        bool set_setting(const QString& key, const QString& value, QString* error_message = nullptr) override;

        QMap<QString, QString> all_settings(QString* error_message = nullptr) const override;

        // ------------------------------------------------------------ 导入来源

        QList<ImportSource> load_import_sources(QString* error_message = nullptr) const override;

        bool add_import_source(const ImportSource& source, QString* error_message = nullptr) override;

        // ---------------------------------------------------------------- 维护

        bool backup_to(const QString& target_path, QString* error_message = nullptr) const override;

        bool restore_from(const QString& source_path, QString* error_message = nullptr) override;

        // ---------------------------------------------------------------- 静态

        /** @return 默认数据库路径：`AppDataLocation/schedule.db`。 */
        static QString default_database_path();

        /** @return 当前实现支持的最新 schema 版本。 */
        static int latest_schema_version();

        /** @return 校验一个文件是否是本应用可识别的 SQLite 数据库。 */
        static bool is_valid_database_file(const QString& file_path, QString* error_message = nullptr);

    private:
        /** @brief 执行一条不返回结果集的 SQL；失败时写入中文错误（含 SQL 语句）。 */
        bool exec(const QString& sql, QString* error_message) const;

        /** @brief 按 `user_version` 逐版本迁移。 */
        bool migrate(QString* error_message);

        /** @return 当前 `PRAGMA user_version` 的值；失败返回 -1。 */
        int read_user_version() const;

        /** @brief 清空并在事务内写入快照的作息表与课程。 */
        bool write_snapshot_content(const ScheduleSnapshot& snapshot, QString* error_message);

        /**
         * @brief 开启事务（若已在事务中则复用外层事务）。
         *
         * `save_snapshot()` 会调用 `save_semester()`，而后者又可能需要调整“当前学期”，
         * 直接嵌套调用 `QSqlDatabase::transaction()` 会报 "cannot start a transaction
         * within a transaction"。因此统一走这三个辅助函数，由最外层负责真正提交。
         */
        bool begin_transaction(QString* error_message);

        /** @brief 提交事务；若当前事务由外层持有则不做任何事。 */
        bool commit_transaction(QString* error_message);

        /** @brief 回滚事务；若当前事务由外层持有则不做任何事。 */
        void rollback_transaction();

        /** 数据库文件路径（或 `":memory:"`）。 */
        QString m_database_path;

        /** Qt 具名连接名；`close()` 后清空。 */
        QString m_connection_name;

        /** Qt 连接句柄。 */
        QSqlDatabase m_database;

        /** 是否已成功打开。 */
        bool m_open = false;

        /** 当前是否处于本实例开启的事务中（用于避免事务嵌套）。 */
        bool m_transaction_active = false;
    };

} // namespace Schedule
