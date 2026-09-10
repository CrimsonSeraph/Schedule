#include "core/model/ScheduleSnapshot.h"
#include "core/model/Semester.h"
#include "core/model/TimeSlot.h"
#include "core/service/ReminderScheduler.h"

#include <QTest>

using Schedule::Course;
using Schedule::CourseSession;
using Schedule::Reminder;
using Schedule::ReminderScheduler;
using Schedule::ScheduleSnapshot;
using Schedule::Semester;
using Schedule::TimeSlot;
using Schedule::WeekMask;

namespace {

    const QDate SEMESTER_START(2024, 9, 2); // 周一

    /** @return 含两门课（周一 1-2 节每周、周三 5-6 节单周）的测试快照。 */
    ScheduleSnapshot make_snapshot() {
        ScheduleSnapshot snapshot;
        snapshot.semester = Semester::create(QStringLiteral("测试学期"), SEMESTER_START, 16);
        snapshot.time_slots = TimeSlot::default_slots();

        Course math;
        math.id = QStringLiteral("math");
        math.name = QStringLiteral("高等数学");
        math.teacher = QStringLiteral("张老师");
        math.location = QStringLiteral("教一 101");

        CourseSession math_session;
        math_session.id = QStringLiteral("math-1");
        math_session.day_of_week = 1;
        math_session.start_slot = 1;
        math_session.slot_count = 2;
        math_session.weeks = WeekMask::from_expression(QStringLiteral("1-16"), 16);
        math.sessions.append(math_session);
        snapshot.courses.append(math);

        Course physics;
        physics.id = QStringLiteral("physics");
        physics.name = QStringLiteral("大学物理");

        CourseSession physics_session;
        physics_session.id = QStringLiteral("physics-1");
        physics_session.day_of_week = 3;
        physics_session.start_slot = 5;
        physics_session.slot_count = 2;
        physics_session.weeks = WeekMask::from_expression(QStringLiteral("1-15/2"), 16);
        physics_session.location = QStringLiteral("理科楼 204");
        physics.sessions.append(physics_session);
        snapshot.courses.append(physics);

        return snapshot;
    }

} // namespace

/**
 * @brief 提醒计算器的单元测试：窗口过滤、到点判定、下一次提醒与文本格式化。
 */
class TestReminderScheduler : public QObject {
    Q_OBJECT

private slots:
    /** 支持的提前分钟数只有 5 / 10 / 15。 */
    void validates_supported_minutes();

    /** 某天的提醒列表按上课时刻排序。 */
    void lists_reminders_for_date();

    /** 单双周课程只在对应周次出现。 */
    void honours_week_mask();

    /** 未来窗口过滤：过去的提醒不补发。 */
    void filters_by_time_window();

    /** 到点判定：提醒时刻之后、上课之前才算到点。 */
    void detects_due_reminders();

    /** 取下一次提醒。 */
    void finds_next_reminder();

    /** 文本格式化与去重键。 */
    void formats_reminder_text();
};

void TestReminderScheduler::validates_supported_minutes() {
    QCOMPARE(ReminderScheduler::supported_minutes(), QList<int>({5, 10, 15}));
    QVERIFY(ReminderScheduler::is_supported_minutes(5));
    QVERIFY(ReminderScheduler::is_supported_minutes(10));
    QVERIFY(ReminderScheduler::is_supported_minutes(15));
    QVERIFY(!ReminderScheduler::is_supported_minutes(0));
    QVERIFY(!ReminderScheduler::is_supported_minutes(30));
}

void TestReminderScheduler::lists_reminders_for_date() {
    const ScheduleSnapshot snapshot = make_snapshot();

    // 第 1 周周一：高等数学 08:00
    const QList<Reminder> monday = ReminderScheduler::for_date(snapshot, SEMESTER_START, 10);
    QCOMPARE(monday.size(), 1);
    QCOMPARE(monday.first().course_name, QStringLiteral("高等数学"));
    QCOMPARE(monday.first().start, QDateTime(QDate(2024, 9, 2), QTime(8, 0)));
    // 提前 10 分钟 → 07:50
    QCOMPARE(monday.first().remind_at, QDateTime(QDate(2024, 9, 2), QTime(7, 50)));
    QCOMPARE(monday.first().week, 1);
    QCOMPARE(monday.first().day_of_week, 1);
    QVERIFY(monday.first().is_valid());

    // 第 1 周周三：大学物理 14:00
    const QList<Reminder> wednesday = ReminderScheduler::for_date(snapshot, QDate(2024, 9, 4), 15);
    QCOMPARE(wednesday.size(), 1);
    QCOMPARE(wednesday.first().course_name, QStringLiteral("大学物理"));
    QCOMPARE(wednesday.first().start, QDateTime(QDate(2024, 9, 4), QTime(14, 0)));
    QCOMPARE(wednesday.first().remind_at, QDateTime(QDate(2024, 9, 4), QTime(13, 45)));

    // 没有课的日期
    QVERIFY(ReminderScheduler::for_date(snapshot, QDate(2024, 9, 3), 10).isEmpty());
    // 学期之外
    QVERIFY(ReminderScheduler::for_date(snapshot, QDate(2025, 5, 1), 10).isEmpty());

    // 非法提前分钟数回退为默认值 10
    const QList<Reminder> fallback = ReminderScheduler::for_date(snapshot, SEMESTER_START, 99);
    QCOMPARE(fallback.first().minutes_before, ReminderScheduler::DEFAULT_MINUTES_BEFORE);
}

void TestReminderScheduler::honours_week_mask() {
    const ScheduleSnapshot snapshot = make_snapshot();

    // 大学物理是单周课：第 1 周有、第 2 周没有
    const int first_week = snapshot.semester.week_of(SEMESTER_START);
    const QDate week_two_wednesday = snapshot.semester.week_start_date(2).addDays(2);
    QCOMPARE(first_week, 1);
    QCOMPARE(ReminderScheduler::for_date(snapshot, QDate(2024, 9, 4), 10).size(), 1);
    QVERIFY(ReminderScheduler::for_date(snapshot, week_two_wednesday, 10).isEmpty());
}

void TestReminderScheduler::filters_by_time_window() {
    const ScheduleSnapshot snapshot = make_snapshot();

    // now = 周一 09:00，此时高等数学（08:00）已经开课，不应再出现
    const QDateTime after_class(SEMESTER_START, QTime(9, 0));
    QVERIFY(ReminderScheduler::upcoming(snapshot, after_class, 10, 24).isEmpty());

    // now = 周一 07:00：高等数学 07:50 提醒落在 24 小时窗口内，周三的课还不在窗口内
    const QDateTime early(SEMESTER_START, QTime(7, 0));
    QCOMPARE(ReminderScheduler::upcoming(snapshot, early, 10, 24).size(), 1);
    QCOMPARE(ReminderScheduler::upcoming(snapshot, early, 10, 24).first().course_name, QStringLiteral("高等数学"));

    // 窗口放大到 72 小时（周一 07:00 ~ 周四 07:00）：两门课都在范围内且按时间升序
    const QList<Reminder> upcoming = ReminderScheduler::upcoming(snapshot, early, 10, 72);
    QCOMPARE(upcoming.size(), 2);
    QCOMPARE(upcoming.first().course_name, QStringLiteral("高等数学"));
    QCOMPARE(upcoming.last().course_name, QStringLiteral("大学物理"));
    QVERIFY(upcoming.first().remind_at <= upcoming.last().remind_at);

    // 只向前看 2 小时：仅剩高等数学
    QCOMPARE(ReminderScheduler::upcoming(snapshot, early, 10, 2).size(), 1);

    // 无效输入返回空列表
    QVERIFY(ReminderScheduler::upcoming(ScheduleSnapshot(), early, 10, 24).isEmpty());
    QVERIFY(ReminderScheduler::upcoming(snapshot, QDateTime(), 10, 24).isEmpty());
}

void TestReminderScheduler::detects_due_reminders() {
    const Reminder reminder = ReminderScheduler::for_date(make_snapshot(), SEMESTER_START, 10).first();
    QCOMPARE(reminder.remind_at, QDateTime(QDate(2024, 9, 2), QTime(7, 50)));
    QCOMPARE(reminder.start, QDateTime(QDate(2024, 9, 2), QTime(8, 0)));

    // 提醒时刻之前：还没到点
    QVERIFY(!ReminderScheduler::is_due(reminder, QDateTime(QDate(2024, 9, 2), QTime(7, 49))));
    // 恰好到点
    QVERIFY(ReminderScheduler::is_due(reminder, reminder.remind_at));
    // 上课前一分钟仍然算到点
    QVERIFY(ReminderScheduler::is_due(reminder, QDateTime(QDate(2024, 9, 2), QTime(7, 59))));
    // 已经上课：不再提醒
    QVERIFY(!ReminderScheduler::is_due(reminder, reminder.start));
    QVERIFY(!ReminderScheduler::is_due(reminder, QDateTime(QDate(2024, 9, 2), QTime(8, 30))));

    // 无效提醒不算到点
    QVERIFY(!ReminderScheduler::is_due(Reminder(), QDateTime(QDate(2024, 9, 2), QTime(7, 55))));
}

void TestReminderScheduler::finds_next_reminder() {
    const ScheduleSnapshot snapshot = make_snapshot();

    Reminder next;
    QVERIFY(ReminderScheduler::next(snapshot, QDateTime(SEMESTER_START, QTime(7, 0)), 10, &next));
    QCOMPARE(next.course_name, QStringLiteral("高等数学"));

    // 周一课程结束后，下一次是周三的大学物理
    QVERIFY(ReminderScheduler::next(snapshot, QDateTime(SEMESTER_START, QTime(9, 0)), 10, &next));
    QCOMPARE(next.course_name, QStringLiteral("大学物理"));
    QCOMPARE(next.start.date(), QDate(2024, 9, 4));

    // 学期结束后没有下一次
    QVERIFY(!ReminderScheduler::next(snapshot, QDateTime(QDate(2025, 5, 1), QTime(8, 0)), 10, &next));
}

void TestReminderScheduler::formats_reminder_text() {
    const Reminder reminder = ReminderScheduler::for_date(make_snapshot(), SEMESTER_START, 10).first();

    QCOMPARE(reminder.title(), QStringLiteral("10 分钟后上课：高等数学"));
    QVERIFY(reminder.message().contains(QStringLiteral("08:00")));
    QVERIFY(reminder.message().contains(QStringLiteral("教一 101")));
    QVERIFY(reminder.message().contains(QStringLiteral("张老师")));
    QVERIFY(reminder.message().contains(QStringLiteral("第 1 周")));
    QVERIFY(reminder.message().contains(QStringLiteral("周一")));

    // 去重键包含课程、时间段、周次与日期，保证同一节课只提醒一次
    QVERIFY(reminder.unique_key().contains(QStringLiteral("math")));
    QVERIFY(reminder.unique_key().contains(QStringLiteral("math-1")));
    QVERIFY(reminder.unique_key().contains(QStringLiteral("2024-09-02")));

    const Reminder other = ReminderScheduler::for_date(make_snapshot(), QDate(2024, 9, 9), 10).first();
    QVERIFY(other.unique_key() != reminder.unique_key());
}

QTEST_GUILESS_MAIN(TestReminderScheduler)

#include "tst_reminder_scheduler.moc"
