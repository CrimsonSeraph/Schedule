#pragma once

#include "core/service/ReminderScheduler.h"
#include "core/service/ScheduleService.h"
#include "data/AppSettings.h"
#include "engine/INotificationBackend.h"

#include <QList>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantList>

class QEvent;

namespace Schedule {

    /**
     * @brief 本地课程提醒服务：按设置的提前分钟数，在上课前弹出系统通知。
     *
     * **工作方式**：
     *  - 每 30 秒轮询一次（`QTimer`），用 `core::ReminderScheduler` 计算“当前应提醒”的课程；
     *  - 到点后先发出 `notificationRequested(title, message)`（QML 据此显示应用内横幅），
     *    再交给已注册的 `INotificationBackend` 投递系统通知；
     *  - 同一条提醒（课程 + 时间段 + 周次 + 日期）只会通知一次，避免轮询期间反复弹出；
     *  - 应用从后台回到前台时立即补检查一次（移动端进程可能被冻结，见 `applicationStateChanged`）。
     *
     * **平台差异**：桌面端由 `app` 层注册 `TrayNotificationBackend`（系统托盘气泡）；
     * Android 使用 `AndroidNotificationBackend`；其它平台回退为“仅应用内提醒”。
     *
     * **权限**：`backendStatus` 暴露当前后端的权限状态，`request_permission()` 可主动申请。
     */
    class NotificationService : public QObject {
        Q_OBJECT

        /** 是否启用提醒（持久化在设置中）。 */
        Q_PROPERTY(bool enabled READ enabled NOTIFY settingsChanged)

        /** 提前提醒分钟数（5 / 10 / 15）。 */
        Q_PROPERTY(int minutesBefore READ minutes_before NOTIFY settingsChanged)

        /** 当前分钟数在 `minutesOptions` 中的下标（5→0 / 10→1 / 15→2），供 ComboBox 直接绑定。 */
        Q_PROPERTY(int minutesIndex READ minutes_index NOTIFY settingsChanged)

        /** 当前通知后端名称。 */
        Q_PROPERTY(QString backendName READ backend_name NOTIFY backendChanged)

        /** 当前后端的权限 / 可用性描述。 */
        Q_PROPERTY(QString backendStatus READ backend_status NOTIFY backendChanged)

        /** 下一次提醒的可读文本；没有待提醒时为提示语。 */
        Q_PROPERTY(QString nextReminderText READ next_reminder_text NOTIFY scheduleChanged)

        /** 今天的课程提醒（`title` / `message` / `time` / `courseId` 等字段）。 */
        Q_PROPERTY(QVariantList todayReminders READ today_reminders NOTIFY scheduleChanged)

        /** 可选的提前分钟数（供 ComboBox 使用），元素为 `{ value, label }`。 */
        Q_PROPERTY(QVariantList minutesOptions READ minutes_options CONSTANT)

        /** 最近一次通知的文本，用于状态栏回显。 */
        Q_PROPERTY(QString lastNotificationText READ last_notification_text NOTIFY notificationRequested)

    public:
        /**
         * @param service  课表服务（不持有所有权）
         * @param settings 设置门面（不持有所有权）
         * @param parent   父对象
         */
        NotificationService(ScheduleService* service, AppSettings* settings, QObject* parent = nullptr);

        ~NotificationService() override;

        /**
         * @brief 注册系统通知后端（不接管所有权，由 `app` 层持有）。
         *
         * 传 nullptr 或后端不可用时会自动回退到 `NullNotificationBackend`（仅应用内提醒）。
         */
        void set_backend(INotificationBackend* backend);

        /** @brief 启动轮询定时器并立即计算一次待提醒列表。 */
        void start();

        /** @brief 停止轮询（应用退出前调用，非必需）。 */
        void stop();

        /** @brief 主动申请通知权限，结果写入 `backendStatus`。 */
        void request_permission();

        // ------------------------------------------------------------ 属性读取

        bool enabled() const;
        int minutes_before() const;
        int minutes_index() const;
        QString backend_name() const;
        QString backend_status() const;
        QString next_reminder_text() const;
        QVariantList today_reminders() const;
        QVariantList minutes_options() const;
        QString last_notification_text() const;

        /** @return 轮询间隔（毫秒）。 */
        static int poll_interval_ms();

    public slots:
        /** @brief 启用 / 停用提醒，并持久化到设置。 */
        void set_enabled(bool enabled);

        /** @brief 设置提前提醒分钟数（仅接受 5 / 10 / 15），并持久化到设置。 */
        void set_minutes_before(int minutes);

        /** @brief 重新计算待提醒列表并发出 `scheduleChanged`。 */
        void refresh();

        /** @brief 立即检查一次是否有到点的提醒。 */
        void check_now();

        /** @brief 立刻弹出一条测试通知（用于验证权限与后端）。 */
        void show_test_notification();

    protected:
        /**
         * @brief 监听应用前后台切换事件。
         *
         * 移动端退到后台后进程可能被系统冻结，`QTimer` 不再准时；回到前台时
         * 必须补检查一次，否则会漏掉这段时间内的提醒。
         *
         * @note 使用 QtCore 的 `QEvent::ApplicationStateChange` 而不是
         *       `QGuiApplication::applicationStateChanged`，以避免 engine 层依赖 `Qt6::Gui`。
         */
        bool eventFilter(QObject* watched, QEvent* event) override;

    signals:
        /** 提醒开关或提前分钟数变化后发出。 */
        void settingsChanged();

        /** 通知后端或其权限状态变化后发出。 */
        void backendChanged();

        /** 待提醒列表（下一次提醒 / 今日课程）变化后发出。 */
        void scheduleChanged();

        /**
         * @brief 需要展示一条提醒。
         * @param title   标题（如“10 分钟后上课：高等数学”）
         * @param message 正文（时间 · 地点 · 教师 · 周次）
         */
        void notificationRequested(const QString& title, const QString& message);

        /**
         * @brief 一条课程提醒到点。
         * @param course_id     课程 id
         * @param course_name   课程名称
         * @param minutes_before 提前分钟数
         */
        void reminderDue(const QString& course_id, const QString& course_name, int minutes_before);

    private:
        /** @brief 定时轮询：检查到点提醒并刷新列表。 */
        void tick();

        /** @return 课表快照（服务为空时返回空快照）。 */
        ScheduleSnapshot snapshot() const;

        /** @brief 投递一条通知（应用内 + 系统）。 */
        void notify(const QString& title, const QString& message);

        /** @return 提醒的 QML 友好表示。 */
        static QVariantMap reminder_to_map(const Reminder& reminder);

        /** 课表服务；不持有所有权。 */
        ScheduleService* m_service = nullptr;

        /** 设置门面；不持有所有权。 */
        AppSettings* m_settings = nullptr;

        /** 当前通知后端；不持有所有权。 */
        INotificationBackend* m_backend = nullptr;

        /** 默认（空）后端，避免 `m_backend` 为空时的分支判断。 */
        NullNotificationBackend m_null_backend;

        /** 轮询定时器。 */
        QTimer m_timer;

        /** 已通知过的提醒键，避免重复弹出。 */
        QSet<QString> m_notified_keys;

        /** 已通知键对应的日期，跨天时清空。 */
        QDate m_notified_date;

        /** 今日提醒缓存。 */
        QList<Reminder> m_today;

        /** 下一次提醒缓存。 */
        Reminder m_next;

        /** 是否存在下一次提醒。 */
        bool m_has_next = false;

        /** 后端状态文本。 */
        QString m_backend_status;

        /** 最近一次通知文本。 */
        QString m_last_notification_text;
    };

} // namespace Schedule
