#include "core/model/Course.h"

#include <QRegularExpression>
#include <QUuid>

namespace Schedule {

    namespace {

        /** @return 是否为 `#RRGGBB` 或 `#AARRGGBB` 形式的颜色串。 */
        bool is_valid_color(const QString& color) {
            static const QRegularExpression pattern(QStringLiteral("^#(?:[0-9a-fA-F]{6}|[0-9a-fA-F]{8})$"));
            return pattern.match(color).hasMatch();
        }

        /**
         * @brief 课卡调色板：低饱和、在深浅主题下均可读。
         *
         * 与 `Course::default_color_for()` 的散列结果取模使用。
         */
        const char* const COLOR_PALETTE[] = {
            "#4C8DFF", // 蓝
            "#37B67A", // 绿
            "#E0873C", // 橙
            "#B158D8", // 紫
            "#D9534F", // 红
            "#2AA6B8", // 青
            "#C9A227", // 金
            "#5C7CFA", // 靛
            "#7A9E3F", // 橄榄
            "#C2568C", // 洋红
        };
        constexpr int COLOR_PALETTE_SIZE = int(sizeof(COLOR_PALETTE) / sizeof(COLOR_PALETTE[0]));

    } // namespace

    bool Course::has_sessions() const {
        for (const CourseSession& session : sessions) {
            if (session.is_valid()) {
                return true;
            }
        }
        return false;
    }

    WeekMask Course::total_weeks() const {
        WeekMask mask;
        for (const CourseSession& session : sessions) {
            mask = mask.united(session.weeks);
        }
        return mask;
    }

    bool Course::occurs_on(int day_of_week, int week) const {
        for (const CourseSession& session : sessions) {
            if (session.day_of_week == day_of_week && session.weeks.contains(week)) {
                return true;
            }
        }
        return false;
    }

    QList<CourseSession> Course::sessions_on_day(int day_of_week) const {
        QList<CourseSession> result;
        for (const CourseSession& session : sessions) {
            if (session.day_of_week == day_of_week) {
                result.append(session);
            }
        }
        return result;
    }

    bool Course::is_valid(int max_slot, QString* error_message) const {
        if (name.trimmed().isEmpty()) {
            if (error_message) {
                *error_message = QStringLiteral("课程名称不能为空");
            }
            return false;
        }

        for (const CourseSession& session : sessions) {
            QString session_error;
            if (!session.is_valid(max_slot, &session_error)) {
                if (error_message) {
                    *error_message = QStringLiteral("课程“%1”的上课时间不合法：%2").arg(name, session_error);
                }
                return false;
            }
        }

        if (error_message) {
            error_message->clear();
        }
        return true;
    }

    void Course::ensure_session_ids() {
        for (CourseSession& session : sessions) {
            if (session.id.isEmpty()) {
                session.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            }
        }
    }

    QString Course::display_color() const {
        if (is_valid_color(color)) {
            return color;
        }
        return default_color_for(name.isEmpty() ? id : name);
    }

    QString Course::default_color_for(const QString& seed) {
        if (seed.isEmpty()) {
            return QString::fromLatin1(COLOR_PALETTE[0]);
        }
        // 简单 32 位散列（djb2 变体）：只求“稳定且分布均匀”，不用于安全用途。
        quint32 hash = 5381;
        for (const QChar& character : seed) {
            hash = (hash * 33) ^ static_cast<quint32>(character.unicode());
        }
        return QString::fromLatin1(COLOR_PALETTE[hash % COLOR_PALETTE_SIZE]);
    }

    bool Course::operator==(const Course& other) const {
        return id == other.id && semester_id == other.semester_id && name == other.name && code == other.code && teacher == other.teacher && location == other.location && color == other.color && qFuzzyCompare(credits + 1.0, other.credits + 1.0) && notes == other.notes && sessions == other.sessions;
    }

    bool Course::operator!=(const Course& other) const {
        return !(*this == other);
    }

} // namespace Schedule
