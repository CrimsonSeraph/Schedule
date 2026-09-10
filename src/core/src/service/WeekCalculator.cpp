#include "core/service/WeekCalculator.h"

#include <QStringList>

namespace Schedule {

    namespace {

        const QStringList DAY_NAMES = {
            QString(),
            QStringLiteral("周一"),
            QStringLiteral("周二"),
            QStringLiteral("周三"),
            QStringLiteral("周四"),
            QStringLiteral("周五"),
            QStringLiteral("周六"),
            QStringLiteral("周日"),
        };

        const QStringList SHORT_DAY_NAMES = {
            QString(),
            QStringLiteral("一"),
            QStringLiteral("二"),
            QStringLiteral("三"),
            QStringLiteral("四"),
            QStringLiteral("五"),
            QStringLiteral("六"),
            QStringLiteral("日"),
        };

        /** @return 时间文本的首尾去空白与全角冒号规整结果。 */
        QString normalise_time_text(const QString& text) {
            QString result = text.trimmed();
            result.replace(QChar(0xFF1A), QLatin1Char(':')); // 全角冒号 -> 半角
            return result;
        }

    } // namespace

    const QStringList& WeekCalculator::day_names() {
        return DAY_NAMES;
    }

    int WeekCalculator::week_of(const Semester& semester, const QDate& date) {
        return semester.week_of(date);
    }

    int WeekCalculator::current_week(const Semester& semester, const QDate& today) {
        return semester.week_of(today);
    }

    QDate WeekCalculator::week_start_date(const Semester& semester, int week) {
        return semester.week_start_date(week);
    }

    QDate WeekCalculator::week_end_date(const Semester& semester, int week) {
        return semester.week_end_date(week);
    }

    QDate WeekCalculator::date_of(const Semester& semester, int week, int day_of_week) {
        const QDate start = semester.week_start_date(week);
        if (!start.isValid() || day_of_week < 1 || day_of_week > DAYS_PER_WEEK) {
            return QDate();
        }
        // 星期取值与 QDate::dayOfWeek() 一致：1=周一。`week_start_date()` 返回的是
        // 该自然周的周一，因此 day_of_week=1 恰好落在周首，day_of_week=7 落在周日。
        return start.addDays(day_of_week - 1);
    }

    QDate WeekCalculator::monday_of(const QDate& date) {
        if (!date.isValid()) {
            return QDate();
        }
        return date.addDays(-(date.dayOfWeek() - 1));
    }

    QString WeekCalculator::day_name(int day_of_week) {
        if (day_of_week < 1 || day_of_week > DAYS_PER_WEEK) {
            return QString();
        }
        return DAY_NAMES.at(day_of_week);
    }

    QString WeekCalculator::short_day_name(int day_of_week) {
        if (day_of_week < 1 || day_of_week > DAYS_PER_WEEK) {
            return QString();
        }
        return SHORT_DAY_NAMES.at(day_of_week);
    }

    QTime WeekCalculator::parse_time(const QString& text) {
        const QString normalised = normalise_time_text(text);
        if (normalised.isEmpty()) {
            return QTime();
        }

        static const QStringList formats = {
            QStringLiteral("HH:mm:ss"),
            QStringLiteral("HH:mm"),
            QStringLiteral("H:mm"),
            QStringLiteral("H:mm:ss"),
            QStringLiteral("HHmm"),
        };
        for (const QString& format : formats) {
            const QTime time = QTime::fromString(normalised, format);
            if (time.isValid()) {
                return time;
            }
        }
        return QTime();
    }

    QString WeekCalculator::format_time(const QTime& time) {
        return time.isValid() ? time.toString(QStringLiteral("HH:mm")) : QString();
    }

    QDateTime WeekCalculator::session_start_datetime(const Semester& semester, const TimeSlot& slot, int week, int day_of_week) {
        const QDate date = date_of(semester, week, day_of_week);
        if (!date.isValid() || !slot.start_time.isValid()) {
            return QDateTime();
        }
        return QDateTime(date, slot.start_time);
    }

    int WeekCalculator::parse_day_of_week(const QString& text) {
        const QString normalised = text.trimmed().toLower();
        if (normalised.isEmpty()) {
            return 0;
        }

        // 纯数字：1..7
        bool numeric_ok = false;
        const int numeric = normalised.toInt(&numeric_ok);
        if (numeric_ok) {
            return (numeric >= 1 && numeric <= 7) ? numeric : 0;
        }

        // 中文：一 / 周一 / 星期一 / 礼拜一
        for (int day = 1; day <= DAYS_PER_WEEK; ++day) {
            const QString short_name = SHORT_DAY_NAMES.at(day);
            if (normalised == short_name || normalised == QStringLiteral("周") + short_name || normalised == QStringLiteral("星期") + short_name || normalised == QStringLiteral("礼拜") + short_name) {
                return day;
            }
        }
        if (normalised == QStringLiteral("周日") || normalised == QStringLiteral("周天") || normalised == QStringLiteral("星期日") || normalised == QStringLiteral("星期天") || normalised == QStringLiteral("日") || normalised == QStringLiteral("天")) {
            return 7;
        }

        // 英文：Monday / Mon
        static const QStringList english = {
            QString(),
            QStringLiteral("monday"),
            QStringLiteral("tuesday"),
            QStringLiteral("wednesday"),
            QStringLiteral("thursday"),
            QStringLiteral("friday"),
            QStringLiteral("saturday"),
            QStringLiteral("sunday"),
        };
        static const QStringList english_short = {
            QString(),
            QStringLiteral("mon"),
            QStringLiteral("tue"),
            QStringLiteral("wed"),
            QStringLiteral("thu"),
            QStringLiteral("fri"),
            QStringLiteral("sat"),
            QStringLiteral("sun"),
        };
        for (int day = 1; day <= DAYS_PER_WEEK; ++day) {
            if (normalised == english.at(day) || normalised == english_short.at(day)) {
                return day;
            }
        }

        // ICS 中常见的两字母前缀：MO / TU / WE / TH / FR / SA / SU
        static const QStringList ics_codes = {
            QString(),
            QStringLiteral("mo"),
            QStringLiteral("tu"),
            QStringLiteral("we"),
            QStringLiteral("th"),
            QStringLiteral("fr"),
            QStringLiteral("sa"),
            QStringLiteral("su"),
        };
        for (int day = 1; day <= DAYS_PER_WEEK; ++day) {
            if (normalised.startsWith(ics_codes.at(day))) {
                return day;
            }
        }
        return 0;
    }

} // namespace Schedule
