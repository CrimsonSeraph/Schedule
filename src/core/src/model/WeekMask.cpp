#include "core/model/WeekMask.h"

#include <QRegularExpression>
#include <QStringList>

#include <bit>

namespace Schedule {

    namespace {

        /** @return 第 week 周对应的位；越界返回 0（表示“无此位”）。 */
        quint64 bit_of(int week) {
            if (week < 1 || week > WeekMask::MAX_WEEKS) {
                return 0;
            }
            return quint64(1) << (week - 1);
        }

        /** @return 位图中置位的数量。 */
        int bit_count(quint64 bits) {
            return static_cast<int>(std::popcount(bits));
        }

        /** @return 最低置位对应的周次；bits 为 0 时返回 0。 */
        int lowest_week(quint64 bits) {
            if (bits == 0) {
                return 0;
            }
            return static_cast<int>(std::countr_zero(bits)) + 1;
        }

        /** @return 最高置位对应的周次；bits 为 0 时返回 0。 */
        int highest_week(quint64 bits) {
            if (bits == 0) {
                return 0;
            }
            return 64 - static_cast<int>(std::countl_zero(bits));
        }

        /**
         * @brief 把表达式切成片段。
         *
         * 同时接受中英文逗号 / 分号与任意空白，兼容用户从 Excel、网页复制的文本
         * （这类文本经常混用 `,` 与 `，`，或使用空格分隔）。
         */
        QStringList split_segments(const QString& expression) {
            static const QRegularExpression separator(QStringLiteral("[,，;；\\s]+"));
            return expression.split(separator, Qt::SkipEmptyParts);
        }

        /** @return 屏蔽大小写与首尾空白的规范化片段。 */
        QString normalise_segment(const QString& segment) {
            return segment.trimmed().toLower();
        }

        /**
         * @brief 解析 `A`、`A-B`、`A-B/S`、`A/S` 四种数值型片段。
         *
         * @param segment 已规范化的片段
         * @param upper   允许的最大周次（学期总周数或 MAX_WEEKS）
         * @param out     解析结果（含进掩码）
         * @param error   失败时的中文原因
         * @return 是否解析成功
         */
        bool parse_numeric_segment(const QString& segment, int upper, quint64* out, QString* error) {
            static const QRegularExpression pattern(QStringLiteral("^(\\d+)(?:\\s*-\\s*(\\d+))?(?:\\s*/\\s*(\\d+))?$"));
            const QRegularExpressionMatch match = pattern.match(segment);
            if (!match.hasMatch()) {
                if (error) {
                    *error = QStringLiteral("无法识别的周次片段：\"%1\"").arg(segment);
                }
                return false;
            }

            const int begin = match.captured(1).toInt();
            const bool has_end = !match.captured(2).isEmpty();
            const bool has_step = !match.captured(3).isEmpty();
            const int end = has_end ? match.captured(2).toInt() : begin;
            const int step = has_step ? match.captured(3).toInt() : 1;

            if (begin < 1) {
                if (error) {
                    *error = QStringLiteral("周次必须从 1 开始：\"%1\"").arg(segment);
                }
                return false;
            }
            if (end < begin) {
                if (error) {
                    *error = QStringLiteral("周次区间起止颠倒：\"%1\"").arg(segment);
                }
                return false;
            }
            if (step < 1) {
                if (error) {
                    *error = QStringLiteral("周次步长必须大于 0：\"%1\"").arg(segment);
                }
                return false;
            }
            // 只有 `A/S`（显式给出步长、但未给出末周）才表示“从 A 起按步长 S 排到学期末”；
            // 裸写 `A` 必须始终是单个周次，否则 `1-4,6,8-10` 里的 `6` 会被错误地展开成 `6-16`。
            const int limit = (has_step && !has_end) ? upper : end;
            if (limit > upper) {
                if (error) {
                    *error = QStringLiteral("周次超出学期范围（上限 %1）：\"%2\"").arg(upper).arg(segment);
                }
                return false;
            }

            quint64 bits = 0;
            for (int week = begin; week <= limit; week += step) {
                bits |= bit_of(week);
            }
            *out |= bits;
            return true;
        }

    } // namespace

    WeekMask::WeekMask(quint64 bits)
        : m_bits(bits) {
    }

    WeekMask WeekMask::from_bits(quint64 bits) {
        return WeekMask(bits);
    }

    WeekMask WeekMask::from_weeks(const QList<int>& weeks) {
        WeekMask mask;
        mask.set_weeks(weeks);
        return mask;
    }

    WeekMask WeekMask::all(int total_weeks) {
        const int upper = total_weeks > 0 ? qMin(total_weeks, MAX_WEEKS) : MAX_WEEKS;
        quint64 bits = 0;
        for (int week = 1; week <= upper; ++week) {
            bits |= bit_of(week);
        }
        return WeekMask(bits);
    }

    WeekMask WeekMask::odd(int total_weeks) {
        WeekMask mask;
        const int upper = total_weeks > 0 ? qMin(total_weeks, MAX_WEEKS) : MAX_WEEKS;
        for (int week = 1; week <= upper; week += 2) {
            mask.add(week);
        }
        return mask;
    }

    WeekMask WeekMask::even(int total_weeks) {
        WeekMask mask;
        const int upper = total_weeks > 0 ? qMin(total_weeks, MAX_WEEKS) : MAX_WEEKS;
        for (int week = 2; week <= upper; week += 2) {
            mask.add(week);
        }
        return mask;
    }

    WeekMask WeekMask::from_expression(const QString& expression, int total_weeks, QString* error_message) {
        if (error_message) {
            error_message->clear();
        }

        const int upper = total_weeks > 0 ? qMin(total_weeks, MAX_WEEKS) : MAX_WEEKS;
        const QStringList segments = split_segments(expression);
        if (segments.isEmpty()) {
            // 空表达式是合法的：表示“未设置周次”，由上层决定是否视为错误。
            return WeekMask();
        }

        quint64 bits = 0;
        QString error;
        for (const QString& raw_segment : segments) {
            const QString segment = normalise_segment(raw_segment);
            if (segment == QStringLiteral("*") || segment == QStringLiteral("all") || segment == QStringLiteral("全周")) {
                bits |= all(upper).bits();
                continue;
            }
            if (segment == QStringLiteral("odd") || segment == QStringLiteral("单周")) {
                bits |= odd(upper).bits();
                continue;
            }
            if (segment == QStringLiteral("even") || segment == QStringLiteral("双周")) {
                bits |= even(upper).bits();
                continue;
            }
            if (!parse_numeric_segment(segment, upper, &bits, &error)) {
                if (error_message) {
                    *error_message = error;
                }
                return WeekMask();
            }
        }

        return WeekMask(bits);
    }

    bool WeekMask::validate_expression(const QString& expression, int total_weeks, QString* error_message) {
        QString error;
        from_expression(expression, total_weeks, &error);
        if (!error.isEmpty()) {
            if (error_message) {
                *error_message = error;
            }
            return false;
        }
        if (error_message) {
            error_message->clear();
        }
        return true;
    }

    bool WeekMask::is_empty() const {
        return m_bits == 0;
    }

    bool WeekMask::contains(int week) const {
        return (m_bits & bit_of(week)) != 0;
    }

    int WeekMask::count() const {
        return bit_count(m_bits);
    }

    int WeekMask::first_week() const {
        return lowest_week(m_bits);
    }

    int WeekMask::last_week() const {
        return highest_week(m_bits);
    }

    quint64 WeekMask::bits() const {
        return m_bits;
    }

    QList<int> WeekMask::weeks() const {
        QList<int> result;
        if (m_bits == 0) {
            return result;
        }
        result.reserve(count());
        for (int week = first_week(); week <= last_week(); ++week) {
            if (contains(week)) {
                result.append(week);
            }
        }
        return result;
    }

    void WeekMask::add(int week) {
        m_bits |= bit_of(week);
    }

    void WeekMask::remove(int week) {
        m_bits &= ~bit_of(week);
    }

    void WeekMask::clear() {
        m_bits = 0;
    }

    void WeekMask::set_weeks(const QList<int>& weeks) {
        m_bits = 0;
        for (int week : weeks) {
            add(week);
        }
    }

    WeekMask WeekMask::united(const WeekMask& other) const {
        return WeekMask(m_bits | other.m_bits);
    }

    WeekMask WeekMask::intersected(const WeekMask& other) const {
        return WeekMask(m_bits & other.m_bits);
    }

    WeekMask WeekMask::subtracted(const WeekMask& other) const {
        return WeekMask(m_bits & ~other.m_bits);
    }

    bool WeekMask::intersects(const WeekMask& other) const {
        return (m_bits & other.m_bits) != 0;
    }

    bool WeekMask::contains_all(const WeekMask& other) const {
        return (m_bits & other.m_bits) == other.m_bits;
    }

    QString WeekMask::to_expression() const {
        const QList<int> list = weeks();
        if (list.isEmpty()) {
            return QString();
        }

        // 优先识别整体等差数列（典型场景是单双周：1-16/2、2-16/2）。
        if (list.size() >= 2) {
            const int step = list.at(1) - list.at(0);
            if (step > 1) {
                bool arithmetic = true;
                for (int i = 2; i < list.size(); ++i) {
                    if (list.at(i) - list.at(i - 1) != step) {
                        arithmetic = false;
                        break;
                    }
                }
                if (arithmetic) {
                    return QStringLiteral("%1-%2/%3").arg(list.first()).arg(list.last()).arg(step);
                }
            }
        }

        // 退化情形：按“连续区间”贪心合并，如 1-4,6,8-10。
        QStringList parts;
        int run_begin = list.first();
        int previous = list.first();
        for (int i = 1; i < list.size(); ++i) {
            const int week = list.at(i);
            if (week == previous + 1) {
                previous = week;
                continue;
            }
            parts.append(run_begin == previous ? QString::number(run_begin)
                                               : QStringLiteral("%1-%2").arg(run_begin).arg(previous));
            run_begin = week;
            previous = week;
        }
        parts.append(run_begin == previous ? QString::number(run_begin)
                                           : QStringLiteral("%1-%2").arg(run_begin).arg(previous));
        return parts.join(QLatin1Char(','));
    }

    QString WeekMask::to_display_string() const {
        if (is_empty()) {
            return QStringLiteral("无");
        }

        const QList<int> list = weeks();
        bool all_odd = list.size() >= 2;
        bool all_even = list.size() >= 2;
        for (int week : list) {
            if (week % 2 == 0) {
                all_odd = false;
            }
            else {
                all_even = false;
            }
        }

        const QString expression = to_expression();
        if (all_odd) {
            return QStringLiteral("第 %1 周（单周）").arg(expression);
        }
        if (all_even) {
            return QStringLiteral("第 %1 周（双周）").arg(expression);
        }
        return QStringLiteral("第 %1 周").arg(expression);
    }

    bool WeekMask::operator==(const WeekMask& other) const {
        return m_bits == other.m_bits;
    }

    bool WeekMask::operator!=(const WeekMask& other) const {
        return m_bits != other.m_bits;
    }

} // namespace Schedule
