#include "core/model/Course.h"
#include "core/model/ScheduleSnapshot.h"
#include "core/model/Semester.h"
#include "core/model/TimeSlot.h"
#include "data/ScheduleJson.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

using Schedule::Course;
using Schedule::CourseSession;
using Schedule::ScheduleJson;
using Schedule::ScheduleSnapshot;
using Schedule::Semester;
using Schedule::TimeSlot;
using Schedule::WeekMask;

namespace {

    const QDate SEMESTER_START(2024, 9, 2); // 周一

    /** @return 一个包含作息表与两门课程的测试快照。 */
    ScheduleSnapshot make_snapshot() {
        ScheduleSnapshot snapshot;
        snapshot.semester = Semester::create(QStringLiteral("2024-2025 学年第一学期"), SEMESTER_START, 16);
        snapshot.semester.is_current = true;
        snapshot.time_slots = TimeSlot::default_slots();

        Course math;
        math.id = QStringLiteral("course-math");
        math.semester_id = snapshot.semester.id;
        math.name = QStringLiteral("高等数学 A");
        math.code = QStringLiteral("MATH101");
        math.teacher = QStringLiteral("张老师");
        math.location = QStringLiteral("教一 101");
        math.color = QStringLiteral("#4C8DFF");
        math.credits = 4.0;
        math.notes = QStringLiteral("需带教材");

        CourseSession lecture;
        lecture.id = QStringLiteral("session-1");
        lecture.day_of_week = 1;
        lecture.start_slot = 1;
        lecture.slot_count = 2;
        lecture.weeks = WeekMask::from_expression(QStringLiteral("1-16"), 16);
        math.sessions.append(lecture);

        CourseSession lab;
        lab.id = QStringLiteral("session-2");
        lab.day_of_week = 3;
        lab.start_slot = 5;
        lab.slot_count = 2;
        lab.weeks = WeekMask::from_expression(QStringLiteral("1-15/2"), 16);
        lab.location = QStringLiteral("实验楼 302");
        lab.teacher = QStringLiteral("李老师");
        math.sessions.append(lab);

        Course english;
        english.id = QStringLiteral("course-english");
        english.semester_id = snapshot.semester.id;
        english.name = QStringLiteral("大学英语");
        english.sessions.append(lecture);
        english.sessions.first().id = QStringLiteral("session-3");

        snapshot.courses.append(math);
        snapshot.courses.append(english);
        return snapshot;
    }

} // namespace

/**
 * @brief JSON 序列化 / 反序列化与文件读写的单元测试。
 */
class TestScheduleJson : public QObject {
    Q_OBJECT

private slots:
    /** 快照 → 文档 → 快照 往返不丢数据。 */
    void round_trips_snapshot();

    /** 文档带 format / version 信封字段。 */
    void writes_document_envelope();

    /** 拒绝非课表 JSON 与过高版本。 */
    void rejects_foreign_document();

    /** `week_bits` 缺失时回退解析 `weeks` 表达式。 */
    void falls_back_to_week_expression();

    /** 兼容 `start_time` / `end_time` 命名与缺省作息表。 */
    void accepts_alternative_field_names();

    /** 学期起始日期缺失时报错。 */
    void rejects_semester_without_date();

    /** 文件读写：自动创建父目录。 */
    void writes_and_reads_file();

    /** 读取不存在的文件返回错误。 */
    void reports_missing_file();
};

void TestScheduleJson::round_trips_snapshot() {
    const ScheduleSnapshot source = make_snapshot();

    const QByteArray document = ScheduleJson::to_document(source);
    QVERIFY(!document.isEmpty());

    ScheduleSnapshot restored;
    QString error;
    QVERIFY2(ScheduleJson::from_document(document, &restored, &error), qPrintable(error));
    QVERIFY(error.isEmpty());

    QCOMPARE(restored.semester.id, source.semester.id);
    QCOMPARE(restored.semester.name, source.semester.name);
    QCOMPARE(restored.semester.start_date, SEMESTER_START);
    QCOMPARE(restored.semester.total_weeks, 16);
    QVERIFY(restored.semester.is_current);
    QCOMPARE(restored.time_slots.size(), source.time_slots.size());
    QCOMPARE(restored.courses.size(), 2);

    const Course& math = restored.courses.first();
    QCOMPARE(math.name, QStringLiteral("高等数学 A"));
    QCOMPARE(math.code, QStringLiteral("MATH101"));
    QCOMPARE(math.credits, 4.0);
    QCOMPARE(math.sessions.size(), 2);
    QCOMPARE(math.sessions.at(0).weeks, WeekMask::from_expression(QStringLiteral("1-16"), 16));
    QCOMPARE(math.sessions.at(1).weeks, WeekMask::from_expression(QStringLiteral("1-15/2"), 16));
    QCOMPARE(math.sessions.at(1).location, QStringLiteral("实验楼 302"));
    QCOMPARE(math.sessions.at(1).teacher, QStringLiteral("李老师"));
}

void TestScheduleJson::writes_document_envelope() {
    const QByteArray document = ScheduleJson::to_document(make_snapshot());
    const QJsonObject root = QJsonDocument::fromJson(document).object();

    QCOMPARE(root.value(QStringLiteral("format")).toString(), ScheduleJson::format_name());
    QCOMPARE(root.value(QStringLiteral("version")).toInt(), ScheduleJson::FORMAT_VERSION);
    QVERIFY(!root.value(QStringLiteral("exported_at")).toString().isEmpty());
    QVERIFY(root.value(QStringLiteral("semester")).isObject());
    QVERIFY(root.value(QStringLiteral("courses")).isArray());
}

void TestScheduleJson::rejects_foreign_document() {
    QString error;

    ScheduleSnapshot snapshot;
    QVERIFY(!ScheduleJson::from_document(QByteArray("not json"), &snapshot, &error));
    QVERIFY(!error.isEmpty());

    QVERIFY(!ScheduleJson::from_document(QByteArray("[1,2,3]"), &snapshot, &error));
    QVERIFY(!error.isEmpty());

    QJsonObject foreign;
    foreign.insert(QStringLiteral("format"), QStringLiteral("some-other-app"));
    QVERIFY(!ScheduleJson::from_document(QJsonDocument(foreign).toJson(), &snapshot, &error));
    QVERIFY(error.contains(QStringLiteral("课表")));

    QJsonObject future = ScheduleJson::to_json(make_snapshot());
    future.insert(QStringLiteral("format"), ScheduleJson::format_name());
    future.insert(QStringLiteral("version"), ScheduleJson::FORMAT_VERSION + 1);
    QVERIFY(!ScheduleJson::from_document(QJsonDocument(future).toJson(), &snapshot, &error));
    QVERIFY(error.contains(QStringLiteral("版本")));
}

void TestScheduleJson::falls_back_to_week_expression() {
    QJsonObject course = ScheduleJson::to_json(make_snapshot().courses.first());
    QJsonArray sessions = course.value(QStringLiteral("sessions")).toArray();
    QJsonObject session = sessions.at(0).toObject();
    session.remove(QStringLiteral("week_bits")); // 模拟手工编辑 / 第三方生成的文件
    sessions.replace(0, session);
    course.insert(QStringLiteral("sessions"), sessions);

    Course restored;
    QString error;
    QVERIFY2(ScheduleJson::course_from_json(course, &restored, &error), qPrintable(error));
    QCOMPARE(restored.sessions.first().weeks, WeekMask::from_expression(QStringLiteral("1-16"), 16));
}

void TestScheduleJson::accepts_alternative_field_names() {
    QJsonObject slot;
    slot.insert(QStringLiteral("index"), 1);
    slot.insert(QStringLiteral("start_time"), QStringLiteral("08:00"));
    slot.insert(QStringLiteral("end_time"), QStringLiteral("08:45"));

    TimeSlot restored;
    QString error;
    QVERIFY2(ScheduleJson::time_slot_from_json(slot, &restored, &error), qPrintable(error));
    QCOMPARE(restored.index, 1);
    QCOMPARE(restored.start_time, QTime(8, 0));
    QCOMPARE(restored.end_time, QTime(8, 45));
    QCOMPARE(restored.display_label(), QStringLiteral("第 1 节"));

    // 缺少作息表时回退为默认作息，保证导入结果仍可渲染
    QJsonObject root = ScheduleJson::to_json(make_snapshot());
    root.remove(QStringLiteral("time_slots"));
    root.insert(QStringLiteral("format"), ScheduleJson::format_name());
    root.insert(QStringLiteral("version"), ScheduleJson::FORMAT_VERSION);

    ScheduleSnapshot snapshot;
    QVERIFY2(ScheduleJson::snapshot_from_json(root, &snapshot, &error), qPrintable(error));
    QCOMPARE(snapshot.time_slots.size(), TimeSlot::default_slots().size());
}

void TestScheduleJson::rejects_semester_without_date() {
    QJsonObject root = ScheduleJson::to_json(make_snapshot());
    QJsonObject semester = root.value(QStringLiteral("semester")).toObject();
    semester.remove(QStringLiteral("start_date"));
    root.insert(QStringLiteral("semester"), semester);

    ScheduleSnapshot snapshot;
    QString error;
    QVERIFY(!ScheduleJson::snapshot_from_json(root, &snapshot, &error));
    QVERIFY(error.contains(QStringLiteral("起始日期")));
}

void TestScheduleJson::writes_and_reads_file() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString path = directory.filePath(QStringLiteral("nested/dir/schedule.json"));
    QString error;
    QVERIFY2(ScheduleJson::write_file(path, make_snapshot(), &error), qPrintable(error));
    QVERIFY(QFile::exists(path));

    ScheduleSnapshot restored;
    QVERIFY2(ScheduleJson::read_file(path, &restored, &error), qPrintable(error));
    QCOMPARE(restored.courses.size(), 2);

    // 覆盖写入应当是原子替换，仍然可读
    QVERIFY(ScheduleJson::write_file(path, make_snapshot(), &error));
    QVERIFY(ScheduleJson::read_file(path, &restored, &error));

    QVERIFY(!ScheduleJson::write_file(QString(), make_snapshot(), &error));
    QVERIFY(!error.isEmpty());
}

void TestScheduleJson::reports_missing_file() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ScheduleSnapshot snapshot;
    QString error;
    QVERIFY(!ScheduleJson::read_file(directory.filePath(QStringLiteral("nope.json")), &snapshot, &error));
    QVERIFY(error.contains(QStringLiteral("不存在")));
}

QTEST_GUILESS_MAIN(TestScheduleJson)

#include "tst_schedule_json.moc"
