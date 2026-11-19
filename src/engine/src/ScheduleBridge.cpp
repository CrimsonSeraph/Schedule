#include "engine/ScheduleBridge.h"

#include "core/model/WeekMask.h"
#include "core/service/ConflictDetector.h"
#include "core/service/WeekCalculator.h"

#include <QDate>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QStandardPaths>
#include <QStyleHints>
#if defined(Q_OS_ANDROID)
#include <QJniObject>
#endif
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace Schedule {

    namespace {

        /**
         * @brief 把 QML 传来的值转成课程时间段。
         *
         * QML 侧的 map 字段名与 `CourseListModel::get()` 的输出保持一致，
         * 因此“读出来编辑 → 原样写回去”是闭合的。
         *
         * @param data 单个时间段的 map
         * @param semester 当前学期（用于校验周次上界）
         * @param error_message 失败原因
         * @param out_session 解析结果
         */
        bool session_from_map(const QVariantMap& data,
            const Semester& semester,
            QString* error_message,
            CourseSession* out_session) {
            CourseSession session;
            session.id = data.value(QStringLiteral("sessionId")).toString();
            session.day_of_week = data.value(QStringLiteral("dayOfWeek")).toInt();
            session.start_slot = data.value(QStringLiteral("startSlot")).toInt();

            const int slot_count = data.value(QStringLiteral("slotCount")).toInt();
            session.slot_count = slot_count >= 1 ? slot_count : 1;
            session.location = data.value(QStringLiteral("location")).toString();
            session.teacher = data.value(QStringLiteral("teacher")).toString();

            const QString weeks_text = data.value(QStringLiteral("weeks")).toString();
            QString weeks_error;
            session.weeks = WeekMask::from_expression(weeks_text, semester.total_weeks, &weeks_error);
            if (!weeks_error.isEmpty()) {
                if (error_message) {
                    *error_message = QStringLiteral("周次非法：%1").arg(weeks_error);
                }
                return false;
            }
            if (session.weeks.is_empty()) {
                if (error_message) {
                    *error_message = QStringLiteral("周次不能为空");
                }
                return false;
            }

            if (!session.is_valid(-1, error_message)) {
                return false;
            }

            *out_session = session;
            return true;
        }

    } // namespace

    ScheduleBridge::ScheduleBridge(ScheduleService* service,
        IScheduleRepository* repository,
        AppSettings* settings,
        QObject* parent)
        : QObject(parent)
        , m_service(service)
        , m_repository(repository)
        , m_settings(settings) {
        m_course_model = new CourseListModel(this);
        m_session_model = new SessionListModel(this);
        m_week_model = new SessionListModel(this);
        // 排布模型需要数据源；注入后每次 set_week() 都会向服务重新拉取
        m_session_model->set_service(m_service);
        m_week_model->set_service(m_service);
        // 周视图始终展示整周，因此固定不过滤星期
        m_week_model->set_day_filter(0);

        if (m_settings) {
            m_week_model->set_include_other_weeks(m_settings->show_other_weeks());
            m_session_model->set_include_other_weeks(m_settings->show_other_weeks());
        }

        m_import_export = new ImportExportBridge(m_service, m_repository, m_settings, this);

        connect_service();

        // "system" 主题跟随系统深浅色；系统切换时通知 QML 重新取色
        if (QStyleHints* hints = QGuiApplication::styleHints()) {
            connect(hints, &QStyleHints::colorSchemeChanged, this, [this](Qt::ColorScheme) {
                if (theme() == QStringLiteral("system")) {
                    emit themeChanged();
                }
            });
        }
    }

    ScheduleBridge::~ScheduleBridge() = default;

    bool ScheduleBridge::initialize() {
        bool loaded = false;

        if (m_repository && m_repository->is_open()) {
            bool found = false;
            const Semester current = m_repository->current_semester(&found);
            if (found) {
                ScheduleSnapshot snapshot;
                QString error;
                if (m_repository->load_snapshot(current.id, &snapshot, &error)) {
                    m_service->load_snapshot(snapshot);
                    loaded = true;
                    // 载入时 normalise_course 会去掉完全重复的时间段，回写一次让数据库同步修复结果
                    save_to_repository();
                    qInfo() << "[bridge] 已回写规范化后的课表";
                }
                else {
                    report_error(QStringLiteral("加载课表失败：%1").arg(error));
                }
            }
        }

        // 没有历史数据时给一个可用的默认学期，避免界面空转
        if (!m_service->has_semester()) {
            const QDate today = QDate::currentDate();
            const QDate monday = WeekCalculator::monday_of(today);
            const Semester semester = Semester::create(QStringLiteral("我的课表"), monday, 20);
            m_service->set_semester(semester);
            if (m_service->time_slots().isEmpty()) {
                m_service->set_time_slots(TimeSlot::default_slots());
            }
            if (m_repository && m_repository->is_open()) {
                ScheduleSnapshot snapshot = m_service->snapshot();
                snapshot.semester.is_current = true;
                QString error;
                if (!m_repository->save_snapshot(snapshot, &error)) {
                    report_error(QStringLiteral("初始化课表失败：%1").arg(error));
                }
                else {
                    m_repository->set_current_semester(snapshot.semester.id);
                }
            }
        }
        else if (m_service->time_slots().isEmpty()) {
            m_service->set_time_slots(TimeSlot::default_slots());
        }

        refresh_all();
        m_selected_week = current_week() > 0 ? current_week() : 1;
        if (m_session_model) {
            m_session_model->set_week(m_selected_week);
            m_session_model->set_day_filter(m_selected_day);
        }
        if (m_week_model) {
            m_week_model->set_week(m_selected_week);
        }
        emit selectedWeekChanged();
        emit selectedDayChanged();
        emit semestersChanged();
        return loaded;
    }

    void ScheduleBridge::connect_service() {
        if (!m_service) {
            return;
        }

        // 服务 → 桥接：所有刷新都在 C++ 侧显式驱动，QML 只读取属性
        connect(m_service, &ScheduleService::semester_changed, this, [this]() {
            recompute_conflicts();
            emit semesterChanged();
            emit currentWeekChanged();
            refresh_models();
        });
        connect(m_service, &ScheduleService::time_slots_changed, this, [this]() {
            recompute_conflicts();
            emit timeSlotsChanged();
            refresh_models();
        });
        connect(m_service, &ScheduleService::courses_changed, this, [this]() {
            recompute_conflicts();
            emit coursesChanged();
            refresh_models();
        });
        connect(m_service, &ScheduleService::error_occurred, this, [this](const QString& message) {
            report_error(message);
        });
    }

    // ---------------------------------------------------------------- 属性读取

    QAbstractItemModel* ScheduleBridge::course_model() const {
        return m_course_model;
    }

    QAbstractItemModel* ScheduleBridge::session_model() const {
        return m_session_model;
    }

    QAbstractItemModel* ScheduleBridge::week_model() const {
        return m_week_model;
    }

    ImportExportBridge* ScheduleBridge::import_export() const {
        return m_import_export;
    }

    bool ScheduleBridge::has_semester() const {
        return m_service && m_service->has_semester();
    }

    QString ScheduleBridge::semester_name() const {
        return m_service ? m_service->semester().name : QString();
    }

    QString ScheduleBridge::semester_start_date() const {
        if (!m_service) {
            return QString();
        }
        const Semester semester = m_service->semester();
        return semester.start_date.isValid() ? semester.start_date.toString(Qt::ISODate) : QString();
    }

    QString ScheduleBridge::semester_end_date() const {
        if (!m_service) {
            return QString();
        }
        const QDate end = m_service->semester().end_date();
        return end.isValid() ? end.toString(Qt::ISODate) : QString();
    }

    int ScheduleBridge::total_weeks() const {
        return m_service ? m_service->total_weeks() : 0;
    }

    int ScheduleBridge::current_week() const {
        return m_service ? m_service->current_week() : 0;
    }

    int ScheduleBridge::selected_week() const {
        return m_selected_week;
    }

    int ScheduleBridge::selected_day() const {
        return m_selected_day;
    }

    QString ScheduleBridge::selected_week_range() const {
        if (!m_service || m_selected_week <= 0) {
            return QString();
        }
        const QDate begin = m_service->week_start_date(m_selected_week);
        const QDate end = begin.isValid() ? begin.addDays(WeekCalculator::DAYS_PER_WEEK - 1) : QDate();
        if (!begin.isValid() || !end.isValid()) {
            return QString();
        }
        return QStringLiteral("%1 ~ %2").arg(begin.toString(Qt::ISODate), end.toString(Qt::ISODate));
    }

    QString ScheduleBridge::today_text() const {
        return QDate::currentDate().toString(Qt::ISODate);
    }

    int ScheduleBridge::course_count() const {
        return m_service ? m_service->course_count() : 0;
    }

    int ScheduleBridge::conflict_count() const {
        int count = 0;
        for (const Conflict& conflict : m_conflicts) {
            if (conflict.is_blocking()) {
                ++count;
            }
        }
        return count;
    }

    bool ScheduleBridge::has_blocking_conflicts() const {
        return ConflictDetector::has_blocking_conflict(m_conflicts);
    }

    QString ScheduleBridge::conflict_summary() const {
        if (m_conflicts.isEmpty()) {
            return QStringLiteral("未发现冲突");
        }
        return QStringLiteral("发现 %1 处问题，其中 %2 处需要处理")
            .arg(m_conflicts.size())
            .arg(conflict_count());
    }

    QVariantList ScheduleBridge::conflicts() const {
        QVariantList list;
        for (const Conflict& conflict : m_conflicts) {
            QVariantMap map;
            map.insert(QStringLiteral("type"), static_cast<int>(conflict.type));
            map.insert(QStringLiteral("typeName"), conflict.type_name());
            map.insert(QStringLiteral("message"), conflict.message);
            map.insert(QStringLiteral("dayOfWeek"), conflict.day_of_week);
            map.insert(QStringLiteral("week"), conflict.week);
            map.insert(QStringLiteral("courseIdA"), conflict.course_id_a);
            map.insert(QStringLiteral("courseIdB"), conflict.course_id_b);
            map.insert(QStringLiteral("blocking"), conflict.is_blocking());
            list.append(map);
        }
        return list;
    }

    QVariantList ScheduleBridge::time_slots() const {
        QVariantList list;
        if (!m_service) {
            return list;
        }
        for (const TimeSlot& slot : m_service->time_slots()) {
            QVariantMap map;
            map.insert(QStringLiteral("index"), slot.index);
            map.insert(QStringLiteral("label"), slot.display_label());
            map.insert(QStringLiteral("start"), WeekCalculator::format_time(slot.start_time));
            map.insert(QStringLiteral("end"), WeekCalculator::format_time(slot.end_time));
            map.insert(QStringLiteral("duration"), slot.duration_minutes());
            list.append(map);
        }
        return list;
    }

    QVariantList ScheduleBridge::week_options() const {
        QVariantList list;
        const int total = total_weeks();
        const int today_week = current_week();
        for (int week = 1; week <= total; ++week) {
            QVariantMap map;
            map.insert(QStringLiteral("value"), week);
            if (week == today_week) {
                map.insert(QStringLiteral("label"), QStringLiteral("第 %1 周（本周）").arg(week));
            }
            else {
                map.insert(QStringLiteral("label"), QStringLiteral("第 %1 周").arg(week));
            }
            list.append(map);
        }
        return list;
    }

    QVariantList ScheduleBridge::day_options() const {
        QVariantList list;
        QVariantMap all_days;
        all_days.insert(QStringLiteral("value"), 0);
        all_days.insert(QStringLiteral("label"), QStringLiteral("整周"));
        list.append(all_days);

        for (int day = 1; day <= WeekCalculator::DAYS_PER_WEEK; ++day) {
            QVariantMap map;
            map.insert(QStringLiteral("value"), day);
            map.insert(QStringLiteral("label"), WeekCalculator::day_name(day));
            list.append(map);
        }
        return list;
    }

    QString ScheduleBridge::database_path() const {
        return m_repository ? m_repository->location() : QStringLiteral("（未启用持久化）");
    }

    QString ScheduleBridge::version() const {
        return QStringLiteral("1.0.0");
    }

    QString ScheduleBridge::last_error() const {
        return m_last_error;
    }

    QString ScheduleBridge::last_info() const {
        return m_last_info;
    }

    QString ScheduleBridge::theme() const {
        return m_settings ? m_settings->theme() : QStringLiteral("system");
    }

    bool ScheduleBridge::dark_mode() const {
        const QString current = theme();
        if (current == QStringLiteral("dark")) {
            return true;
        }
        if (current == QStringLiteral("light")) {
            return false;
        }
        return QGuiApplication::styleHints() && QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
    }

    void ScheduleBridge::set_theme(const QString& theme) {
        if (!m_settings) {
            return;
        }
        QString error;
        if (!m_settings->set_theme(theme, &error)) {
            report_error(error);
            return;
        }
        emit themeChanged();
    }

    QString ScheduleBridge::course_note(const QString& course_id, int week) const {
        if (!m_repository || course_id.isEmpty() || week <= 0) {
            return QString();
        }
        return m_repository->setting(QStringLiteral("course_note/%1/%2").arg(course_id).arg(week));
    }

    void ScheduleBridge::set_course_note(const QString& course_id, int week, const QString& note) {
        if (!m_repository || course_id.isEmpty() || week <= 0) {
            return;
        }
        QString error;
        if (!m_repository->set_setting(QStringLiteral("course_note/%1/%2").arg(course_id).arg(week), note, &error)) {
            report_error(error);
            return;
        }
        emit courseNoteChanged(course_id, week);
    }

    QVariantList ScheduleBridge::semester_options() const {
        QVariantList list;
        if (!m_repository || !m_repository->is_open()) {
            return list;
        }
        const QList<Semester> semesters = m_repository->load_semesters();
        const QString current_id = (m_service && m_service->has_semester()) ? m_service->semester().id : QString();
        for (const Semester& semester : semesters) {
            QVariantMap map;
            map.insert(QStringLiteral("id"), semester.id);
            map.insert(QStringLiteral("name"), semester.name);
            map.insert(QStringLiteral("startDate"), semester.start_date.toString(Qt::ISODate));
            map.insert(QStringLiteral("totalWeeks"), semester.total_weeks);
            map.insert(QStringLiteral("isCurrent"), semester.id == current_id);
            list.append(map);
        }
        return list;
    }

    int ScheduleBridge::current_semester_index() const {
        const QVariantList list = semester_options();
        for (int index = 0; index < list.size(); ++index) {
            if (list.at(index).toMap().value(QStringLiteral("isCurrent")).toBool()) {
                return index;
            }
        }
        return -1;
    }

    void ScheduleBridge::switch_semester(const QString& semester_id) {
        if (!m_repository || !m_repository->is_open() || semester_id.isEmpty()) {
            return;
        }
        if (m_service && m_service->has_semester() && m_service->semester().id == semester_id) {
            return;
        }

        save_to_repository();

        ScheduleSnapshot snapshot;
        QString error;
        if (!m_repository->load_snapshot(semester_id, &snapshot, &error)) {
            report_error(QStringLiteral("切换学期失败：%1").arg(error));
            return;
        }
        m_service->load_snapshot(snapshot);
        m_repository->set_current_semester(semester_id);
        if (m_settings) {
            m_settings->set_current_semester_id(semester_id);
        }

        m_selected_week = current_week() > 0 ? current_week() : 1;
        if (m_session_model) {
            m_session_model->set_week(m_selected_week);
            m_session_model->set_day_filter(m_selected_day);
        }
        if (m_week_model) {
            m_week_model->set_week(m_selected_week);
        }
        refresh_all();
        emit selectedWeekChanged();
        emit semestersChanged();
        report_info(QStringLiteral("已切换到“%1”").arg(snapshot.semester.name));
    }

    bool ScheduleBridge::backup_database(const QString& path) {
        if (!m_repository || !m_repository->is_open()) {
            report_error(QStringLiteral("数据库尚未打开，无法备份"));
            return false;
        }
        QString error;
        if (!m_repository->backup_to(path, &error)) {
            report_error(QStringLiteral("备份数据库失败：%1").arg(error));
            qWarning() << "[db] 备份失败" << path << error;
            return false;
        }
        qInfo() << "[db] 数据库已备份" << path;
        report_info(QStringLiteral("数据库已备份：%1").arg(path));
        return true;
    }

    bool ScheduleBridge::restore_database(const QString& path) {
        if (!m_repository || !m_repository->is_open()) {
            report_error(QStringLiteral("数据库尚未打开，无法恢复"));
            return false;
        }
        QString error;
        if (!m_repository->restore_from(path, &error)) {
            report_error(QStringLiteral("恢复数据库失败：%1").arg(error));
            qWarning() << "[db] 恢复失败" << path << error;
            return false;
        }
        reload_from_repository();
        emit themeChanged();
        emit displaySettingsChanged();
        emit logSettingsChanged();
        qInfo() << "[db] 数据库已恢复" << path;
        report_info(QStringLiteral("数据库已恢复：%1").arg(path));
        return true;
    }

    void ScheduleBridge::post_info(const QString& message) {
        qInfo().noquote() << "[ui]" << message;
        report_info(message);
    }

    void ScheduleBridge::post_error(const QString& message) {
        qWarning().noquote() << "[ui]" << message;
        report_error(message);
    }

    void ScheduleBridge::remove_semester(const QString& semester_id) {
        if (!m_repository || !m_repository->is_open() || semester_id.isEmpty()) {
            return;
        }
        const QList<Semester> semesters = m_repository->load_semesters();
        if (semesters.size() <= 1) {
            report_error(QStringLiteral("至少保留一个学期"));
            return;
        }
        const bool was_current = m_service && m_service->has_semester() && m_service->semester().id == semester_id;
        if (was_current) {
            QString other_id;
            for (const Semester& semester : semesters) {
                if (semester.id != semester_id) {
                    other_id = semester.id;
                    break;
                }
            }
            if (other_id.isEmpty()) {
                return;
            }
            switch_semester(other_id);
        }
        QString error;
        if (!m_repository->remove_semester(semester_id, &error)) {
            report_error(QStringLiteral("删除学期失败：%1").arg(error));
            return;
        }
        emit semestersChanged();
        report_info(QStringLiteral("学期已删除"));
    }

    // ---------------------------------------------------------------- 显示设置

    bool ScheduleBridge::show_weekend() const {
        return !m_settings || m_settings->show_weekend();
    }

    void ScheduleBridge::set_show_weekend(bool enabled) {
        if (!m_settings) {
            return;
        }
        QString error;
        if (!m_settings->set_show_weekend(enabled, &error)) {
            report_error(error);
            return;
        }
        emit displaySettingsChanged();
    }

    int ScheduleBridge::slot_density() const {
        return m_settings ? m_settings->slot_density() : 1;
    }

    void ScheduleBridge::set_slot_density(int density) {
        if (!m_settings) {
            return;
        }
        QString error;
        if (!m_settings->set_slot_density(density, &error)) {
            report_error(error);
            return;
        }
        emit displaySettingsChanged();
    }

    int ScheduleBridge::card_radius() const {
        return m_settings ? m_settings->card_radius() : 1;
    }

    bool ScheduleBridge::show_other_weeks() const {
        return m_settings && m_settings->show_other_weeks();
    }

    void ScheduleBridge::set_show_other_weeks(bool enabled) {
        if (!m_settings) {
            return;
        }
        QString error;
        if (!m_settings->set_show_other_weeks(enabled, &error)) {
            report_error(error);
            return;
        }
        if (m_week_model) {
            m_week_model->set_include_other_weeks(enabled);
        }
        if (m_session_model) {
            m_session_model->set_include_other_weeks(enabled);
        }
        emit displaySettingsChanged();
    }

    int ScheduleBridge::font_scale_index() const {
        return m_settings ? m_settings->font_scale() : 1;
    }

    qreal ScheduleBridge::font_scale() const {
        switch (font_scale_index()) {
        case 0:
            return 0.9;
        case 2:
            return 1.15;
        case 3:
            return 1.3;
        default:
            return 1.0;
        }
    }

    void ScheduleBridge::set_font_scale_index(int index) {
        if (!m_settings) {
            return;
        }
        QString error;
        if (!m_settings->set_font_scale(index, &error)) {
            report_error(error);
            return;
        }
        emit displaySettingsChanged();
    }

    int ScheduleBridge::log_level() const {
        return m_settings ? m_settings->log_level() : 0;
    }

    void ScheduleBridge::set_log_level(int level) {
        if (!m_settings) {
            return;
        }
        QString error;
        if (!m_settings->set_log_level(level, &error)) {
            report_error(error);
            return;
        }
        emit logSettingsChanged();
    }

    int ScheduleBridge::log_retention_days() const {
        return m_settings ? m_settings->log_retention_days() : 7;
    }

    void ScheduleBridge::set_log_retention_days(int days) {
        if (!m_settings) {
            return;
        }
        QString error;
        if (!m_settings->set_log_retention_days(days, &error)) {
            report_error(error);
            return;
        }
        emit logSettingsChanged();
    }

    void ScheduleBridge::set_card_radius(int radius) {
        if (!m_settings) {
            return;
        }
        QString error;
        if (!m_settings->set_card_radius(radius, &error)) {
            report_error(error);
            return;
        }
        emit displaySettingsChanged();
    }

    QString ScheduleBridge::day_name(int day_of_week) const {
        return WeekCalculator::day_name(day_of_week);
    }

    QString ScheduleBridge::week_date_text(int week, int day_of_week) const {
        if (!m_service || !m_service->has_semester()) {
            return QString();
        }
        const QDate date = WeekCalculator::date_of(m_service->semester(), week, day_of_week);
        return date.isValid() ? date.toString(QStringLiteral("MM-dd")) : QString();
    }

    QString ScheduleBridge::format_time(int hour, int minute) const {
        return WeekCalculator::format_time(QTime(hour, minute));
    }

    // -------------------------------------------------------------------- 内部

    ScheduleSnapshot ScheduleBridge::snapshot() const {
        return m_service ? m_service->snapshot() : ScheduleSnapshot();
    }

    void ScheduleBridge::report_error(const QString& message) {
        m_last_error = message;
        emit errorOccurred(message);
    }

    void ScheduleBridge::report_info(const QString& message) {
        m_last_info = message;
        emit infoMessage(message);
    }

    void ScheduleBridge::refresh_models() {
        if (m_course_model && m_service) {
            m_course_model->set_courses(m_service->courses());
        }
        if (m_session_model) {
            m_session_model->refresh();
        }
        if (m_week_model) {
            m_week_model->refresh();
        }
    }

    void ScheduleBridge::recompute_conflicts() {
        m_conflicts = m_service ? m_service->detect_conflicts() : QList<Conflict>();
        if (m_conflicts.isEmpty()) {
            qInfo() << "[conflicts] 未发现冲突";
        }
        else {
            qInfo().noquote() << "[conflicts] 共" << m_conflicts.size() << "处问题";
            for (const Conflict& conflict : m_conflicts) {
                qInfo().noquote() << "[conflicts]  -" << conflict.message;
            }
        }
        emit conflictsChanged();
    }

    void ScheduleBridge::refresh_all() {
        refresh_models();
        recompute_conflicts();
        update_android_widget();
        emit semesterChanged();
        emit timeSlotsChanged();
        emit coursesChanged();
        emit currentWeekChanged();
    }

    void ScheduleBridge::update_android_widget() {
#if defined(Q_OS_ANDROID)
        if (!m_service || !m_service->has_semester()) {
            return;
        }
        const int week = current_week();
        QJsonArray array;
        if (week > 0) {
            const int day = QDate::currentDate().dayOfWeek();
            const QList<ScheduleService::PlacedSession> sessions = m_service->sessions_at(day, week);
            for (const ScheduleService::PlacedSession& placed : sessions) {
                QJsonObject item;
                QString line = QStringLiteral("%1-%2  %3").arg(
                    WeekCalculator::format_time(placed.start_time()),
                    WeekCalculator::format_time(placed.end_time()),
                    placed.course_name);
                if (!placed.location.isEmpty()) {
                    line += QStringLiteral("  ·  ") + placed.location;
                }
                item.insert(QStringLiteral("line"), line);
                array.append(item);
            }
        }
        const QByteArray json = QJsonDocument(array).toJson(QJsonDocument::Compact);

        // 小组件不读数据库：主程序把今日课程写成 JSON，放在应用私有 files 目录，
        // 再用一条应用内广播通知桌面小组件刷新；这样 Java 侧不需要理解周次/学期。
        const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir().mkpath(dir);
        const QString path = dir + QStringLiteral("/widget_today.json");
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            qWarning().noquote() << "[widget] 无法写入" << path;
            return;
        }
        file.write(json);
        file.close();

        QJniObject activity = QJniObject::callStaticObjectMethod(
            "org/qtproject/qt/android/QtNative", "activity", "()Landroid/app/Activity;");
        if (!activity.isValid()) {
            qWarning() << "[widget] 无法取得 Activity";
            return;
        }
        const QString package = activity.callObjectMethod(
            "getPackageName", "()Ljava/lang/String;").toString();
        QJniObject intent("android/content/Intent", "(Ljava/lang/String;)V",
            QJniObject::fromString(QStringLiteral("org.qtproject.example.Schedule.WIDGET_REFRESH")).object());
        intent.callObjectMethod("setPackage", "(Ljava/lang/String;)Landroid/content/Intent;",
            QJniObject::fromString(package).object());
        activity.callMethod<void>("sendBroadcast", "(Landroid/content/Intent;)V", intent.object());
        qInfo().noquote() << "[widget] 已写入" << path << "并广播刷新";
#endif
    }

    // -------------------------------------------------------------------- 槽

    void ScheduleBridge::reload_from_repository() {
        if (!m_repository || !m_repository->is_open()) {
            report_error(QStringLiteral("尚未配置持久化存储"));
            return;
        }

        bool found = false;
        const Semester current = m_repository->current_semester(&found);
        if (!found) {
            report_error(QStringLiteral("没有找到当前学期"));
            return;
        }

        ScheduleSnapshot loaded;
        QString error;
        if (!m_repository->load_snapshot(current.id, &loaded, &error)) {
            report_error(QStringLiteral("重新加载失败：%1").arg(error));
            return;
        }

        m_service->load_snapshot(loaded);
        m_selected_week = current_week() > 0 ? current_week() : 1;
        if (m_session_model) {
            m_session_model->set_week(m_selected_week);
        }
        if (m_week_model) {
            m_week_model->set_week(m_selected_week);
        }
        emit selectedWeekChanged();
        report_info(QStringLiteral("已从数据库重新加载"));
    }

    void ScheduleBridge::save_to_repository() {
        if (!m_repository || !m_repository->is_open()) {
            report_error(QStringLiteral("尚未配置持久化存储"));
            return;
        }
        QString error;
        if (!m_repository->save_snapshot(snapshot(), &error)) {
            report_error(QStringLiteral("保存失败：%1").arg(error));
            return;
        }
        report_info(QStringLiteral("已保存到数据库"));
    }

    void ScheduleBridge::create_semester(const QString& name, const QString& start_date, int total_weeks) {
        const QDate date = QDate::fromString(start_date.trimmed(), Qt::ISODate);
        if (!date.isValid()) {
            report_error(QStringLiteral("学期起始日期无效，应为 yyyy-MM-dd"));
            return;
        }

        Semester semester = Semester::create(name.trimmed(), date, total_weeks);
        semester.is_current = true;
        QString error;
        if (!m_service->set_semester(semester)) {
            return; // 服务已发出 error_occurred
        }
        m_service->set_time_slots(TimeSlot::default_slots());

        if (m_repository && m_repository->is_open()) {
            ScheduleSnapshot fresh = m_service->snapshot();
            fresh.semester.is_current = true;
            if (!m_repository->save_snapshot(fresh, &error)) {
                report_error(QStringLiteral("学期已创建，但保存失败：%1").arg(error));
                return;
            }
            m_repository->set_current_semester(fresh.semester.id);
        }

        m_selected_week = 1;
        if (m_session_model) {
            m_session_model->set_week(m_selected_week);
        }
        if (m_week_model) {
            m_week_model->set_week(m_selected_week);
        }
        emit selectedWeekChanged();
        report_info(QStringLiteral("已创建学期“%1”").arg(semester.name));
        emit semestersChanged();
    }

    void ScheduleBridge::update_semester(const QString& name, const QString& start_date, int total_weeks) {
        if (!m_service->has_semester()) {
            report_error(QStringLiteral("尚未设置学期"));
            return;
        }

        const QDate date = QDate::fromString(start_date.trimmed(), Qt::ISODate);
        if (!date.isValid()) {
            report_error(QStringLiteral("学期起始日期无效，应为 yyyy-MM-dd"));
            return;
        }

        Semester semester = m_service->semester();
        semester.name = name.trimmed();
        semester.start_date = date;
        semester.total_weeks = qBound(1, total_weeks, WeekMask::MAX_WEEKS);
        semester.updated_at = QDateTime::currentDateTime();

        if (!m_service->set_semester(semester)) {
            return;
        }
        save_to_repository();

        // 周数可能变短，把越界的当前周夹回合法范围
        if (m_selected_week > semester.total_weeks) {
            m_selected_week = semester.total_weeks;
            if (m_session_model) {
                m_session_model->set_week(m_selected_week);
            }
            emit selectedWeekChanged();
        }
        report_info(QStringLiteral("学期信息已更新"));
        emit semestersChanged();
    }

    void ScheduleBridge::reset_time_slots_to_default() {
        m_service->set_time_slots(TimeSlot::default_slots());
        save_to_repository();
        report_info(QStringLiteral("已恢复默认作息时间"));
    }

    void ScheduleBridge::save_time_slot(int index, const QString& label, const QString& start, const QString& end) {
        if (index < 1) {
            report_error(QStringLiteral("节次序号必须大于 0"));
            return;
        }

        const QTime start_time = WeekCalculator::parse_time(start);
        const QTime end_time = WeekCalculator::parse_time(end);
        if (!start_time.isValid() || !end_time.isValid()) {
            report_error(QStringLiteral("时间格式无效，应为 HH:mm"));
            return;
        }
        if (end_time <= start_time) {
            report_error(QStringLiteral("结束时间必须晚于开始时间"));
            return;
        }

        // 注意：局部变量不能命名为 slots —— Qt 把 slots 定义成了空宏
        QList<TimeSlot> periods = m_service->time_slots();
        bool replaced = false;
        for (TimeSlot& slot : periods) {
            if (slot.index == index) {
                slot.label = label.trimmed().isEmpty() ? QStringLiteral("第 %1 节").arg(index) : label.trimmed();
                slot.start_time = start_time;
                slot.end_time = end_time;
                replaced = true;
                break;
            }
        }
        if (!replaced) {
            TimeSlot slot;
            slot.index = index;
            slot.label = label.trimmed().isEmpty() ? QStringLiteral("第 %1 节").arg(index) : label.trimmed();
            slot.start_time = start_time;
            slot.end_time = end_time;
            periods.append(slot);
        }

        m_service->set_time_slots(periods);
        save_to_repository();
        report_info(QStringLiteral("第 %1 节的作息时间已保存").arg(index));
    }

    void ScheduleBridge::select_week(int week) {
        const int total = total_weeks();
        const int clamped = qBound(1, week, qMax(1, total));
        if (m_selected_week == clamped) {
            return;
        }
        m_selected_week = clamped;
        if (m_session_model) {
            m_session_model->set_week(m_selected_week);
        }
        if (m_week_model) {
            m_week_model->set_week(m_selected_week);
        }
        if (m_week_model) {
            m_week_model->set_week(m_selected_week);
        }
        emit selectedWeekChanged();
    }

    void ScheduleBridge::go_to_current_week() {
        const int week = current_week();
        if (week <= 0) {
            report_info(QStringLiteral("今天不在本学期范围内"));
            return;
        }
        select_week(week);
    }

    void ScheduleBridge::previous_week() {
        select_week(m_selected_week - 1);
    }

    void ScheduleBridge::next_week() {
        select_week(m_selected_week + 1);
    }

    void ScheduleBridge::select_day(int day_of_week) {
        const int normalised = (day_of_week >= 1 && day_of_week <= WeekCalculator::DAYS_PER_WEEK) ? day_of_week : 0;
        if (m_selected_day == normalised) {
            return;
        }
        m_selected_day = normalised;
        if (m_session_model) {
            m_session_model->set_day_filter(m_selected_day);
        }
        emit selectedDayChanged();
    }

    bool ScheduleBridge::save_course(const QVariantMap& data) {
        if (!m_service->has_semester()) {
            report_error(QStringLiteral("尚未设置学期，无法保存课程"));
            return false;
        }

        Course course;
        course.id = data.value(QStringLiteral("courseId")).toString();
        course.name = data.value(QStringLiteral("name")).toString().trimmed();
        course.code = data.value(QStringLiteral("code")).toString().trimmed();
        course.teacher = data.value(QStringLiteral("teacher")).toString().trimmed();
        course.location = data.value(QStringLiteral("location")).toString().trimmed();
        course.color = data.value(QStringLiteral("color")).toString().trimmed();
        course.notes = data.value(QStringLiteral("notes")).toString();
        course.semester_id = m_service->semester().id;

        bool credits_ok = false;
        const double credits = data.value(QStringLiteral("credits")).toDouble(&credits_ok);
        course.credits = credits_ok ? credits : 0.0;

        if (course.name.isEmpty()) {
            report_error(QStringLiteral("课程名称不能为空"));
            return false;
        }

        const QVariantList sessions = data.value(QStringLiteral("sessions")).toList();
        const Semester semester = m_service->semester();
        qInfo().noquote() << "[bridge] 保存课程" << course.name << "，时间段" << sessions.size();
        for (const QVariant& entry : sessions) {
            CourseSession session;
            QString error;
            if (!session_from_map(entry.toMap(), semester, &error, &session)) {
                qWarning().noquote() << "[bridge] 时间段不合法：" << error << "原始数据" << entry;
                report_error(QStringLiteral("课程“%1”的上课时间不合法：%2").arg(course.name, error));
                return false;
            }
            qInfo().noquote() << "[bridge]   时间段 周" << session.day_of_week << "第" << session.start_slot << "-" << session.end_slot() << "周次" << session.weeks.to_expression();
            course.sessions.append(session);
        }

        if (course.sessions.isEmpty()) {
            report_error(QStringLiteral("请至少添加一个上课时间段"));
            return false;
        }

        const bool is_update = !course.id.isEmpty() && m_service->index_of_course(course.id) >= 0;

        QString error;
        const bool ok = is_update ? m_service->update_course(course, &error) : m_service->add_course(course, &error);
        if (!ok) {
            qWarning().noquote() << "[bridge] 课程写入失败：" << error;
            if (!error.isEmpty()) {
                report_error(error);
            }
            return false;
        }

        qInfo().noquote() << "[bridge] 课程已写入" << (is_update ? "更新" : "新增") << course.name;
        save_to_repository();
        report_info(is_update ? QStringLiteral("课程“%1”已更新").arg(course.name)
                              : QStringLiteral("课程“%1”已添加").arg(course.name));
        return true;
    }

    void ScheduleBridge::remove_course(const QString& course_id) {
        if (!m_service->remove_course(course_id)) {
            report_error(QStringLiteral("未找到要删除的课程"));
            return;
        }
        save_to_repository();
        report_info(QStringLiteral("课程已删除"));
    }

    void ScheduleBridge::refresh_conflicts() {
        recompute_conflicts();
    }

} // namespace Schedule
