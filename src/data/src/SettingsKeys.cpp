#include "data/SettingsKeys.h"

namespace Schedule {

    QString SettingsKeys::default_import_dir() {
        return QStringLiteral("io/default_import_dir");
    }

    QString SettingsKeys::default_export_dir() {
        return QStringLiteral("io/default_export_dir");
    }

    QString SettingsKeys::last_import_dir() {
        return QStringLiteral("io/last_import_dir");
    }

    QString SettingsKeys::last_export_dir() {
        return QStringLiteral("io/last_export_dir");
    }

    QString SettingsKeys::current_semester_id() {
        return QStringLiteral("schedule/current_semester_id");
    }

    QString SettingsKeys::reminder_enabled() {
        return QStringLiteral("reminder/enabled");
    }

    QString SettingsKeys::reminder_minutes() {
        return QStringLiteral("reminder/minutes");
    }

    QString SettingsKeys::theme() {
        return QStringLiteral("ui/theme");
    }

    QStringList SettingsKeys::all_keys() {
        return QStringList{
            default_import_dir(),
            default_export_dir(),
            last_import_dir(),
            last_export_dir(),
            current_semester_id(),
            reminder_enabled(),
            reminder_minutes(),
            theme(),
        };
    }

} // namespace Schedule
