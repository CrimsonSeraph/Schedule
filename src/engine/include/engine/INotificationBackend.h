#pragma once

#include <QObject>
#include <QString>

namespace Schedule {

    /**
     * @brief 系统级通知后端接口。
     *
     * `NotificationService` 只负责“何时通知”，把“怎么通知”交给后端实现：
     *  - 桌面（Windows / Linux / macOS）：`TrayNotificationBackend`（系统托盘气泡），
     *    实现在 `app` 层，因为 `QSystemTrayIcon` 属于 Qt Widgets；
     *  - Android：`AndroidNotificationBackend`（`NotificationManager` 本地通知，本层内实现）；
     *  - 其它平台（含 iOS）：回退到 `NullNotificationBackend`，仅使用应用内横幅。
     *
     * 无论后端能否投递，`NotificationService` 都会发出 `notification_requested` 信号，
     * 由 QML 展示应用内横幅，保证提醒不会因为缺少系统权限而彻底丢失。
     */
    class INotificationBackend : public QObject {
        Q_OBJECT

    public:
        /** @param parent 父对象 */
        explicit INotificationBackend(QObject* parent = nullptr);

        ~INotificationBackend() override;

        /** @return 后端名称（中文），用于设置页展示当前通知方式。 */
        virtual QString name() const = 0;

        /** @return 当前平台 / 系统是否支持该后端。 */
        virtual bool is_available() const = 0;

        /**
         * @brief 申请通知权限。
         *
         * 桌面系统托盘通常无需授权；Android 13+ 需要 `POST_NOTIFICATIONS` 运行时权限。
         *
         * @param error_message 失败原因（面向用户的中文）
         * @return 是否已获得权限（或本平台无需权限）
         */
        virtual bool request_permission(QString* error_message = nullptr) = 0;

        /**
         * @brief 投递一条通知。
         * @param title          标题
         * @param message        正文
         * @param error_message  失败原因（面向用户的中文）
         * @return 是否成功交给系统
         */
        virtual bool show(const QString& title, const QString& message, QString* error_message = nullptr) = 0;

        /** @return 权限状态的展示文本，如“已授权”“未授权”“无需授权”。 */
        virtual QString permission_text() const;
    };

    /**
     * @brief 空后端：不做任何系统级投递。
     *
     * 用于平台不支持、或用户未授予通知权限的场景；此时提醒仍然通过
     * `NotificationService::notification_requested` 在应用内横幅展示。
     */
    class NullNotificationBackend : public INotificationBackend {
        Q_OBJECT

    public:
        /** @param parent 父对象 */
        explicit NullNotificationBackend(QObject* parent = nullptr);

        QString name() const override;
        bool is_available() const override;
        bool request_permission(QString* error_message = nullptr) override;
        bool show(const QString& title, const QString& message, QString* error_message = nullptr) override;
        QString permission_text() const override;
    };

} // namespace Schedule
