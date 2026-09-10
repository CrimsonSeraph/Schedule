#pragma once

#include "data/IScheduleRepository.h"

#include <QList>
#include <QString>

namespace Schedule {

    /**
     * @brief 应用设置的门面：把键值设置读写包装成**类型安全**的访问器。
     *
     * 所有值都落在仓库的 `settings` 表里；当仓库为空或键不存在时返回默认值，
     * 因此设置页可以在“尚未打开数据库”的早期阶段安全读取。
     *
     * 默认目录规则（硬性要求）：
     * `QStandardPaths::DocumentsLocation + "/Schedule"`，见 `fallback_directory()`。
     */
    class AppSettings {
    public:
        /** 允许的提前提醒分钟数。 */
        static const QList<int>& allowed_reminder_minutes();

        /** 默认提前提醒分钟数。 */
        static int default_reminder_minutes();

        /**
         * @brief 默认导入 / 导出目录。
         * @return `QStandardPaths::DocumentsLocation + "/Schedule"`；极端环境下回退到当前目录。
         */
        static QString fallback_directory();

        /**
         * @brief 确保目录存在（递归创建）。
         * @param path 目录路径；为空时使用 `fallback_directory()`
         * @return 创建或已存在后的绝对路径；失败返回空串
         */
        static QString ensure_directory(const QString& path, QString* error_message = nullptr);

        /** @param repository 仓库指针（不接管所有权，可为空） */
        explicit AppSettings(IScheduleRepository* repository = nullptr);

        /** @brief 更换仓库（不接管所有权）。 */
        void set_repository(IScheduleRepository* repository);

        /** @return 当前仓库；未设置时返回 nullptr。 */
        IScheduleRepository* repository() const;

        // ------------------------------------------------------------ 目录设置

        /** @return 默认导入目录（未设置时回退 `fallback_directory()`）。 */
        QString default_import_dir() const;

        /** @brief 设置默认导入目录；空串表示恢复为默认值。 */
        bool set_default_import_dir(const QString& directory, QString* error_message = nullptr);

        /** @return 默认导出目录（未设置时回退 `fallback_directory()`）。 */
        QString default_export_dir() const;

        /** @brief 设置默认导出目录；空串表示恢复为默认值。 */
        bool set_default_export_dir(const QString& directory, QString* error_message = nullptr);

        /** @return 最近一次导入使用的目录；未记录时回退默认导入目录。 */
        QString last_import_dir() const;

        /** @brief 记录最近一次导入目录（仅用于下次打开对话框的初始位置）。 */
        bool set_last_import_dir(const QString& directory, QString* error_message = nullptr);

        /** @return 最近一次导出使用的目录；未记录时回退默认导出目录。 */
        QString last_export_dir() const;

        /** @brief 记录最近一次导出目录。 */
        bool set_last_export_dir(const QString& directory, QString* error_message = nullptr);

        // ------------------------------------------------------------ 提醒设置

        /** @return 是否启用课程提醒（默认开启）。 */
        bool reminder_enabled() const;

        /** @brief 设置是否启用提醒。 */
        bool set_reminder_enabled(bool enabled, QString* error_message = nullptr);

        /** @return 提前提醒分钟数；取值非法时回退为 `default_reminder_minutes()`。 */
        int reminder_minutes() const;

        /** @brief 设置提前提醒分钟数；仅接受 5 / 10 / 15。 */
        bool set_reminder_minutes(int minutes, QString* error_message = nullptr);

        // -------------------------------------------------------------- 其它

        /** @return 界面主题："system" / "light" / "dark"。 */
        QString theme() const;

        /** @brief 设置界面主题；非法值回退为 "system"。 */
        bool set_theme(const QString& theme, QString* error_message = nullptr);

        /** @return 当前学期 id；未设置时为空串。 */
        QString current_semester_id() const;

        /** @brief 记录当前学期 id。 */
        bool set_current_semester_id(const QString& semester_id, QString* error_message = nullptr);

    private:
        /** @return 读取字符串设置；失败或不存在时返回 default_value。 */
        QString read_string(const QString& key, const QString& default_value) const;

        /** @brief 写入字符串设置。 */
        bool write_string(const QString& key, const QString& value, QString* error_message);

        /** 仓库指针；不持有所有权。 */
        IScheduleRepository* m_repository = nullptr;
    };

} // namespace Schedule
