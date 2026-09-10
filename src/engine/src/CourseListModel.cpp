#include "engine/CourseListModel.h"

#include "core/service/WeekCalculator.h"

#include <QStringList>

namespace Schedule {

    namespace {

        /** @return 课程时间段的摘要文本，如“周一 第 1-2 节；周三 第 5-6 节”。 */
        QString day_summary(const Course& course) {
            QStringList parts;
            for (const CourseSession& session : course.sessions) {
                const QString day = WeekCalculator::day_name(session.day_of_week);
                if (session.start_slot == session.end_slot()) {
                    parts.append(QStringLiteral("%1 第 %2 节").arg(day).arg(session.start_slot));
                }
                else {
                    parts.append(QStringLiteral("%1 第 %2-%3 节").arg(day).arg(session.start_slot).arg(session.end_slot()));
                }
            }
            return parts.join(QStringLiteral("；"));
        }

    } // namespace

    CourseListModel::CourseListModel(QObject* parent)
        : QAbstractListModel(parent) {
    }

    CourseListModel::~CourseListModel() = default;

    int CourseListModel::rowCount(const QModelIndex& parent) const {
        if (parent.isValid()) {
            return 0;
        }
        return static_cast<int>(m_courses.size());
    }

    QVariant CourseListModel::data(const QModelIndex& index, int role) const {
        if (!index.isValid() || index.row() < 0 || index.row() >= m_courses.size()) {
            return QVariant();
        }
        const Course& course = m_courses.at(index.row());

        switch (role) {
        case Qt::DisplayRole:
        case NameRole:
            return course.name;
        case CourseIdRole:
            return course.id;
        case CodeRole:
            return course.code;
        case TeacherRole:
            return course.teacher;
        case LocationRole:
            return course.location;
        case ColorRole:
            return course.display_color();
        case CreditsRole:
            return course.credits;
        case NotesRole:
            return course.notes;
        case SessionCountRole:
            return static_cast<int>(course.sessions.size());
        case WeekExpressionRole:
            return course.total_weeks().to_expression();
        case WeekDisplayRole: {
            const WeekMask weeks = course.total_weeks();
            return weeks.is_empty() ? QStringLiteral("未设置周次") : weeks.to_display_string();
        }
        case DaySummaryRole:
            return day_summary(course);
        default:
            return QVariant();
        }
    }

    QHash<int, QByteArray> CourseListModel::roleNames() const {
        return QHash<int, QByteArray>{
            {CourseIdRole, "courseId"},
            {NameRole, "name"},
            {CodeRole, "code"},
            {TeacherRole, "teacher"},
            {LocationRole, "location"},
            {ColorRole, "color"},
            {CreditsRole, "credits"},
            {NotesRole, "notes"},
            {SessionCountRole, "sessionCount"},
            {WeekExpressionRole, "weekExpression"},
            {WeekDisplayRole, "weekDisplay"},
            {DaySummaryRole, "daySummary"},
        };
    }

    void CourseListModel::set_courses(const QList<Course>& courses) {
        beginResetModel();
        m_courses = courses;
        endResetModel();
    }

    QList<Course> CourseListModel::courses() const {
        return m_courses;
    }

    int CourseListModel::index_of_course(const QString& course_id) const {
        for (int row = 0; row < m_courses.size(); ++row) {
            if (m_courses.at(row).id == course_id) {
                return row;
            }
        }
        return -1;
    }

    QVariantMap CourseListModel::to_map(const Course& course) {
        QVariantMap map;
        map.insert(QStringLiteral("courseId"), course.id);
        map.insert(QStringLiteral("name"), course.name);
        map.insert(QStringLiteral("code"), course.code);
        map.insert(QStringLiteral("teacher"), course.teacher);
        map.insert(QStringLiteral("location"), course.location);
        map.insert(QStringLiteral("color"), course.display_color());
        map.insert(QStringLiteral("credits"), course.credits);
        map.insert(QStringLiteral("notes"), course.notes);

        QVariantList sessions;
        for (const CourseSession& session : course.sessions) {
            QVariantMap entry;
            entry.insert(QStringLiteral("sessionId"), session.id);
            entry.insert(QStringLiteral("dayOfWeek"), session.day_of_week);
            entry.insert(QStringLiteral("dayName"), WeekCalculator::day_name(session.day_of_week));
            entry.insert(QStringLiteral("startSlot"), session.start_slot);
            entry.insert(QStringLiteral("slotCount"), session.slot_count);
            entry.insert(QStringLiteral("endSlot"), session.end_slot());
            entry.insert(QStringLiteral("weeks"), session.weeks.to_expression());
            entry.insert(QStringLiteral("weeksDisplay"), session.weeks.to_display_string());
            entry.insert(QStringLiteral("location"), session.location);
            entry.insert(QStringLiteral("teacher"), session.teacher);
            sessions.append(entry);
        }
        map.insert(QStringLiteral("sessions"), sessions);
        map.insert(QStringLiteral("sessionCount"), static_cast<int>(course.sessions.size()));
        map.insert(QStringLiteral("weekExpression"), course.total_weeks().to_expression());
        map.insert(QStringLiteral("weekDisplay"), course.total_weeks().to_display_string());
        map.insert(QStringLiteral("daySummary"), day_summary(course));
        return map;
    }

    QVariantMap CourseListModel::get(int row) const {
        if (row < 0 || row >= m_courses.size()) {
            return QVariantMap();
        }
        return to_map(m_courses.at(row));
    }

    QVariantMap CourseListModel::find(const QString& course_id) const {
        const int row = index_of_course(course_id);
        return row < 0 ? QVariantMap() : to_map(m_courses.at(row));
    }

    int CourseListModel::index_of(const QString& course_id) const {
        return index_of_course(course_id);
    }

} // namespace Schedule
