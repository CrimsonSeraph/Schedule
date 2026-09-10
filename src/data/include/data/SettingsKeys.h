#pragma once

#include <QString>
#include <QStringList>

namespace Schedule {

    /**
     * @brief 设置项键名与其默认值。
     *
     * 集中定义避免各处硬编码字符串造成拼写漂移。所有键都通过 `data` 层的
     * `IScheduleRepository::setting()/set_setting()` 读写，落库在 `settings` 表。
     *
     * 键名采用 `域名/键` 形式（如 `io/default_export_dir`），便于将来分组展示。
     */
    class SettingsKeys {
    public:
        /** 默认导入目录；未设置时使用 `AppSettings::fallback_directory()`。 */
        static QString default_import_dir();

        /** 默认导出目录；未设置时使用 `AppSettings::fallback_directory()`。 */
        static QString default_export_dir();

        /** 最近一次导入所在目录（仅用于下次打开对话框时的初始位置）。 */
        static QString last_import_dir();

        /** 最近一次导出所在目录。 */
        static QString last_export_dir();

        /** 当前学期 id；与 `semesters.is_current` 保持同步，便于快速读取。 */
        static QString current_semester_id();

        /** 是否启用课程提醒（"1" / "0"）。 */
        static QString reminder_enabled();

        /** 提前提醒分钟数（"5" / "10" / "15"）。 */
        static QString reminder_minutes();

        /** 界面主题（"system" / "light" / "dark"）。 */
        static QString theme();

        /** 全部已知键，便于设置页遍历与导出。 */
        static QStringList all_keys();
    };

} // namespace Schedule
