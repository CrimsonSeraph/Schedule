#include "core/model/Course.h"
#include "core/model/ScheduleSnapshot.h"
#include "data/import_export/CsvScheduleIo.h"
#include "data/import_export/IcsScheduleIo.h"
#include "data/import_export/ImportExportTypes.h"
#include "data/import_export/JsonScheduleIo.h"

#include <QDir>
#include <QFile>
#include <QTest>

using Schedule::Course;
using Schedule::CourseSession;
using Schedule::CsvScheduleIo;
using Schedule::IcsScheduleIo;
using Schedule::JsonScheduleIo;
using Schedule::ScheduleSnapshot;
using Schedule::WeekMask;

namespace {

    /** @return 样本文件目录（由 CMake 通过编译定义注入）。 */
    QString samples_dir() {
        return QStringLiteral(SCHEDULE_SAMPLES_DIR);
    }

    /** @return 样本文件的绝对路径。 */
    QString sample_path(const QString& file_name) {
        return QDir(samples_dir()).filePath(file_name);
    }

    /**
     * @brief 把课程的时间段规整成可比较的字符串集合。
     *
     * CSV / ICS 不含课程 id，且 ICS 的作息表是按出现时间合成的，
     * 因此比较“星期 + 起始节次 + 连续节数 + 周次表达式”即可。
     */
    QStringList session_signatures(const Course& course) {
        QStringList signatures;
        for (const CourseSession& session : course.sessions) {
            signatures.append(QStringLiteral("%1|%2|%3|%4")
                    .arg(session.day_of_week)
                    .arg(session.start_slot)
                    .arg(session.slot_count)
                    .arg(session.weeks.to_expression()));
        }
        signatures.sort();
        return signatures;
    }

    /** @return 课程名 → 时间段签名集合。 */
    QMap<QString, QStringList> signature_map(const QList<Course>& courses) {
        QMap<QString, QStringList> map;
        for (const Course& course : courses) {
            map.insert(course.name, session_signatures(course));
        }
        return map;
    }

} // namespace

/**
 * @brief 样本文件测试：三个格式必须解析出**内容等价**的课表。
 *
 * 这条测试同时充当“格式映射的回归测试”：任何一侧的映射被改坏，
 * 三个格式之间的一致性就会被打破。
 */
class TestSampleFiles : public QObject {
    Q_OBJECT

private slots:
    /** 样本文件存在且非空。 */
    void sample_files_exist();

    /** JSON 样本可解析且内容符合预期。 */
    void parses_json_sample();

    /** CSV 样本可解析且与 JSON 内容等价。 */
    void parses_csv_sample();

    /** ICS 样本可解析且与 JSON 内容等价。 */
    void parses_ics_sample();

    /** 三种格式解析出的课程集合完全一致。 */
    void formats_agree_with_each_other();
};

void TestSampleFiles::sample_files_exist() {
    const QStringList files = {QStringLiteral("schedule_sample.json"),
        QStringLiteral("schedule_sample.csv"),
        QStringLiteral("schedule_sample.ics")};
    for (const QString& file_name : files) {
        const QString path = sample_path(file_name);
        QVERIFY2(QFile::exists(path), qPrintable(path));
        QVERIFY2(QFileInfo(path).size() > 0, qPrintable(path));
    }
}

void TestSampleFiles::parses_json_sample() {
    JsonScheduleIo io;
    ScheduleSnapshot snapshot;
    QString error;

    QVERIFY2(io.parse(sample_path(QStringLiteral("schedule_sample.json")), &snapshot, &error), qPrintable(error));
    QCOMPARE(snapshot.semester.name, QStringLiteral("2024-2025 学年第一学期"));
    QCOMPARE(snapshot.semester.start_date, QDate(2024, 9, 2));
    QCOMPARE(snapshot.semester.total_weeks, 16);
    QCOMPARE(snapshot.time_slots.size(), 11);
    QCOMPARE(snapshot.courses.size(), 4);
    QVERIFY(snapshot.is_valid(&error));
    QVERIFY2(snapshot.detect_conflicts().isEmpty(), "样本课表本身不应包含冲突");

    // 单双周与非连续周次是两种最易写错的形态，单独断言
    QMap<QString, QStringList> signatures = signature_map(snapshot.courses);
    QVERIFY(signatures.value(QStringLiteral("大学物理")).contains(QStringLiteral("4|5|2|1-15/2")));
    QVERIFY(signatures.value(QStringLiteral("程序设计基础")).contains(QStringLiteral("5|7|2|1-4,6,9-10")));
    QCOMPARE(signatures.value(QStringLiteral("高等数学 A")).size(), 2);
}

void TestSampleFiles::parses_csv_sample() {
    CsvScheduleIo io;
    ScheduleSnapshot snapshot;
    QString error;

    QVERIFY2(io.parse(sample_path(QStringLiteral("schedule_sample.csv")), &snapshot, &error), qPrintable(error));
    QCOMPARE(snapshot.courses.size(), 4);

    QMap<QString, QStringList> signatures = signature_map(snapshot.courses);
    QCOMPARE(signatures.value(QStringLiteral("高等数学 A")),
        QStringList({QStringLiteral("1|1|2|1-16"), QStringLiteral("3|3|2|1-16")}));
    QCOMPARE(signatures.value(QStringLiteral("大学英语")), QStringList({QStringLiteral("2|3|2|1-16")}));
    QCOMPARE(signatures.value(QStringLiteral("大学物理")), QStringList({QStringLiteral("4|5|2|1-15/2")}));
    QCOMPARE(signatures.value(QStringLiteral("程序设计基础")), QStringList({QStringLiteral("5|7|2|1-4,6,9-10")}));

    // CSV 含时间列，应据此合成作息表（样本用到的节次为 1/3/5/7）
    QCOMPARE(snapshot.time_slots.size(), 7);
    QCOMPARE(snapshot.time_slots.first().start_time, QTime(8, 0));
}

void TestSampleFiles::parses_ics_sample() {
    IcsScheduleIo io;
    ScheduleSnapshot snapshot;
    QString error;

    QVERIFY2(io.parse(sample_path(QStringLiteral("schedule_sample.ics")), &snapshot, &error), qPrintable(error));
    QCOMPARE(snapshot.courses.size(), 4);
    QCOMPARE(snapshot.semester.name, QStringLiteral("2024-2025 学年第一学期"));

    QMap<QString, QStringList> signatures = signature_map(snapshot.courses);
    QCOMPARE(signatures.value(QStringLiteral("高等数学 A")),
        QStringList({QStringLiteral("1|1|2|1-16"), QStringLiteral("3|3|2|1-16")}));
    QCOMPARE(signatures.value(QStringLiteral("大学英语")), QStringList({QStringLiteral("2|3|2|1-16")}));
    // RRULE + INTERVAL=2 → 单周
    QCOMPARE(signatures.value(QStringLiteral("大学物理")), QStringList({QStringLiteral("4|5|2|1-15/2")}));
    // 非等差周次走 RDATE 分支
    QCOMPARE(signatures.value(QStringLiteral("程序设计基础")), QStringList({QStringLiteral("5|7|2|1-4,6,9-10")}));
}

void TestSampleFiles::formats_agree_with_each_other() {
    JsonScheduleIo json_io;
    CsvScheduleIo csv_io;
    IcsScheduleIo ics_io;

    ScheduleSnapshot from_json;
    ScheduleSnapshot from_csv;
    ScheduleSnapshot from_ics;
    QString error;

    QVERIFY2(json_io.parse(sample_path(QStringLiteral("schedule_sample.json")), &from_json, &error), qPrintable(error));
    QVERIFY2(csv_io.parse(sample_path(QStringLiteral("schedule_sample.csv")), &from_csv, &error), qPrintable(error));
    QVERIFY2(ics_io.parse(sample_path(QStringLiteral("schedule_sample.ics")), &from_ics, &error), qPrintable(error));

    const QMap<QString, QStringList> json_signatures = signature_map(from_json.courses);
    QCOMPARE(signature_map(from_csv.courses), json_signatures);
    QCOMPARE(signature_map(from_ics.courses), json_signatures);

    // 课程级属性（教师 / 地点 / 学分）在三种格式间也应一致
    for (const Course& course : from_json.courses) {
        Course csv_course;
        Course ics_course;
        for (const Course& candidate : from_csv.courses) {
            if (candidate.name == course.name) {
                csv_course = candidate;
            }
        }
        for (const Course& candidate : from_ics.courses) {
            if (candidate.name == course.name) {
                ics_course = candidate;
            }
        }
        QCOMPARE(csv_course.teacher, course.teacher);
        QCOMPARE(csv_course.location, course.location);
        QCOMPARE(csv_course.code, course.code);
        QCOMPARE(ics_course.teacher, course.teacher);
        QCOMPARE(ics_course.location, course.location);
    }
}

QTEST_GUILESS_MAIN(TestSampleFiles)

#include "tst_sample_files.moc"
