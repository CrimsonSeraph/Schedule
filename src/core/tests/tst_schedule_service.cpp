#include "core/model/ScheduleSnapshot.h"
#include "core/model/Semester.h"
#include "core/model/TimeSlot.h"
#include "core/service/ScheduleService.h"

#include <QSignalSpy>
#include <QTest>

using Schedule::Conflict;
using Schedule::Course;
using Schedule::CourseSession;
using Schedule::ScheduleService;
using Schedule::ScheduleSnapshot;
using Schedule::Semester;
using Schedule::TimeSlot;
using Schedule::WeekMask;

namespace {

    const QDate SEMESTER_START(2024, 9, 2); // 周一

    /** @return 2024-09-02 起、共 16 周的测试学期。 */
    Semester make_semester() {
        Semester semester = Semester::create(QStringLiteral("测试学期"), SEMESTER_START, 16);
        semester.is_current = true;
        return semester;
    }

    /** @return 一门课程的构造结果（未设置 id，交由服务补齐）。 */
    Course make_course(const QString& name, int day_of_week, int start_slot, int slot_count = 2, const QString& weeks = QStringLiteral("1-16")) {
        Course course;
        course.name = name;

        CourseSession session;
        session.day_of_week = day_of_week;
        session.start_slot = start_slot;
        session.slot_count = slot_count;
        session.weeks = WeekMask::from_expression(weeks, 16);
        course.sessions.append(session);
        return course;
    }

} // namespace

/**
 * @brief ScheduleService 的单元测试：学期 / 作息表 / 课程 CRUD / 查询 / 冲突 / 快照。
 */
class TestScheduleService : public QObject {
    Q_OBJECT

private slots:
    /** 构造函数后应处于“无学期”状态。 */
    void starts_empty();

    /** 设置学期：非法学期被拒绝并发出错误信号。 */
    void rejects_invalid_semester();

    /** 切换学期会清空课程，避免串数据。 */
    void clears_courses_when_semester_switches();

    /** 新增课程：自动补齐 id / 颜色 / 归属学期，并发出信号。 */
    void adds_course_with_defaults();

    /** 非法课程被拒绝。 */
    void rejects_invalid_course();

    /** 更新与删除课程。 */
    void updates_and_removes_course();

    /** 按周与星期查询排布，顺序稳定。 */
    void queries_sessions_by_week_and_day();

    /** 单双周课程只出现在对应周次。 */
    void honours_week_mask_when_querying();

    /** 按日期查询课程。 */
    void queries_sessions_by_date();

    /** 当前周计算。 */
    void computes_current_week();

    /** 冲突检测返回两门课之间的时间冲突。 */
    void detects_conflicts();

    /** 候选课程冲突检测（编辑器实时提示）。 */
    void detects_conflicts_for_candidate();

    /** 快照往返不丢数据。 */
    void round_trips_snapshot();

    /** 作息表为空时回退为默认作息。 */
    void falls_back_to_default_time_slots();
};

void TestScheduleService::starts_empty() {
    ScheduleService service;

    QVERIFY(!service.has_semester());
    QCOMPARE(service.course_count(), 0);
    QVERIFY(service.courses().isEmpty());
    QCOMPARE(service.total_weeks(), 0);
    QCOMPARE(service.max_slot_index(), 0);

    // 未设置学期时不允许添加课程
    QString error;
    QVERIFY(!service.add_course(make_course(QStringLiteral("高数"), 1, 1), &error));
    QVERIFY(!error.isEmpty());
}

void TestScheduleService::rejects_invalid_semester() {
    ScheduleService service;
    QSignalSpy error_spy(&service, &ScheduleService::error_occurred);

    Semester invalid;
    invalid.name = QStringLiteral("缺少起始日期");

    QVERIFY(!service.set_semester(invalid));
    QVERIFY(!service.has_semester());
    QCOMPARE(error_spy.count(), 1);
}

void TestScheduleService::clears_courses_when_semester_switches() {
    ScheduleService service;
    QVERIFY(service.set_semester(make_semester()));
    QVERIFY(service.add_course(make_course(QStringLiteral("高等数学"), 1, 1)));
    QCOMPARE(service.course_count(), 1);

    // 同一学期只改元数据：课程保留
    Semester renamed = service.semester();
    renamed.name = QStringLiteral("测试学期（改名）");
    QVERIFY(service.set_semester(renamed));
    QCOMPARE(service.course_count(), 1);

    // 换成另一个学期：课程清空
    QSignalSpy courses_spy(&service, &ScheduleService::courses_changed);
    QVERIFY(service.set_semester(Semester::create(QStringLiteral("新学期"), QDate(2025, 2, 17), 18)));
    QCOMPARE(service.course_count(), 0);
    QCOMPARE(courses_spy.count(), 1);
}

void TestScheduleService::adds_course_with_defaults() {
    ScheduleService service;
    QVERIFY(service.set_semester(make_semester()));
    service.set_time_slots(TimeSlot::default_slots());

    QSignalSpy courses_spy(&service, &ScheduleService::courses_changed);

    Course course = make_course(QStringLiteral("高等数学"), 1, 1);
    QString error;
    QVERIFY2(service.add_course(course, &error), qPrintable(error));
    QCOMPARE(courses_spy.count(), 1);

    Course stored;
    QVERIFY(service.find_course(service.courses().first().id, &stored));
    QVERIFY(!stored.id.isEmpty());
    QCOMPARE(stored.semester_id, service.semester().id);
    QVERIFY(!stored.color.isEmpty());
    QVERIFY(stored.color.startsWith(QLatin1Char('#')));
    QVERIFY(!stored.sessions.first().id.isEmpty());

    // 超过作息表范围的时间段被拒绝
    QVERIFY(!service.add_course(make_course(QStringLiteral("越界课"), 1, 12, 2), &error));
    QVERIFY(!error.isEmpty());
}

void TestScheduleService::rejects_invalid_course() {
    ScheduleService service;
    QVERIFY(service.set_semester(make_semester()));
    service.set_time_slots(TimeSlot::default_slots());

    Course nameless = make_course(QStringLiteral("临时"), 1, 1);
    nameless.name.clear();

    QString error;
    QVERIFY(!service.add_course(nameless, &error));
    QVERIFY(error.contains(QStringLiteral("课程名称")));
    QCOMPARE(service.course_count(), 0);

    // 重复 id 被拒绝
    QVERIFY(service.add_course(make_course(QStringLiteral("A"), 1, 1)));
    Course duplicate = make_course(QStringLiteral("B"), 2, 3);
    duplicate.id = service.courses().first().id;
    QVERIFY(!service.add_course(duplicate, &error));
    QCOMPARE(service.course_count(), 1);
}

void TestScheduleService::updates_and_removes_course() {
    ScheduleService service;
    QVERIFY(service.set_semester(make_semester()));
    service.set_time_slots(TimeSlot::default_slots());
    QVERIFY(service.add_course(make_course(QStringLiteral("高等数学"), 1, 1)));

    Course course = service.courses().first();
    course.name = QStringLiteral("高等数学 A");
    course.teacher = QStringLiteral("张老师");

    QString error;
    QVERIFY2(service.update_course(course, &error), qPrintable(error));
    QCOMPARE(service.courses().first().name, QStringLiteral("高等数学 A"));
    QCOMPARE(service.courses().first().teacher, QStringLiteral("张老师"));

    // 更新不存在的课程
    Course ghost = course;
    ghost.id = QStringLiteral("not-exist");
    QVERIFY(!service.update_course(ghost, &error));
    QVERIFY(!error.isEmpty());

    QVERIFY(service.remove_course(course.id));
    QCOMPARE(service.course_count(), 0);
    QVERIFY(!service.remove_course(course.id));
}

void TestScheduleService::queries_sessions_by_week_and_day() {
    ScheduleService service;
    QVERIFY(service.set_semester(make_semester()));
    service.set_time_slots(TimeSlot::default_slots());

    QVERIFY(service.add_course(make_course(QStringLiteral("高等数学"), 1, 3)));
    QVERIFY(service.add_course(make_course(QStringLiteral("大学物理"), 1, 1)));
    QVERIFY(service.add_course(make_course(QStringLiteral("程序设计"), 3, 1)));

    const QList<ScheduleService::PlacedSession> monday = service.sessions_at(1, 5);
    QCOMPARE(monday.size(), 2);
    // 按起始节次排序：大学物理（第 1 节）在前
    QCOMPARE(monday.at(0).course_name, QStringLiteral("大学物理"));
    QCOMPARE(monday.at(1).course_name, QStringLiteral("高等数学"));

    // 携带作息表信息，便于界面直接渲染时间
    QCOMPARE(monday.at(0).start_time(), QTime(8, 0));
    QCOMPARE(monday.at(0).end_time(), QTime(9, 40));
    QCOMPARE(monday.at(0).week, 5);
    QCOMPARE(monday.at(0).date, QDate(2024, 9, 30));

    // 越界参数返回空
    QVERIFY(service.sessions_at(0, 1).isEmpty());
    QVERIFY(service.sessions_at(8, 1).isEmpty());
    QVERIFY(service.sessions_at(1, 0).isEmpty());

    // 全周查询
    QCOMPARE(service.sessions_in_week(1).size(), 3);
}

void TestScheduleService::honours_week_mask_when_querying() {
    ScheduleService service;
    QVERIFY(service.set_semester(make_semester()));
    service.set_time_slots(TimeSlot::default_slots());

    QVERIFY(service.add_course(make_course(QStringLiteral("单周课"), 2, 1, 2, QStringLiteral("1-15/2"))));
    QVERIFY(service.add_course(make_course(QStringLiteral("双周课"), 2, 3, 2, QStringLiteral("2-16/2"))));

    QCOMPARE(service.sessions_at(2, 1).size(), 1);
    QCOMPARE(service.sessions_at(2, 1).first().course_name, QStringLiteral("单周课"));
    QCOMPARE(service.sessions_at(2, 2).size(), 1);
    QCOMPARE(service.sessions_at(2, 2).first().course_name, QStringLiteral("双周课"));
    QCOMPARE(service.sessions_at(2, 16).size(), 1);
    QVERIFY(service.sessions_at(2, 17).isEmpty());
}

void TestScheduleService::queries_sessions_by_date() {
    ScheduleService service;
    QVERIFY(service.set_semester(make_semester()));
    service.set_time_slots(TimeSlot::default_slots());
    QVERIFY(service.add_course(make_course(QStringLiteral("高等数学"), 1, 1)));

    // 2024-09-02 是第 1 周周一
    QCOMPARE(service.sessions_on_date(SEMESTER_START).size(), 1);
    // 2024-09-09 是第 2 周周一
    QCOMPARE(service.sessions_on_date(QDate(2024, 9, 9)).size(), 1);
    // 周二没有课
    QVERIFY(service.sessions_on_date(QDate(2024, 9, 3)).isEmpty());
    // 学期外
    QVERIFY(service.sessions_on_date(QDate(2025, 5, 1)).isEmpty());

    // 按课程查询时间段（week <= 0 表示不过滤周次）
    const QString course_id = service.courses().first().id;
    QCOMPARE(service.sessions_of_course(course_id, 0).size(), 1);
    QCOMPARE(service.sessions_of_course(course_id, 1).size(), 1);
    QCOMPARE(service.sessions_of_course(QStringLiteral("missing"), 0).size(), 0);
}

void TestScheduleService::computes_current_week() {
    ScheduleService service;
    QVERIFY(service.set_semester(make_semester()));

    QCOMPARE(service.current_week(SEMESTER_START), 1);
    QCOMPARE(service.current_week(SEMESTER_START.addDays(15)), 3);
    QCOMPARE(service.current_week(SEMESTER_START.addDays(-1)), 0);
    QCOMPARE(service.week_of(SEMESTER_START.addDays(7)), 2);
    QCOMPARE(service.week_start_date(2), SEMESTER_START.addDays(7));
}

void TestScheduleService::detects_conflicts() {
    ScheduleService service;
    QVERIFY(service.set_semester(make_semester()));
    service.set_time_slots(TimeSlot::default_slots());

    QVERIFY(service.add_course(make_course(QStringLiteral("高等数学"), 2, 1)));
    QVERIFY(service.add_course(make_course(QStringLiteral("线性代数"), 2, 2)));
    QVERIFY(service.add_course(make_course(QStringLiteral("大学物理"), 4, 1)));

    const QList<Conflict> conflicts = service.detect_conflicts();
    QCOMPARE(conflicts.size(), 1);
    QCOMPARE(conflicts.first().type, Conflict::Type::TimeOverlap);
    QVERIFY(conflicts.first().message.contains(QStringLiteral("周二")));
}

void TestScheduleService::detects_conflicts_for_candidate() {
    ScheduleService service;
    QVERIFY(service.set_semester(make_semester()));
    service.set_time_slots(TimeSlot::default_slots());
    QVERIFY(service.add_course(make_course(QStringLiteral("高等数学"), 2, 1)));

    // 与已有课程冲突的候选课程
    const Course conflicting = make_course(QStringLiteral("候选课"), 2, 2);
    QCOMPARE(service.detect_conflicts_for(conflicting).size(), 1);

    // 不冲突的候选课程
    const Course clean = make_course(QStringLiteral("候选课"), 3, 2);
    QVERIFY(service.detect_conflicts_for(clean).isEmpty());

    // 更新自身时不应把自己判为冲突
    Course existing = service.courses().first();
    existing.name = QStringLiteral("高等数学（改名）");
    QVERIFY(service.detect_conflicts_for(existing).isEmpty());
}

void TestScheduleService::round_trips_snapshot() {
    ScheduleService source;
    QVERIFY(source.set_semester(make_semester()));
    source.set_time_slots(TimeSlot::default_slots());
    QVERIFY(source.add_course(make_course(QStringLiteral("高等数学"), 1, 1)));
    QVERIFY(source.add_course(make_course(QStringLiteral("程序设计"), 4, 5, 2, QStringLiteral("1-8"))));

    const ScheduleSnapshot snapshot = source.snapshot();
    QVERIFY(snapshot.is_valid());
    QCOMPARE(snapshot.courses.size(), 2);
    QCOMPARE(snapshot.max_slot_index(), 11);
    QCOMPARE(snapshot.index_of_course(snapshot.courses.first().id), 0);

    Course found;
    QVERIFY(snapshot.find_course(snapshot.courses.last().id, &found));
    QCOMPARE(found.name, QStringLiteral("程序设计"));
    QVERIFY(!snapshot.find_course(QStringLiteral("missing"), &found));

    ScheduleService target;
    QSignalSpy semester_spy(&target, &ScheduleService::semester_changed);
    QSignalSpy courses_spy(&target, &ScheduleService::courses_changed);
    target.load_snapshot(snapshot);

    QCOMPARE(semester_spy.count(), 1);
    QCOMPARE(courses_spy.count(), 1);
    QCOMPARE(target.semester().id, source.semester().id);
    QCOMPARE(target.course_count(), 2);
    QCOMPARE(target.time_slots().size(), 11);
    QCOMPARE(target.snapshot().courses, snapshot.courses);

    target.clear_courses();
    QCOMPARE(target.course_count(), 0);
    QVERIFY(target.time_slots().isEmpty());
    QVERIFY(target.has_semester()); // 学期元数据保留
}

void TestScheduleService::falls_back_to_default_time_slots() {
    ScheduleService service;

    ScheduleSnapshot snapshot;
    snapshot.semester = make_semester();
    snapshot.courses.append(make_course(QStringLiteral("高等数学"), 1, 1));
    // 故意不提供作息表

    service.load_snapshot(snapshot);

    QCOMPARE(service.time_slots().size(), TimeSlot::default_slots().size());
    QCOMPARE(service.max_slot_index(), 11);
    QCOMPARE(service.sessions_at(1, 1).first().start_time(), QTime(8, 0));
}

QTEST_GUILESS_MAIN(TestScheduleService)

#include "tst_schedule_service.moc"
