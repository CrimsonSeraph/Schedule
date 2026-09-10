#include "core/model/Course.h"
#include "core/model/ScheduleSnapshot.h"
#include "core/model/Semester.h"
#include "core/model/TimeSlot.h"
#include "core/service/ConflictDetector.h"

#include <QTest>

using Schedule::Conflict;
using Schedule::ConflictDetector;
using Schedule::Course;
using Schedule::CourseSession;
using Schedule::Semester;
using Schedule::TimeSlot;
using Schedule::WeekMask;

namespace {

    const QDate SEMESTER_START(2024, 9, 2);

    /** @return 2024-09-02 起、共 16 周的测试学期。 */
    Semester make_semester(int total_weeks = 16) {
        return Semester::create(QStringLiteral("测试学期"), SEMESTER_START, total_weeks);
    }

    /** @return 构造一个上课时间段。 */
    CourseSession make_session(int day_of_week, int start_slot, int slot_count, const QString& weeks) {
        CourseSession session;
        session.day_of_week = day_of_week;
        session.start_slot = start_slot;
        session.slot_count = slot_count;
        session.weeks = WeekMask::from_expression(weeks, 16);
        return session;
    }

    /** @return 构造一门带单个时间段的课程。 */
    Course make_course(const QString& id, const QString& name, int day_of_week, int start_slot, int slot_count, const QString& weeks) {
        Course course;
        course.id = id;
        course.name = name;
        course.sessions.append(make_session(day_of_week, start_slot, slot_count, weeks));
        return course;
    }

    /** @return 统计指定类型的冲突数量。 */
    int count_of_type(const QList<Conflict>& conflicts, Conflict::Type type) {
        int count = 0;
        for (const Conflict& conflict : conflicts) {
            if (conflict.type == type) {
                ++count;
            }
        }
        return count;
    }

} // namespace

/**
 * @brief 冲突检测器的单元测试，覆盖时间重叠、数据非法与重复课程三类场景。
 */
class TestConflictDetector : public QObject {
    Q_OBJECT

private slots:
    /** 同一时间段、公共周次 → 时间冲突。 */
    void detects_time_overlap();

    /** 无公共周次 → 不冲突。 */
    void ignores_disjoint_weeks();

    /** 节次相邻不重叠，部分重叠算冲突。 */
    void handles_slot_boundaries();

    /** 不同星期不冲突。 */
    void ignores_different_days();

    /** 同一课程内部时间段重叠 → SelfOverlap。 */
    void detects_self_overlap();

    /** 课程没有任何时间段 → MissingSession。 */
    void detects_missing_session();

    /** 节次超出作息表 → OutOfRangeSlot。 */
    void detects_out_of_range_slot();

    /** 周次超出学期总周数 → InvalidWeek。 */
    void detects_invalid_week();

    /** 同名 / 同代码课程 → DuplicateCourse（非阻断）。 */
    void detects_duplicate_course();

    /** 无冲突课表返回空列表且顺序稳定。 */
    void returns_empty_for_clean_schedule();

    /** 结果排序稳定：同输入多次检测结果一致。 */
    void produces_stable_order();
};

void TestConflictDetector::detects_time_overlap() {
    const Semester semester = make_semester();
    const QList<TimeSlot> periods = TimeSlot::default_slots();

    QList<Course> courses;
    courses.append(make_course(QStringLiteral("c1"), QStringLiteral("高等数学"), 2, 1, 2, QStringLiteral("1-16")));
    courses.append(make_course(QStringLiteral("c2"), QStringLiteral("线性代数"), 2, 2, 2, QStringLiteral("1-16")));

    const QList<Conflict> conflicts = ConflictDetector::detect(courses, semester, periods);

    QCOMPARE(count_of_type(conflicts, Conflict::Type::TimeOverlap), 1);
    const Conflict& conflict = conflicts.first();
    QCOMPARE(conflict.course_id_a, QStringLiteral("c1"));
    QCOMPARE(conflict.course_id_b, QStringLiteral("c2"));
    QCOMPARE(conflict.day_of_week, 2);
    QCOMPARE(conflict.week, 1);
    QVERIFY(conflict.is_blocking());
    QVERIFY(conflict.message.contains(QStringLiteral("高等数学")));
    QVERIFY(conflict.message.contains(QStringLiteral("线性代数")));
}

void TestConflictDetector::ignores_disjoint_weeks() {
    const Semester semester = make_semester();
    const QList<TimeSlot> periods = TimeSlot::default_slots();

    QList<Course> courses;
    courses.append(make_course(QStringLiteral("c1"), QStringLiteral("单周课"), 2, 1, 2, QStringLiteral("1-15/2")));
    courses.append(make_course(QStringLiteral("c2"), QStringLiteral("双周课"), 2, 1, 2, QStringLiteral("2-16/2")));

    const QList<Conflict> conflicts = ConflictDetector::detect(courses, semester, periods);
    QCOMPARE(count_of_type(conflicts, Conflict::Type::TimeOverlap), 0);
}

void TestConflictDetector::handles_slot_boundaries() {
    const Semester semester = make_semester();
    const QList<TimeSlot> periods = TimeSlot::default_slots();

    // 相邻：1-2 节 与 3-4 节，不冲突
    QList<Course> adjacent;
    adjacent.append(make_course(QStringLiteral("c1"), QStringLiteral("A"), 2, 1, 2, QStringLiteral("1-16")));
    adjacent.append(make_course(QStringLiteral("c2"), QStringLiteral("B"), 2, 3, 2, QStringLiteral("1-16")));
    QCOMPARE(count_of_type(ConflictDetector::detect(adjacent, semester, periods), Conflict::Type::TimeOverlap), 0);

    // 部分重叠：1-2 节 与 2-3 节，冲突
    QList<Course> partial;
    partial.append(make_course(QStringLiteral("c1"), QStringLiteral("A"), 2, 1, 2, QStringLiteral("1-16")));
    partial.append(make_course(QStringLiteral("c2"), QStringLiteral("B"), 2, 2, 2, QStringLiteral("1-16")));
    QCOMPARE(count_of_type(ConflictDetector::detect(partial, semester, periods), Conflict::Type::TimeOverlap), 1);

    // 完全包含：1-4 节 与 2-3 节，冲突
    QList<Course> contained;
    contained.append(make_course(QStringLiteral("c1"), QStringLiteral("A"), 2, 1, 4, QStringLiteral("1-16")));
    contained.append(make_course(QStringLiteral("c2"), QStringLiteral("B"), 2, 2, 2, QStringLiteral("1-16")));
    QCOMPARE(count_of_type(ConflictDetector::detect(contained, semester, periods), Conflict::Type::TimeOverlap), 1);
}

void TestConflictDetector::ignores_different_days() {
    const Semester semester = make_semester();
    const QList<TimeSlot> periods = TimeSlot::default_slots();

    QList<Course> courses;
    courses.append(make_course(QStringLiteral("c1"), QStringLiteral("A"), 1, 1, 2, QStringLiteral("1-16")));
    courses.append(make_course(QStringLiteral("c2"), QStringLiteral("B"), 3, 1, 2, QStringLiteral("1-16")));

    QVERIFY(ConflictDetector::detect(courses, semester, periods).isEmpty());
}

void TestConflictDetector::detects_self_overlap() {
    const Semester semester = make_semester();
    const QList<TimeSlot> periods = TimeSlot::default_slots();

    Course course = make_course(QStringLiteral("c1"), QStringLiteral("体育"), 5, 1, 2, QStringLiteral("1-16"));
    course.sessions.append(make_session(5, 2, 2, QStringLiteral("1-16")));

    const QList<Conflict> conflicts = ConflictDetector::detect_in_course(course, semester, periods);
    QCOMPARE(count_of_type(conflicts, Conflict::Type::SelfOverlap), 1);
}

void TestConflictDetector::detects_missing_session() {
    const Semester semester = make_semester();
    const QList<TimeSlot> periods = TimeSlot::default_slots();

    Course course;
    course.id = QStringLiteral("c1");
    course.name = QStringLiteral("待排课程");

    const QList<Conflict> conflicts = ConflictDetector::detect_in_course(course, semester, periods);
    QCOMPARE(count_of_type(conflicts, Conflict::Type::MissingSession), 1);
}

void TestConflictDetector::detects_out_of_range_slot() {
    const Semester semester = make_semester();
    const QList<TimeSlot> periods = TimeSlot::default_slots(); // 共 11 节

    // 第 11 节起、连续 3 节 → 结束节次 13，超出作息表
    const Course course = make_course(QStringLiteral("c1"), QStringLiteral("越界课"), 2, 11, 3, QStringLiteral("1-16"));

    const QList<Conflict> conflicts = ConflictDetector::detect_in_course(course, semester, periods);
    QCOMPARE(count_of_type(conflicts, Conflict::Type::OutOfRangeSlot), 1);
}

void TestConflictDetector::detects_invalid_week() {
    const Semester semester = make_semester(16);
    const QList<TimeSlot> periods = TimeSlot::default_slots();

    // 直接构造第 20 周（超出学期总周数），模拟外部导入数据
    CourseSession session;
    session.day_of_week = 2;
    session.start_slot = 1;
    session.slot_count = 2;
    session.weeks = WeekMask::from_weeks({1, 20});

    Course course;
    course.id = QStringLiteral("c1");
    course.name = QStringLiteral("超周课");
    course.sessions.append(session);

    const QList<Conflict> conflicts = ConflictDetector::detect_in_course(course, semester, periods);
    QCOMPARE(count_of_type(conflicts, Conflict::Type::InvalidWeek), 1);
}

void TestConflictDetector::detects_duplicate_course() {
    const Semester semester = make_semester();
    const QList<TimeSlot> periods = TimeSlot::default_slots();

    Course first = make_course(QStringLiteral("c1"), QStringLiteral("大学英语"), 1, 1, 2, QStringLiteral("1-16"));
    first.code = QStringLiteral("ENG101");
    Course second = make_course(QStringLiteral("c2"), QStringLiteral("大学英语"), 4, 5, 2, QStringLiteral("1-16"));
    second.code = QStringLiteral("ENG102");

    const QList<Conflict> conflicts = ConflictDetector::detect_between(first, second, semester);
    QCOMPARE(count_of_type(conflicts, Conflict::Type::DuplicateCourse), 1);
    // 重复课程只是提示，不应阻断用户继续使用
    QVERIFY(!conflicts.first().is_blocking());
    QVERIFY(!ConflictDetector::has_blocking_conflict(conflicts));

    // 相同课程代码同样命中
    Course third = make_course(QStringLiteral("c3"), QStringLiteral("其它课"), 4, 5, 2, QStringLiteral("1-16"));
    third.code = QStringLiteral("ENG101");
    QCOMPARE(count_of_type(ConflictDetector::detect_between(first, third, semester), Conflict::Type::DuplicateCourse), 1);
}

void TestConflictDetector::returns_empty_for_clean_schedule() {
    const Semester semester = make_semester();
    const QList<TimeSlot> periods = TimeSlot::default_slots();

    QList<Course> courses;
    courses.append(make_course(QStringLiteral("c1"), QStringLiteral("高等数学"), 1, 1, 2, QStringLiteral("1-16")));
    courses.append(make_course(QStringLiteral("c2"), QStringLiteral("大学物理"), 2, 3, 2, QStringLiteral("1-16")));
    courses.append(make_course(QStringLiteral("c3"), QStringLiteral("程序设计"), 4, 5, 2, QStringLiteral("1-8")));

    QVERIFY(ConflictDetector::detect(courses, semester, periods).isEmpty());
    QVERIFY(!ConflictDetector::has_blocking_conflict(QList<Conflict>()));
}

void TestConflictDetector::produces_stable_order() {
    const Semester semester = make_semester();
    const QList<TimeSlot> periods = TimeSlot::default_slots();

    QList<Course> courses;
    // 周一两条互相冲突；周三两条互相冲突。用于验证排序键“星期升序”稳定生效。
    courses.append(make_course(QStringLiteral("c1"), QStringLiteral("A"), 3, 1, 2, QStringLiteral("1-16")));
    courses.append(make_course(QStringLiteral("c2"), QStringLiteral("B"), 1, 1, 2, QStringLiteral("1-16")));
    courses.append(make_course(QStringLiteral("c3"), QStringLiteral("C"), 1, 2, 2, QStringLiteral("1-16")));
    courses.append(make_course(QStringLiteral("c4"), QStringLiteral("D"), 3, 2, 2, QStringLiteral("1-16")));

    const QList<Conflict> first_run = ConflictDetector::detect(courses, semester, periods);
    const QList<Conflict> second_run = ConflictDetector::detect(courses, semester, periods);

    QCOMPARE(first_run.size(), second_run.size());
    QVERIFY(!first_run.isEmpty());

    // 排序键：星期升序 → 周一的两条冲突排在周三之前
    QCOMPARE(first_run.first().day_of_week, 1);
    QCOMPARE(first_run.last().day_of_week, 3);

    for (int i = 0; i < first_run.size(); ++i) {
        QCOMPARE(first_run.at(i).type, second_run.at(i).type);
        QCOMPARE(first_run.at(i).message, second_run.at(i).message);
    }

    // 周一 1 条 + 周三 1 条，共 2 条时间冲突
    QCOMPARE(count_of_type(first_run, Conflict::Type::TimeOverlap), 2);
}

QTEST_GUILESS_MAIN(TestConflictDetector)

#include "tst_conflict_detector.moc"
