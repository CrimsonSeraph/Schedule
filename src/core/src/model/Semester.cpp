#include "core/model/Semester.h"

#include "core/model/WeekMask.h"

#include <QUuid>

namespace Schedule {

    namespace {

        /** 一周的天数：与 WeekCalculator::DAYS_PER_WEEK 保持一致。 */
        constexpr int DAYS_PER_WEEK = 7;

        /** @return 日期所在自然周的周一；无效日期返回无效 QDate。 */
        QDate monday_of(const QDate& date) {
            if (!date.isValid()) {
                return QDate();
            }
            return date.addDays(-(date.dayOfWeek() - 1));
        }

    } // namespace

    bool Semester::is_valid(QString* error_message) const {
        const auto fail = [error_message](const QString& message) {
            if (error_message) {
                *error_message = message;
            }
            return false;
        };

        if (name.trimmed().isEmpty()) {
            return fail(QStringLiteral("学期名称不能为空"));
        }
        if (!start_date.isValid()) {
            return fail(QStringLiteral("学期起始日期无效"));
        }
        if (total_weeks < 1 || total_weeks > WeekMask::MAX_WEEKS) {
            return fail(QStringLiteral("学期总周数必须在 1 到 %1 之间，当前为 %2")
                    .arg(WeekMask::MAX_WEEKS)
                    .arg(total_weeks));
        }
        if (error_message) {
            error_message->clear();
        }
        return true;
    }

    QDate Semester::week_start_date(int week) const {
        if (week < 1 || week > total_weeks || !start_date.isValid()) {
            return QDate();
        }
        // 周次按**自然周（周一为第 1 天）**对齐：第 1 周是包含 start_date 的那个自然周。
        // 这样即使学期起始日不是周一（例如导入数据从周三开始），周内布局依然稳定。
        return monday_of(start_date).addDays(DAYS_PER_WEEK * (week - 1));
    }

    QDate Semester::week_end_date(int week) const {
        const QDate begin = week_start_date(week);
        return begin.isValid() ? begin.addDays(DAYS_PER_WEEK - 1) : QDate();
    }

    QDate Semester::end_date() const {
        return week_end_date(total_weeks);
    }

    int Semester::week_of(const QDate& date) const {
        if (!date.isValid() || !start_date.isValid()) {
            return 0;
        }
        const QDate anchor = monday_of(start_date);
        const qint64 offset = anchor.daysTo(monday_of(date));
        if (offset < 0) {
            // 位于第 1 周之前（含上一周）的日期不属于本学期。
            return 0;
        }
        const int week = static_cast<int>(offset / DAYS_PER_WEEK) + 1;
        return (week >= 1 && week <= total_weeks) ? week : 0;
    }

    bool Semester::contains(const QDate& date) const {
        return week_of(date) > 0;
    }

    bool Semester::is_active_on(const QDate& today) const {
        return contains(today);
    }

    Semester Semester::create(const QString& name, const QDate& start_date, int total_weeks) {
        Semester semester;
        semester.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        semester.name = name;
        semester.start_date = start_date;
        semester.total_weeks = qBound(1, total_weeks, WeekMask::MAX_WEEKS);
        semester.is_current = false;
        semester.created_at = QDateTime::currentDateTime();
        semester.updated_at = semester.created_at;
        return semester;
    }

    bool Semester::operator==(const Semester& other) const {
        return id == other.id && name == other.name && start_date == other.start_date && total_weeks == other.total_weeks && is_current == other.is_current;
    }

    bool Semester::operator!=(const Semester& other) const {
        return !(*this == other);
    }

} // namespace Schedule
