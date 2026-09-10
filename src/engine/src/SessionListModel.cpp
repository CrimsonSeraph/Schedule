#include "engine/SessionListModel.h"

#include "core/service/WeekCalculator.h"

namespace Schedule {

    SessionListModel::SessionListModel(QObject* parent)
        : QAbstractListModel(parent) {
    }

    SessionListModel::~SessionListModel() = default;

    int SessionListModel::rowCount(const QModelIndex& parent) const {
        if (parent.isValid()) {
            return 0;
        }
        return static_cast<int>(m_sessions.size());
    }

    QVariant SessionListModel::data(const QModelIndex& index, int role) const {
        if (!index.isValid() || index.row() < 0 || index.row() >= m_sessions.size()) {
            return QVariant();
        }
        const ScheduleService::PlacedSession& placed = m_sessions.at(index.row());

        switch (role) {
        case Qt::DisplayRole:
        case CourseNameRole:
            return placed.course_name;
        case CourseIdRole:
            return placed.course_id;
        case SessionIdRole:
            return placed.session.id;
        case DayOfWeekRole:
            return placed.session.day_of_week;
        case DayNameRole:
            return WeekCalculator::day_name(placed.session.day_of_week);
        case StartSlotRole:
            return placed.session.start_slot;
        case EndSlotRole:
            return placed.session.end_slot();
        case SlotCountRole:
        case RowSpanRole:
            return placed.session.slot_count;
        case WeeksRole:
            return placed.session.weeks.to_expression();
        case WeeksDisplayRole:
            return placed.session.weeks.to_display_string();
        case TeacherRole:
            return placed.teacher;
        case LocationRole:
            return placed.location;
        case ColorRole:
            return placed.color;
        case StartTimeRole:
            return WeekCalculator::format_time(placed.start_time());
        case EndTimeRole:
            return WeekCalculator::format_time(placed.end_time());
        case DateRole:
            return placed.date.isValid() ? placed.date.toString(Qt::ISODate) : QString();
        case WeekRole:
            return placed.week;
        default:
            return QVariant();
        }
    }

    QHash<int, QByteArray> SessionListModel::roleNames() const {
        return QHash<int, QByteArray>{
            {CourseIdRole, "courseId"},
            {CourseNameRole, "courseName"},
            {SessionIdRole, "sessionId"},
            {DayOfWeekRole, "dayOfWeek"},
            {DayNameRole, "dayName"},
            {StartSlotRole, "startSlot"},
            {EndSlotRole, "endSlot"},
            {SlotCountRole, "slotCount"},
            {WeeksRole, "weeks"},
            {WeeksDisplayRole, "weeksDisplay"},
            {TeacherRole, "teacher"},
            {LocationRole, "location"},
            {ColorRole, "color"},
            {StartTimeRole, "startTime"},
            {EndTimeRole, "endTime"},
            {DateRole, "date"},
            {WeekRole, "week"},
            {RowSpanRole, "rowSpan"},
        };
    }

    int SessionListModel::week() const {
        return m_week;
    }

    void SessionListModel::set_week(int week) {
        if (m_week == week) {
            return;
        }
        m_week = week;
        reload();
    }

    int SessionListModel::day_filter() const {
        return m_day_filter;
    }

    void SessionListModel::set_day_filter(int day_of_week) {
        const int normalised = (day_of_week >= 1 && day_of_week <= WeekCalculator::DAYS_PER_WEEK) ? day_of_week : 0;
        if (m_day_filter == normalised) {
            return;
        }
        m_day_filter = normalised;
        reload();
    }

    void SessionListModel::set_service(ScheduleService* service) {
        m_service = service;
        reload();
    }

    void SessionListModel::refresh() {
        reload();
    }

    int SessionListModel::count() const {
        return static_cast<int>(m_sessions.size());
    }

    void SessionListModel::reload() {
        QList<ScheduleService::PlacedSession> sessions;
        if (m_service && m_week > 0) {
            if (m_day_filter == 0) {
                sessions = m_service->sessions_in_week(m_week);
            }
            else {
                sessions = m_service->sessions_at(m_day_filter, m_week);
            }
        }

        beginResetModel();
        m_sessions = sessions;
        endResetModel();
    }

    QVariantMap SessionListModel::get(int row) const {
        if (row < 0 || row >= m_sessions.size()) {
            return QVariantMap();
        }
        QVariantMap map;
        map.insert(QStringLiteral("courseId"), data(index(row), CourseIdRole));
        map.insert(QStringLiteral("courseName"), data(index(row), CourseNameRole));
        map.insert(QStringLiteral("sessionId"), data(index(row), SessionIdRole));
        map.insert(QStringLiteral("dayOfWeek"), data(index(row), DayOfWeekRole));
        map.insert(QStringLiteral("dayName"), data(index(row), DayNameRole));
        map.insert(QStringLiteral("startSlot"), data(index(row), StartSlotRole));
        map.insert(QStringLiteral("endSlot"), data(index(row), EndSlotRole));
        map.insert(QStringLiteral("slotCount"), data(index(row), SlotCountRole));
        map.insert(QStringLiteral("weeks"), data(index(row), WeeksRole));
        map.insert(QStringLiteral("teacher"), data(index(row), TeacherRole));
        map.insert(QStringLiteral("location"), data(index(row), LocationRole));
        map.insert(QStringLiteral("color"), data(index(row), ColorRole));
        map.insert(QStringLiteral("startTime"), data(index(row), StartTimeRole));
        map.insert(QStringLiteral("endTime"), data(index(row), EndTimeRole));
        map.insert(QStringLiteral("date"), data(index(row), DateRole));
        return map;
    }

} // namespace Schedule
