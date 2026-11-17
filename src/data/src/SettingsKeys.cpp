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

    QString SettingsKeys::show_weekend() {
        return QStringLiteral("ui/show_weekend");
    }

    QString SettingsKeys::show_other_weeks() {
        return QStringLiteral("ui/show_other_weeks");
    }

    QString SettingsKeys::log_level() {
        return QStringLiteral("log/level");
    }

    QString SettingsKeys::log_retention_days() {
        return QStringLiteral("log/retention_days");
    }

    QString SettingsKeys::slot_density() {
        return QStringLiteral("ui/slot_density");
    }

    QString SettingsKeys::card_radius() {
        return QStringLiteral("ui/card_radius");
    }

    QString SettingsKeys::adapter_schedule_url() {
        return QStringLiteral("adapter/generic_schedule_url");
    }

    QString SettingsKeys::adapter_login_url() {
        return QStringLiteral("adapter/generic_login_url");
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
            show_weekend(),
            show_other_weeks(),
            log_level(),
            log_retention_days(),
            slot_density(),
            card_radius(),
            adapter_schedule_url(),
            adapter_login_url(),
        };
    }

} // namespace Schedule
