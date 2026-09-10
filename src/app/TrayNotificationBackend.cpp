#include "TrayNotificationBackend.h"

#include <QColor>
#include <QFont>
#include <QIcon>
#include <QPainter>
#include <QPixmap>

namespace Schedule {

    namespace {

        /** 托盘气泡展示时长（毫秒）。 */
        constexpr int MESSAGE_TIMEOUT_MS = 10000;

    } // namespace

    TrayNotificationBackend::TrayNotificationBackend(QObject* parent)
        : INotificationBackend(parent) {
    }

    TrayNotificationBackend::~TrayNotificationBackend() {
        delete m_tray_icon;
        m_tray_icon = nullptr;
    }

    QIcon TrayNotificationBackend::build_icon() {
        QIcon icon;
        // 生成多尺寸图标，避免在高 DPI 屏幕上被拉伸模糊
        for (int size : {16, 32, 64}) {
            QPixmap pixmap(size, size);
            pixmap.fill(Qt::transparent);

            QPainter painter(&pixmap);
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setBrush(QColor(0x4C, 0x8D, 0xFF));
            painter.setPen(Qt::NoPen);
            painter.drawRoundedRect(QRectF(0, 0, size, size), size * 0.22, size * 0.22);

            painter.setPen(Qt::white);
            QFont font = painter.font();
            font.setBold(true);
            font.setPixelSize(static_cast<int>(size * 0.62));
            painter.setFont(font);
            painter.drawText(QRectF(0, 0, size, size), Qt::AlignCenter, QStringLiteral("课"));
            painter.end();

            icon.addPixmap(pixmap);
        }
        return icon;
    }

    void TrayNotificationBackend::ensure_tray_icon() {
        if (m_tray_icon) {
            return;
        }
        if (!QSystemTrayIcon::isSystemTrayAvailable()) {
            return;
        }
        m_tray_icon = new QSystemTrayIcon(build_icon(), this);
        m_tray_icon->setToolTip(QStringLiteral("Schedule 课表"));
        m_tray_icon->show();
    }

    QString TrayNotificationBackend::name() const {
        return QStringLiteral("系统托盘通知");
    }

    bool TrayNotificationBackend::is_available() const {
        return QSystemTrayIcon::isSystemTrayAvailable();
    }

    bool TrayNotificationBackend::request_permission(QString* error_message) {
        if (!is_available()) {
            if (error_message) {
                *error_message = QStringLiteral("当前桌面环境没有可用的系统托盘，将只显示应用内提醒");
            }
            return false;
        }
        // 桌面托盘通知不需要运行时授权
        ensure_tray_icon();
        if (error_message) {
            error_message->clear();
        }
        return true;
    }

    bool TrayNotificationBackend::show(const QString& title, const QString& message, QString* error_message) {
        if (!is_available()) {
            if (error_message) {
                *error_message = QStringLiteral("系统托盘不可用");
            }
            return false;
        }
        ensure_tray_icon();
        if (!m_tray_icon) {
            if (error_message) {
                *error_message = QStringLiteral("无法创建托盘图标");
            }
            return false;
        }

        m_tray_icon->showMessage(title, message, QSystemTrayIcon::Information, MESSAGE_TIMEOUT_MS);
        if (error_message) {
            error_message->clear();
        }
        return true;
    }

    QString TrayNotificationBackend::permission_text() const {
        return is_available() ? QStringLiteral("系统托盘已就绪（无需授权）")
                              : QStringLiteral("当前桌面环境不支持系统托盘");
    }

} // namespace Schedule
