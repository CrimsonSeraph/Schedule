#pragma once

#if defined(Q_OS_ANDROID)

#include "engine/INotificationBackend.h"

namespace Schedule {

    /**
     * @brief Android 本地通知后端（`NotificationManager`）。
     *
     * **仅参与 Android 构建**（`#if defined(Q_OS_ANDROID)`），因此在桌面构建中不会编译，
     * 也不会给 `engine` 增加任何额外依赖（只用到 `Qt6::Core` 的 `QJniObject`）。
     *
     * 实现要点：
     *  - Android 8.0（API 26）起必须先创建 `NotificationChannel` 才能投递通知，
     *    见 `ensure_channel()`；应在应用启动时调用一次；
     *  - Android 13（API 33）起投递通知需要 `POST_NOTIFICATIONS` 运行时权限，
     *    `AndroidManifest.xml` 必须声明该权限，`request_permission()` 负责检查状态；
     *  - 应用退到后台后系统可能冻结进程，因此 `NotificationService` 会在应用回到前台时
     *    重新计算待提醒条目（见该类的 `applicationStateChanged` 处理）。
     *
     * @note 本实现无法在当前桌面开发环境中验证，需要在 Android 真机 / 模拟器上实测。
     */
    class AndroidNotificationBackend : public INotificationBackend {
        Q_OBJECT

    public:
        /** @param parent 父对象 */
        explicit AndroidNotificationBackend(QObject* parent = nullptr);

        /** @brief 创建通知渠道（应用启动时调用一次，重复调用是安全的）。 */
        static void ensure_channel();

        QString name() const override;
        bool is_available() const override;
        bool request_permission(QString* error_message = nullptr) override;
        bool show(const QString& title, const QString& message, QString* error_message = nullptr) override;
        QString permission_text() const override;

    private:
        /** @return 是否已获得 POST_NOTIFICATIONS 权限。 */
        static bool has_post_notifications_permission();
    };

} // namespace Schedule

#endif // Q_OS_ANDROID
