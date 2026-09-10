#include "engine/AndroidNotificationBackend.h"

#if defined(Q_OS_ANDROID)

#include <QJniObject>
#include <QtCore/qnativeinterface.h>

namespace Schedule {

    namespace {

        /** 通知渠道 id：同一应用内固定不变。 */
        const char* const CHANNEL_ID = "schedule_reminders";

        /** 通知 id：同一时刻只需展示一条课程提醒，固定 id 可覆盖上一条。 */
        constexpr int NOTIFICATION_ID = 1001;

        /** @return 应用 Context。 */
        QJniObject android_context() {
            return QNativeInterface::QAndroidApplication::context();
        }

        /** @return 系统 NotificationManager。 */
        QJniObject notification_manager() {
            Qt::HANDLE unused = nullptr;
            Q_UNUSED(unused);
            QJniObject context = android_context();
            if (!context.isValid()) {
                return QJniObject();
            }
            return context.callObjectMethod("getSystemService",
                "(Ljava/lang/String;)Ljava/lang/Object;",
                QJniObject::fromString(QStringLiteral("notification")).object<jstring>());
        }

    } // namespace

    AndroidNotificationBackend::AndroidNotificationBackend(QObject* parent)
        : INotificationBackend(parent) {
        ensure_channel();
    }

    void AndroidNotificationBackend::ensure_channel() {
        QJniObject context = android_context();
        QJniObject manager = notification_manager();
        if (!context.isValid() || !manager.isValid()) {
            return;
        }

        // new NotificationChannel(String id, CharSequence name, int importance)
        QJniObject channel(QStringLiteral("android/app/NotificationChannel"),
            QStringLiteral("(Ljava/lang/String;Ljava/lang/CharSequence;I)V"),
            QJniObject::fromString(QString::fromLatin1(CHANNEL_ID)).object<jstring>(),
            QJniObject::fromString(QStringLiteral("课程提醒")).object<jstring>(),
            jint(3) /* NotificationManager.IMPORTANCE_DEFAULT */);
        if (!channel.isValid()) {
            return;
        }
        channel.callMethod<void>("setDescription",
            QStringLiteral("(Ljava/lang/String;)V"),
            QJniObject::fromString(QStringLiteral("上课前提醒")).object<jstring>());
        manager.callMethod<void>("createNotificationChannel",
            QStringLiteral("(Landroid/app/NotificationChannel;)V"),
            channel.object<jobject>());
    }

    QString AndroidNotificationBackend::name() const {
        return QStringLiteral("Android 本地通知");
    }

    bool AndroidNotificationBackend::is_available() const {
        return android_context().isValid();
    }

    bool AndroidNotificationBackend::has_post_notifications_permission() {
        QJniObject context = android_context();
        if (!context.isValid()) {
            return false;
        }
        // Android 12 及以下没有该权限，checkSelfPermission 会返回 PERMISSION_GRANTED
        const jint granted = context.callMethod<jint>("checkSelfPermission",
            QStringLiteral("(Ljava/lang/String;)I"),
            QJniObject::fromString(QStringLiteral("android.permission.POST_NOTIFICATIONS"))
                .object<jstring>());
        return granted == 0; // PackageManager.PERMISSION_GRANTED
    }

    bool AndroidNotificationBackend::request_permission(QString* error_message) {
        if (!is_available()) {
            if (error_message) {
                *error_message = QStringLiteral("当前环境不可用 Android 通知服务");
            }
            return false;
        }
        if (has_post_notifications_permission()) {
            if (error_message) {
                error_message->clear();
            }
            return true;
        }
        if (error_message) {
            *error_message = QStringLiteral("尚未授予通知权限，请在系统设置中允许本应用发送通知"
                                            "（AndroidManifest 需声明 POST_NOTIFICATIONS）");
        }
        return false;
    }

    bool AndroidNotificationBackend::show(const QString& title, const QString& message, QString* error_message) {
        QJniObject context = android_context();
        QJniObject manager = notification_manager();
        if (!context.isValid() || !manager.isValid()) {
            if (error_message) {
                *error_message = QStringLiteral("无法获取 Android 通知管理器");
            }
            return false;
        }

        QJniObject builder(QStringLiteral("android/app/Notification$Builder"),
            QStringLiteral("(Landroid/content/Context;Ljava/lang/String;)V"),
            context.object<jobject>(),
            QJniObject::fromString(QString::fromLatin1(CHANNEL_ID)).object<jstring>());
        if (!builder.isValid()) {
            if (error_message) {
                *error_message = QStringLiteral("无法创建 Android 通知构造器");
            }
            return false;
        }

        const jint small_icon = QJniObject::getStaticField<jint>(QStringLiteral("android/R$drawable"),
            QStringLiteral("ic_dialog_info"));
        builder.callObjectMethod("setSmallIcon",
            QStringLiteral("(I)Landroid/app/Notification$Builder;"),
            small_icon);
        builder.callObjectMethod("setContentTitle",
            QStringLiteral("(Ljava/lang/CharSequence;)Landroid/app/Notification$Builder;"),
            QJniObject::fromString(title).object<jstring>());
        builder.callObjectMethod("setContentText",
            QStringLiteral("(Ljava/lang/CharSequence;)Landroid/app/Notification$Builder;"),
            QJniObject::fromString(message).object<jstring>());
        builder.callObjectMethod("setAutoCancel", QStringLiteral("(Z)Landroid/app/Notification$Builder;"), jboolean(true));

        QJniObject notification = builder.callObjectMethod("build", QStringLiteral("()Landroid/app/Notification;"));
        if (!notification.isValid()) {
            if (error_message) {
                *error_message = QStringLiteral("无法构建 Android 通知");
            }
            return false;
        }

        manager.callMethod<void>("notify",
            QStringLiteral("(ILandroid/app/Notification;)V"),
            jint(NOTIFICATION_ID),
            notification.object<jobject>());

        if (error_message) {
            error_message->clear();
        }
        return true;
    }

    QString AndroidNotificationBackend::permission_text() const {
        if (!is_available()) {
            return QStringLiteral("当前环境不支持 Android 通知");
        }
        return has_post_notifications_permission() ? QStringLiteral("已授权") : QStringLiteral("未授权（需在系统设置中允许通知）");
    }

} // namespace Schedule

#endif // Q_OS_ANDROID
