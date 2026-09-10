#include "engine/NotificationService.h"

#include "core/service/WeekCalculator.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QEvent>
#include <QVariantMap>

namespace Schedule {

    namespace {

        /** 轮询间隔：30 秒足够精确（提醒粒度是分钟），又不会带来明显开销。 */
        constexpr int POLL_INTERVAL_MS = 30 * 1000;

    } // namespace

    NotificationService::NotificationService(ScheduleService* service, AppSettings* settings, QObject* parent)
        : QObject(parent)
        , m_service(service)
        , m_settings(settings) {
        m_backend = &m_null_backend;
        m_backend_status = m_null_backend.permission_text();

        m_timer.setInterval(POLL_INTERVAL_MS);
        connect(&m_timer, &QTimer::timeout, this, &NotificationService::tick);

        if (m_service) {
            // 课表变化后立刻重算，保证设置页与状态栏展示的是最新数据
            connect(m_service, &ScheduleService::courses_changed, this, &NotificationService::refresh);
            connect(m_service, &ScheduleService::semester_changed, this, &NotificationService::refresh);
            connect(m_service, &ScheduleService::time_slots_changed, this, &NotificationService::refresh);
        }

        // 移动端后台进程可能被冻结，回到前台时补检查一次，避免错过提醒。
        // 这里用 QtCore 级别的 `ApplicationStateChange` 事件而不是
        // `QGuiApplication::applicationStateChanged`，因为 engine 层不依赖 Qt6::Gui。
        if (QCoreApplication* application = QCoreApplication::instance()) {
            application->installEventFilter(this);
        }

        refresh();
    }

    bool NotificationService::eventFilter(QObject* watched, QEvent* event) {
        if (event && event->type() == QEvent::ApplicationStateChange) {
            // 进入前台 / 切到后台都会触发；检查本身是幂等的，统一重算一次即可
            check_now();
            refresh();
        }
        return QObject::eventFilter(watched, event);
    }

    NotificationService::~NotificationService() = default;

    void NotificationService::set_backend(INotificationBackend* backend) {
        if (backend && backend->is_available()) {
            m_backend = backend;
        }
        else {
            m_backend = &m_null_backend;
        }
        m_backend_status = m_backend->permission_text();
        emit backendChanged();
    }

    void NotificationService::start() {
        m_timer.start();
        check_now();
    }

    void NotificationService::stop() {
        m_timer.stop();
    }

    void NotificationService::request_permission() {
        if (!m_backend) {
            return;
        }
        QString error;
        const bool granted = m_backend->request_permission(&error);
        m_backend_status = granted ? m_backend->permission_text() : QStringLiteral("%1：%2").arg(m_backend->name(), error);
        emit backendChanged();
        if (granted) {
            notify(QStringLiteral("通知已启用"), QStringLiteral("将在每节课开始前 %1 分钟提醒你。").arg(minutes_before()));
        }
    }

    // ---------------------------------------------------------------- 属性读取

    bool NotificationService::enabled() const {
        return m_settings ? m_settings->reminder_enabled() : true;
    }

    int NotificationService::minutes_before() const {
        return m_settings ? m_settings->reminder_minutes() : ReminderScheduler::DEFAULT_MINUTES_BEFORE;
    }

    int NotificationService::minutes_index() const {
        const int index = ReminderScheduler::supported_minutes().indexOf(minutes_before());
        return index < 0 ? 0 : index;
    }

    QString NotificationService::backend_name() const {
        return m_backend ? m_backend->name() : QStringLiteral("应用内提醒");
    }

    QString NotificationService::backend_status() const {
        return m_backend_status;
    }

    QString NotificationService::next_reminder_text() const {
        if (!enabled()) {
            return QStringLiteral("提醒已关闭");
        }
        if (!m_has_next) {
            return QStringLiteral("近期没有需要提醒的课程");
        }
        return QStringLiteral("%1（%2）")
            .arg(m_next.title())
            .arg(m_next.remind_at.toString(QStringLiteral("MM-dd HH:mm")));
    }

    QVariantList NotificationService::today_reminders() const {
        QVariantList list;
        for (const Reminder& reminder : m_today) {
            list.append(reminder_to_map(reminder));
        }
        return list;
    }

    QVariantList NotificationService::minutes_options() const {
        QVariantList list;
        for (int minutes : ReminderScheduler::supported_minutes()) {
            QVariantMap option;
            option.insert(QStringLiteral("value"), minutes);
            option.insert(QStringLiteral("label"), QStringLiteral("提前 %1 分钟").arg(minutes));
            list.append(option);
        }
        return list;
    }

    QString NotificationService::last_notification_text() const {
        return m_last_notification_text;
    }

    int NotificationService::poll_interval_ms() {
        return POLL_INTERVAL_MS;
    }

    // -------------------------------------------------------------------- 槽

    void NotificationService::set_enabled(bool enabled) {
        if (m_settings) {
            QString error;
            m_settings->set_reminder_enabled(enabled, &error);
        }
        refresh();
        emit settingsChanged();
    }

    void NotificationService::set_minutes_before(int minutes) {
        if (!ReminderScheduler::is_supported_minutes(minutes)) {
            // 非法值忽略，保持当前设置（QML 的 ComboBox 只提供 5/10/15）
            return;
        }
        if (m_settings) {
            QString error;
            m_settings->set_reminder_minutes(minutes, &error);
        }
        refresh();
        emit settingsChanged();
    }

    ScheduleSnapshot NotificationService::snapshot() const {
        return m_service ? m_service->snapshot() : ScheduleSnapshot();
    }

    void NotificationService::refresh() {
        const int minutes = minutes_before();

        m_today = ReminderScheduler::for_date(snapshot(), QDate::currentDate(), minutes);

        m_has_next = ReminderScheduler::next(snapshot(), QDateTime::currentDateTime(), minutes, &m_next);
        if (!m_has_next) {
            m_next = Reminder();
        }

        emit scheduleChanged();
    }

    void NotificationService::tick() {
        check_now();
        refresh();
    }

    void NotificationService::check_now() {
        if (!enabled()) {
            return;
        }

        const QDateTime now = QDateTime::currentDateTime();
        if (m_notified_date != now.date()) {
            // 跨天清空去重集合，保证次日提醒正常触发
            m_notified_keys.clear();
            m_notified_date = now.date();
        }

        const int minutes = minutes_before();
        const QList<Reminder> candidates = ReminderScheduler::upcoming(snapshot(), now, minutes, 2);
        for (const Reminder& reminder : candidates) {
            if (!ReminderScheduler::is_due(reminder, now)) {
                continue;
            }
            const QString key = reminder.unique_key();
            if (m_notified_keys.contains(key)) {
                continue;
            }
            m_notified_keys.insert(key);
            notify(reminder.title(), reminder.message());
            emit reminderDue(reminder.course_id, reminder.course_name, reminder.minutes_before);
        }
    }

    void NotificationService::show_test_notification() {
        notify(QStringLiteral("这是一条测试提醒"),
            QStringLiteral("如果看到系统通知，说明 %1 工作正常。").arg(backend_name()));
    }

    // -------------------------------------------------------------------- 内部

    void NotificationService::notify(const QString& title, const QString& message) {
        // 1) 应用内横幅：即使系统通知不可用也不会丢失提醒
        m_last_notification_text = QStringLiteral("%1 · %2").arg(title, message);
        emit notificationRequested(title, message);

        // 2) 系统通知：交给后端投递，失败只记录不阻塞
        if (m_backend && m_backend->is_available()) {
            QString error;
            if (!m_backend->show(title, message, &error) && !error.isEmpty()) {
                m_backend_status = QStringLiteral("%1：%2").arg(m_backend->name(), error);
                emit backendChanged();
            }
        }
    }

    QVariantMap NotificationService::reminder_to_map(const Reminder& reminder) {
        QVariantMap map;
        map.insert(QStringLiteral("courseId"), reminder.course_id);
        map.insert(QStringLiteral("courseName"), reminder.course_name);
        map.insert(QStringLiteral("sessionId"), reminder.session_id);
        map.insert(QStringLiteral("location"), reminder.location);
        map.insert(QStringLiteral("teacher"), reminder.teacher);
        map.insert(QStringLiteral("week"), reminder.week);
        map.insert(QStringLiteral("dayOfWeek"), reminder.day_of_week);
        map.insert(QStringLiteral("dayName"), WeekCalculator::day_name(reminder.day_of_week));
        map.insert(QStringLiteral("start"), reminder.start.toString(QStringLiteral("HH:mm")));
        map.insert(QStringLiteral("remindAt"), reminder.remind_at.toString(QStringLiteral("HH:mm")));
        map.insert(QStringLiteral("title"), reminder.title());
        map.insert(QStringLiteral("message"), reminder.message());
        map.insert(QStringLiteral("minutesBefore"), reminder.minutes_before);
        return map;
    }

} // namespace Schedule
