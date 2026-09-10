#include "core/model/Semester.h"
#include "core/model/TimeSlot.h"
#include "core/service/WeekCalculator.h"

#include <QTest>

using Schedule::Semester;
using Schedule::TimeSlot;
using Schedule::WeekCalculator;

namespace {

    /** 2024-09-02 是周一，作为测试用学期的第 1 周起始日。 */
    const QDate SEMESTER_START(2024, 9, 2);

    /** @return 一个 2024-09-02 起、共 16 周的测试学期。 */
    Semester make_semester(int total_weeks = 16) {
        return Semester::create(QStringLiteral("2024-2025 学年第一学期"), SEMESTER_START, total_weeks);
    }

} // namespace

/**
 * @brief 周次 / 日期换算与时间解析的单元测试。
 */
class TestWeekCalculator : public QObject {
    Q_OBJECT

private slots:
    /** 学期结构自检的边界条件。 */
    void validates_semester();

    /** 日期 → 周次换算（含边界与越界）。 */
    void converts_date_to_week();

    /** 周次 → 日期换算。 */
    void converts_week_to_date();

    /** 当前周计算（相对任意“今天”）。 */
    void computes_current_week();

    /** 起始日不是周一时，仍按自然周对齐。 */
    void aligns_to_calendar_week();

    /** 星期文本解析覆盖中英文与 ICS 缩写。 */
    void parses_day_of_week();

    /** 时间解析与格式化覆盖多种书写形式。 */
    void parses_and_formats_time();

    /** 计算某次课的开始时刻。 */
    void computes_session_start_datetime();
};

void TestWeekCalculator::validates_semester() {
    QString error;

    QVERIFY(make_semester().is_valid(&error));
    QVERIFY(error.isEmpty());

    Semester nameless = make_semester();
    nameless.name.clear();
    QVERIFY(!nameless.is_valid(&error));
    QVERIFY(!error.isEmpty());

    Semester no_date = make_semester();
    no_date.start_date = QDate();
    QVERIFY(!no_date.is_valid(&error));

    Semester too_long = make_semester();
    too_long.total_weeks = 65;
    QVERIFY(!too_long.is_valid(&error));

    // create() 会把周数夹取到合法区间
    QCOMPARE(Semester::create(QStringLiteral("x"), SEMESTER_START, 999).total_weeks, 64);
    QCOMPARE(Semester::create(QStringLiteral("x"), SEMESTER_START, -3).total_weeks, 1);
}

void TestWeekCalculator::converts_date_to_week() {
    const Semester semester = make_semester();

    QCOMPARE(semester.week_of(SEMESTER_START), 1);
    QCOMPARE(semester.week_of(SEMESTER_START.addDays(6)), 1);  // 第 1 周周日
    QCOMPARE(semester.week_of(SEMESTER_START.addDays(7)), 2);  // 第 2 周周一
    QCOMPARE(semester.week_of(SEMESTER_START.addDays(-1)), 0); // 学期开始前一天
    QCOMPARE(semester.week_of(semester.end_date()), 16);
    QCOMPARE(semester.week_of(semester.end_date().addDays(1)), 0);

    QVERIFY(semester.contains(SEMESTER_START));
    QVERIFY(!semester.contains(SEMESTER_START.addDays(-1)));

    // 跨年场景：2025-01-06 属于第 19 周（若学期足够长）
    const Semester long_semester = make_semester(20);
    QCOMPARE(long_semester.week_of(QDate(2025, 1, 6)), 19);
}

void TestWeekCalculator::converts_week_to_date() {
    const Semester semester = make_semester();

    QCOMPARE(WeekCalculator::week_start_date(semester, 1), SEMESTER_START);
    QCOMPARE(WeekCalculator::week_start_date(semester, 3), SEMESTER_START.addDays(14));
    QCOMPARE(WeekCalculator::week_end_date(semester, 1), SEMESTER_START.addDays(6));
    QCOMPARE(semester.end_date(), SEMESTER_START.addDays(16 * 7 - 1));

    // 星期取值与 QDate::dayOfWeek() 一致：1=周一 ... 7=周日
    QCOMPARE(WeekCalculator::date_of(semester, 1, 1), SEMESTER_START);
    QCOMPARE(WeekCalculator::date_of(semester, 1, 7), SEMESTER_START.addDays(6));
    QCOMPARE(WeekCalculator::date_of(semester, 2, 3), SEMESTER_START.addDays(9));

    // 越界返回无效日期
    QVERIFY(!WeekCalculator::week_start_date(semester, 0).isValid());
    QVERIFY(!WeekCalculator::week_start_date(semester, 17).isValid());
    QVERIFY(!WeekCalculator::date_of(semester, 1, 8).isValid());

    QCOMPARE(WeekCalculator::monday_of(QDate(2024, 9, 8)), SEMESTER_START);
    QVERIFY(!WeekCalculator::monday_of(QDate()).isValid());
}

void TestWeekCalculator::computes_current_week() {
    const Semester semester = make_semester();

    QCOMPARE(WeekCalculator::current_week(semester, SEMESTER_START), 1);
    QCOMPARE(WeekCalculator::current_week(semester, SEMESTER_START.addDays(20)), 3);
    QCOMPARE(WeekCalculator::current_week(semester, SEMESTER_START.addDays(-10)), 0);
    QCOMPARE(WeekCalculator::current_week(semester, SEMESTER_START.addDays(200)), 0);
}

void TestWeekCalculator::aligns_to_calendar_week() {
    // 学期从周三开始：第 1 周仍应是包含该周三的自然周（周一 09-02 起）
    Semester semester = Semester::create(QStringLiteral("周三开学"), QDate(2024, 9, 4), 16);

    QCOMPARE(semester.week_start_date(1), SEMESTER_START);
    QCOMPARE(semester.week_of(QDate(2024, 9, 2)), 1);
    QCOMPARE(semester.week_of(QDate(2024, 9, 4)), 1);
    QCOMPARE(semester.week_of(QDate(2024, 9, 9)), 2);
    QCOMPARE(semester.week_of(QDate(2024, 9, 1)), 0); // 上一个自然周不属于本学期
}

void TestWeekCalculator::parses_day_of_week() {
    QCOMPARE(WeekCalculator::parse_day_of_week(QStringLiteral("1")), 1);
    QCOMPARE(WeekCalculator::parse_day_of_week(QStringLiteral("7")), 7);
    QCOMPARE(WeekCalculator::parse_day_of_week(QStringLiteral("0")), 0);
    QCOMPARE(WeekCalculator::parse_day_of_week(QStringLiteral("8")), 0);
    QCOMPARE(WeekCalculator::parse_day_of_week(QStringLiteral("一")), 1);
    QCOMPARE(WeekCalculator::parse_day_of_week(QStringLiteral("周三")), 3);
    QCOMPARE(WeekCalculator::parse_day_of_week(QStringLiteral("星期四")), 4);
    QCOMPARE(WeekCalculator::parse_day_of_week(QStringLiteral("周日")), 7);
    QCOMPARE(WeekCalculator::parse_day_of_week(QStringLiteral("星期天")), 7);
    QCOMPARE(WeekCalculator::parse_day_of_week(QStringLiteral("Monday")), 1);
    QCOMPARE(WeekCalculator::parse_day_of_week(QStringLiteral("fri")), 5);
    QCOMPARE(WeekCalculator::parse_day_of_week(QStringLiteral("TU")), 2);
    QCOMPARE(WeekCalculator::parse_day_of_week(QStringLiteral("SU")), 7);
    QCOMPARE(WeekCalculator::parse_day_of_week(QStringLiteral("")), 0);
    QCOMPARE(WeekCalculator::parse_day_of_week(QStringLiteral("星期八")), 0);

    QCOMPARE(WeekCalculator::day_name(2), QStringLiteral("周二"));
    QCOMPARE(WeekCalculator::day_name(0), QString());
    QCOMPARE(WeekCalculator::short_day_name(7), QStringLiteral("日"));
}

void TestWeekCalculator::parses_and_formats_time() {
    QCOMPARE(WeekCalculator::parse_time(QStringLiteral("08:00")), QTime(8, 0));
    QCOMPARE(WeekCalculator::parse_time(QStringLiteral("8:05")), QTime(8, 5));
    QCOMPARE(WeekCalculator::parse_time(QStringLiteral("08:00:30")), QTime(8, 0, 30));
    QCOMPARE(WeekCalculator::parse_time(QStringLiteral("0800")), QTime(8, 0));
    // 全角冒号（从网页复制常见）
    QCOMPARE(WeekCalculator::parse_time(QStringLiteral("08：00")), QTime(8, 0));
    QVERIFY(!WeekCalculator::parse_time(QStringLiteral("25:00")).isValid());
    QVERIFY(!WeekCalculator::parse_time(QString()).isValid());

    QCOMPARE(WeekCalculator::format_time(QTime(8, 0)), QStringLiteral("08:00"));
    QCOMPARE(WeekCalculator::format_time(QTime()), QString());
}

void TestWeekCalculator::computes_session_start_datetime() {
    const Semester semester = make_semester();
    const QList<TimeSlot> periods = TimeSlot::default_slots();

    // 第 3 周周三的第 1 节：2024-09-02 + 14 天 + 2 天 = 2024-09-18 08:00
    const QDateTime start = WeekCalculator::session_start_datetime(semester, periods.at(0), 3, 3);
    QVERIFY(start.isValid());
    QCOMPARE(start, QDateTime(QDate(2024, 9, 18), QTime(8, 0)));

    // 节次未定义时间时返回无效值，而不是伪造一个时间
    TimeSlot empty_slot;
    empty_slot.index = 1;
    QVERIFY(!WeekCalculator::session_start_datetime(semester, empty_slot, 1, 1).isValid());
    QVERIFY(!WeekCalculator::session_start_datetime(semester, periods.at(0), 99, 1).isValid());
}

QTEST_GUILESS_MAIN(TestWeekCalculator)

#include "tst_week_calculator.moc"
