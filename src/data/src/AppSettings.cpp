#include "data/AppSettings.h"

#include "data/SettingsKeys.h"

#include <QDir>
#include <QStandardPaths>

namespace Schedule {

    namespace {

        /** 主题允许的取值。 */
        const QStringList THEME_VALUES = {
            QStringLiteral("system"),
            QStringLiteral("light"),
            QStringLiteral("dark"),
        };

        /** @brief 统一设置错误。 */
        bool fail(QString* error_message, const QString& message) {
            if (error_message) {
                *error_message = message;
            }
            return false;
        }

    } // namespace

    const QList<int>& AppSettings::allowed_reminder_minutes() {
        static const QList<int> values = {5, 10, 15};
        return values;
    }

    int AppSettings::default_reminder_minutes() {
        return 10;
    }

    QString AppSettings::fallback_directory() {
        QString documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
        if (documents.isEmpty()) {
            // 精简环境（无 HOME / 无文档目录）下退回当前工作目录，保证功能可用
            documents = QDir::currentPath();
        }
        return QDir(documents).filePath(QStringLiteral("Schedule"));
    }

    QString AppSettings::ensure_directory(const QString& path, QString* error_message) {
        const QString target = path.isEmpty() ? fallback_directory() : path;
        QDir directory(target);
        if (directory.exists()) {
            if (error_message) {
                error_message->clear();
            }
            return directory.absolutePath();
        }
        if (!directory.mkpath(QStringLiteral("."))) {
            fail(error_message, QStringLiteral("无法创建目录：%1").arg(target));
            return QString();
        }
        if (error_message) {
            error_message->clear();
        }
        return directory.absolutePath();
    }

    AppSettings::AppSettings(IScheduleRepository* repository)
        : m_repository(repository) {
    }

    void AppSettings::set_repository(IScheduleRepository* repository) {
        m_repository = repository;
    }

    IScheduleRepository* AppSettings::repository() const {
        return m_repository;
    }

    QString AppSettings::read_string(const QString& key, const QString& default_value) const {
        if (!m_repository) {
            return default_value;
        }
        return m_repository->setting(key, default_value);
    }

    bool AppSettings::write_string(const QString& key, const QString& value, QString* error_message) {
        if (!m_repository) {
            return fail(error_message, QStringLiteral("设置存储尚未就绪（仓库为空）"));
        }
        return m_repository->set_setting(key, value, error_message);
    }

    // ---------------------------------------------------------------- 目录设置

    QString AppSettings::default_import_dir() const {
        const QString stored = read_string(SettingsKeys::default_import_dir(), QString());
        return stored.isEmpty() ? fallback_directory() : stored;
    }

    bool AppSettings::set_default_import_dir(const QString& directory, QString* error_message) {
        return write_string(SettingsKeys::default_import_dir(), directory.trimmed(), error_message);
    }

    QString AppSettings::default_export_dir() const {
        const QString stored = read_string(SettingsKeys::default_export_dir(), QString());
        return stored.isEmpty() ? fallback_directory() : stored;
    }

    bool AppSettings::set_default_export_dir(const QString& directory, QString* error_message) {
        return write_string(SettingsKeys::default_export_dir(), directory.trimmed(), error_message);
    }

    QString AppSettings::last_import_dir() const {
        const QString stored = read_string(SettingsKeys::last_import_dir(), QString());
        return stored.isEmpty() ? default_import_dir() : stored;
    }

    bool AppSettings::set_last_import_dir(const QString& directory, QString* error_message) {
        return write_string(SettingsKeys::last_import_dir(), directory.trimmed(), error_message);
    }

    QString AppSettings::last_export_dir() const {
        const QString stored = read_string(SettingsKeys::last_export_dir(), QString());
        return stored.isEmpty() ? default_export_dir() : stored;
    }

    bool AppSettings::set_last_export_dir(const QString& directory, QString* error_message) {
        return write_string(SettingsKeys::last_export_dir(), directory.trimmed(), error_message);
    }

    // ---------------------------------------------------------------- 提醒设置

    bool AppSettings::reminder_enabled() const {
        return read_string(SettingsKeys::reminder_enabled(), QStringLiteral("1")) != QStringLiteral("0");
    }

    bool AppSettings::set_reminder_enabled(bool enabled, QString* error_message) {
        return write_string(SettingsKeys::reminder_enabled(), enabled ? QStringLiteral("1") : QStringLiteral("0"), error_message);
    }

    bool AppSettings::course_reminder_enabled(const QString& course_id) const {
        if (course_id.isEmpty()) {
            return true;
        }
        return read_string(QStringLiteral("reminder/course/%1").arg(course_id), QStringLiteral("1")) != QStringLiteral("0");
    }

    bool AppSettings::set_course_reminder_enabled(const QString& course_id, bool enabled, QString* error_message) {
        if (course_id.isEmpty()) {
            return true;
        }
        return write_string(QStringLiteral("reminder/course/%1").arg(course_id),
            enabled ? QStringLiteral("1") : QStringLiteral("0"), error_message);
    }

    int AppSettings::reminder_minutes() const {
        bool ok = false;
        const int minutes = read_string(SettingsKeys::reminder_minutes(), QString::number(default_reminder_minutes())).toInt(&ok);
        if (!ok || !allowed_reminder_minutes().contains(minutes)) {
            // 手工改坏数据库时也要能正常启动，因此回退默认值而不是报错
            return default_reminder_minutes();
        }
        return minutes;
    }

    bool AppSettings::set_reminder_minutes(int minutes, QString* error_message) {
        if (!allowed_reminder_minutes().contains(minutes)) {
            return fail(error_message,
                QStringLiteral("提前提醒分钟数只支持 %1，当前为 %2")
                    .arg(QStringLiteral("5 / 10 / 15"))
                    .arg(minutes));
        }
        return write_string(SettingsKeys::reminder_minutes(), QString::number(minutes), error_message);
    }

    // -------------------------------------------------------------------- 其它

    QString AppSettings::theme() const {
        const QString stored = read_string(SettingsKeys::theme(), QStringLiteral("system"));
        return THEME_VALUES.contains(stored) ? stored : QStringLiteral("system");
    }

    bool AppSettings::set_theme(const QString& theme, QString* error_message) {
        const QString normalised = THEME_VALUES.contains(theme) ? theme : QStringLiteral("system");
        return write_string(SettingsKeys::theme(), normalised, error_message);
    }

    // ---------------------------------------------------------------- 显示设置

    bool AppSettings::show_weekend() const {
        return read_string(SettingsKeys::show_weekend(), QStringLiteral("1")) != QStringLiteral("0");
    }

    bool AppSettings::set_show_weekend(bool enabled, QString* error_message) {
        return write_string(SettingsKeys::show_weekend(), enabled ? QStringLiteral("1") : QStringLiteral("0"), error_message);
    }

    bool AppSettings::show_other_weeks() const {
        return read_string(SettingsKeys::show_other_weeks(), QStringLiteral("0")) == QStringLiteral("1");
    }

    bool AppSettings::set_show_other_weeks(bool enabled, QString* error_message) {
        return write_string(SettingsKeys::show_other_weeks(), enabled ? QStringLiteral("1") : QStringLiteral("0"), error_message);
    }

    int AppSettings::log_level() const {
        return qBound(0, read_string(SettingsKeys::log_level(), QStringLiteral("0")).toInt(), 3);
    }

    bool AppSettings::set_log_level(int level, QString* error_message) {
        return write_string(SettingsKeys::log_level(), QString::number(qBound(0, level, 3)), error_message);
    }

    int AppSettings::log_retention_days() const {
        return qBound(1, read_string(SettingsKeys::log_retention_days(), QStringLiteral("7")).toInt(), 365);
    }

    int AppSettings::font_scale() const {
        return qBound(0, read_string(SettingsKeys::font_scale(), QStringLiteral("1")).toInt(), 3);
    }

    bool AppSettings::set_font_scale(int index, QString* error_message) {
        return write_string(SettingsKeys::font_scale(), QString::number(qBound(0, index, 3)), error_message);
    }

    bool AppSettings::set_log_retention_days(int days, QString* error_message) {
        return write_string(SettingsKeys::log_retention_days(), QString::number(qBound(1, days, 365)), error_message);
    }

    int AppSettings::slot_density() const {
        bool ok = false;
        const int density = read_string(SettingsKeys::slot_density(), QStringLiteral("1")).toInt(&ok);
        if (!ok || density < 0 || density > 2) {
            return 1;
        }
        return density;
    }

    bool AppSettings::set_slot_density(int density, QString* error_message) {
        if (density < 0 || density > 2) {
            return fail(error_message, QStringLiteral("格子高度只支持 0 紧凑 / 1 标准 / 2 宽松"));
        }
        return write_string(SettingsKeys::slot_density(), QString::number(density), error_message);
    }

    int AppSettings::card_radius() const {
        bool ok = false;
        const int radius = read_string(SettingsKeys::card_radius(), QStringLiteral("1")).toInt(&ok);
        if (!ok || radius < 0 || radius > 2) {
            return 1;
        }
        return radius;
    }

    bool AppSettings::set_card_radius(int radius, QString* error_message) {
        if (radius < 0 || radius > 2) {
            return fail(error_message, QStringLiteral("课卡圆角只支持 0 小 / 1 中 / 2 大"));
        }
        return write_string(SettingsKeys::card_radius(), QString::number(radius), error_message);
    }

    QString AppSettings::adapter_schedule_url() const {
        return read_string(SettingsKeys::adapter_schedule_url(), QString());
    }

    bool AppSettings::set_adapter_schedule_url(const QString& url, QString* error_message) {
        return write_string(SettingsKeys::adapter_schedule_url(), url.trimmed(), error_message);
    }

    QString AppSettings::adapter_login_url() const {
        return read_string(SettingsKeys::adapter_login_url(), QString());
    }

    bool AppSettings::set_adapter_login_url(const QString& url, QString* error_message) {
        return write_string(SettingsKeys::adapter_login_url(), url.trimmed(), error_message);
    }

    QString AppSettings::current_semester_id() const {
        return read_string(SettingsKeys::current_semester_id(), QString());
    }

    bool AppSettings::set_current_semester_id(const QString& semester_id, QString* error_message) {
        return write_string(SettingsKeys::current_semester_id(), semester_id, error_message);
    }

} // namespace Schedule
