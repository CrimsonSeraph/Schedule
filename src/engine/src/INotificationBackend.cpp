#include "engine/INotificationBackend.h"

namespace Schedule {

    INotificationBackend::INotificationBackend(QObject* parent)
        : QObject(parent) {
    }

    INotificationBackend::~INotificationBackend() = default;

    QString INotificationBackend::permission_text() const {
        return is_available() ? QStringLiteral("无需额外授权") : QStringLiteral("当前平台不支持系统通知");
    }

    NullNotificationBackend::NullNotificationBackend(QObject* parent)
        : INotificationBackend(parent) {
    }

    QString NullNotificationBackend::name() const {
        return QStringLiteral("应用内提醒");
    }

    bool NullNotificationBackend::is_available() const {
        return false;
    }

    bool NullNotificationBackend::request_permission(QString* error_message) {
        if (error_message) {
            *error_message = QStringLiteral("当前平台未实现系统通知，将只显示应用内提醒");
        }
        return false;
    }

    bool NullNotificationBackend::show(const QString& title, const QString& message, QString* error_message) {
        Q_UNUSED(title);
        Q_UNUSED(message);
        if (error_message) {
            *error_message = QStringLiteral("没有可用的系统通知后端");
        }
        return false;
    }

    QString NullNotificationBackend::permission_text() const {
        return QStringLiteral("仅应用内提醒");
    }

} // namespace Schedule
