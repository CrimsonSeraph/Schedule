#pragma once

#include "engine/INotificationBackend.h"

#include <QSystemTrayIcon>

namespace Schedule {

    /**
     * @brief 桌面系统托盘通知后端（Windows / Linux / macOS）。
     *
     * 使用 `QSystemTrayIcon::showMessage()` 投递气泡通知。之所以放在 `app` 层而不是
     * `engine` 层，是因为 `QSystemTrayIcon` 属于 **Qt Widgets**，而 `engine` 的依赖被
     * 限定为 `core` + `data` + `Qt6::Core/Qml`；`app` 作为组装层引入 Widgets 不影响分层。
     *
     * 行为约定：
     *  - 系统托盘不可用（例如无托盘的 Linux 桌面、部分 Wayland 环境）时
     *    `is_available()` 返回 false，`NotificationService` 会自动回退为“仅应用内提醒”；
     *  - 托盘图标在首次可用时创建并常驻，保证 `showMessage()` 有宿主；
     *  - 桌面托盘通知**无需运行时权限**，`request_permission()` 恒为成功。
     */
    class TrayNotificationBackend : public INotificationBackend {
        Q_OBJECT

    public:
        /** @param parent 父对象 */
        explicit TrayNotificationBackend(QObject* parent = nullptr);

        ~TrayNotificationBackend() override;

        QString name() const override;

        bool is_available() const override;

        bool request_permission(QString* error_message = nullptr) override;

        bool show(const QString& title, const QString& message, QString* error_message = nullptr) override;

        QString permission_text() const override;

    private:
        /** @brief 懒创建托盘图标；失败时保持为空指针。 */
        void ensure_tray_icon();

        /** @return 程序内生成的托盘图标（16/32/64 多尺寸）。 */
        static QIcon build_icon();

        /** 托盘图标；生命周期由本对象管理。 */
        QSystemTrayIcon* m_tray_icon = nullptr;
    };

} // namespace Schedule
