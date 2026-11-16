#include "core/model/Course.h"
#include "core/model/Semester.h"
#include "core/model/WeekMask.h"
#include "core/service/ScheduleService.h"
#include "engine/SessionListModel.h"

#include <QTest>

using Schedule::Course;
using Schedule::CourseSession;
using Schedule::ScheduleService;
using Schedule::Semester;
using Schedule::SessionListModel;
using Schedule::WeekMask;

namespace {

    /** @return 2024-09-02 起、共 16 周的测试学期。 */
    Semester make_semester() {
        Semester semester = Semester::create(QStringLiteral("测试学期"), QDate(2024, 9, 2), 16);
        semester.is_current = true;
        return semester;
    }

    /** @return 周一第 1-2 节、指定周次的课程。 */
    Course make_course(const QString& name, const QString& weeks) {
        Course course;
        course.name = name;

        CourseSession session;
        session.day_of_week = 1;
        session.start_slot = 1;
        session.slot_count = 2;
        session.weeks = WeekMask::from_expression(weeks, 16);
        course.sessions.append(session);
        return course;
    }

} // namespace

/**
 * @brief SessionListModel 的单元测试：非本周幽灵卡的包含/排序/activeThisWeek 标记。
 */
class TestSessionModel : public QObject {
    Q_OBJECT

private slots:
    /** 默认只显示本周；开启后把非本周课程排在前面并标记为幽灵卡。 */
    void includes_ghost_sessions();

    /** 本周课程排在幽灵卡之后，保证绘制时盖在幽灵卡上层。 */
    void active_cards_render_after_ghosts();
};

void TestSessionModel::includes_ghost_sessions() {
    ScheduleService service;
    QVERIFY(service.set_semester(make_semester()));

    QString error;
    QVERIFY2(service.add_course(make_course(QStringLiteral("只在第 1 周"), QStringLiteral("1")), &error), qPrintable(error));

    SessionListModel model;
    model.set_service(&service);
    model.set_week(2);

    QCOMPARE(model.count(), 0);

    model.set_include_other_weeks(true);
    QCOMPARE(model.count(), 1);
    QCOMPARE(model.data(model.index(0), SessionListModel::ActiveThisWeekRole).toBool(), false);
    QCOMPARE(model.data(model.index(0), SessionListModel::CourseNameRole).toString(), QStringLiteral("只在第 1 周"));

    model.set_week(1);
    QCOMPARE(model.count(), 1);
    QCOMPARE(model.data(model.index(0), SessionListModel::ActiveThisWeekRole).toBool(), true);

    model.set_include_other_weeks(false);
    QCOMPARE(model.count(), 1);
}

void TestSessionModel::active_cards_render_after_ghosts() {
    ScheduleService service;
    QVERIFY(service.set_semester(make_semester()));

    QString error;
    QVERIFY2(service.add_course(make_course(QStringLiteral("幽灵课"), QStringLiteral("1")), &error), qPrintable(error));
    QVERIFY2(service.add_course(make_course(QStringLiteral("本周课"), QStringLiteral("1-16")), &error), qPrintable(error));

    SessionListModel model;
    model.set_service(&service);
    model.set_week(2);
    model.set_include_other_weeks(true);

    QCOMPARE(model.count(), 2);
    QCOMPARE(model.data(model.index(0), SessionListModel::ActiveThisWeekRole).toBool(), false);
    QCOMPARE(model.data(model.index(1), SessionListModel::ActiveThisWeekRole).toBool(), true);
    QCOMPARE(model.data(model.index(0), SessionListModel::CourseNameRole).toString(), QStringLiteral("幽灵课"));
    QCOMPARE(model.data(model.index(1), SessionListModel::CourseNameRole).toString(), QStringLiteral("本周课"));
}

QTEST_GUILESS_MAIN(TestSessionModel)
#include "tst_session_model.moc"