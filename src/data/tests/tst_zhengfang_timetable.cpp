#include "core/model/Course.h"
#include "core/model/CourseSession.h"
#include "core/model/ScheduleSnapshot.h"
#include "core/model/WeekMask.h"
#include "data/import_export/ImportExportTypes.h"
#include "data/import_export/ImportManager.h"
#include "data/import_export/academic_affairs/CharsetUtil.h"
#include "data/import_export/academic_affairs/ZhengfangTimetableIo.h"

#include <QDir>
#include <QFile>
#include <QTest>

using Schedule::Course;
using Schedule::CourseSession;
using Schedule::ImportManager;
using Schedule::ImportPreview;
using Schedule::ScheduleFormat;
using Schedule::ScheduleSnapshot;
using Schedule::WeekMask;
using Schedule::ZhengfangTimetableIo;

namespace {

    /** @return 样本文件目录（由 CMake 通过编译定义注入）。 */
    QString samples_dir() {
        return QStringLiteral(SCHEDULE_SAMPLES_DIR);
    }

    /** @return 正方教务样本（GBK 编码的 HTML+JS）的绝对路径。 */
    QString sample_path() {
        return QDir(samples_dir()).filePath(QStringLiteral("schedule_sample_zhengfang.xls"));
    }

    /** @return 按课程名查找课程；未找到返回 false。 */
    bool find_course(const QList<Course>& courses, const QString& name, Course* out_course) {
        for (const Course& course : courses) {
            if (course.name == name) {
                if (out_course) {
                    *out_course = course;
                }
                return true;
            }
        }
        return false;
    }

    /** @return 时间段签名 `星期|起始节|节数|周次列表`，按字符串排序后便于比较。 */
    QStringList session_signatures(const Course& course) {
        QStringList signatures;
        for (const CourseSession& session : course.sessions) {
            QStringList weeks;
            for (const int week : session.weeks.weeks()) {
                weeks.append(QString::number(week));
            }
            signatures.append(QStringLiteral("%1|%2|%3|%4")
                    .arg(session.day_of_week)
                    .arg(session.start_slot)
                    .arg(session.slot_count)
                    .arg(weeks.join(QLatin1Char(','))));
        }
        signatures.sort();
        return signatures;
    }

} // namespace

/**
 * @brief 正方教务课表导入器测试。
 *
 * 样本是**虚构数据**（见 `tools/gen_zhengfang_sample.py`），但结构与教务系统
 * 导出的真实页面一致：GBK 编码、`<table id="manualArrangeCourseTable">` 外壳、
 * 内嵌 `new TaskActivity(...)` 脚本。
 *
 * 覆盖的关键行为：
 *  - GBK 解码（课程名 / 教师名 / 教室名是中文，解码错了就会直接失败）；
 *  - 格式嗅探与扩展名映射（`.xls` 实际是 HTML）；
 *  - 「一个活动挂多个连续节次」需要合并；
 *  - 「同一活动挂在同一天不连续节次」需要拆分；
 *  - 「同一门课的多个活动」需要合并为一门课程的多段时间段；
 *  - 周次位图还原为实际周次列表。
 */
class TestZhengfangTimetable : public QObject {
    Q_OBJECT

private slots:
    /** 样本文件存在且非空。 */
    void sample_file_exists();

    /** 内容嗅探与扩展名映射都能识别正方教务页面。 */
    void sniffs_zhengfang_format();

    /** GBK 字节串能被正确解码为中文。 */
    void decodes_gbk_chinese();

    /** 学期信息与作息表按页面内容合成。 */
    void parses_semester_and_time_slots();

    /** 课程按代码聚合，名称经 GBK 解码后正确。 */
    void parses_courses();

    /** 连续节次合并为一个时间段。 */
    void merges_consecutive_units();

    /** 同一天内不连续的节次拆分为多个时间段。 */
    void splits_non_consecutive_units();

    /** 同一门课的多个活动合并为一门课程。 */
    void merges_activities_of_same_course();

    /** 周次位图还原为实际周次列表。 */
    void parses_week_bitmap();

    /** 教室与教师按时间段覆盖课程级默认值。 */
    void keeps_teacher_and_room();

    /** 非正方页面被拒绝。 */
    void rejects_non_zhengfang_content();

    /** 只导入格式不出现在导出过滤器里。 */
    void export_filter_excludes_zhengfang();

    /** 导入器已注册，经 ImportManager 走通「预览」流程。 */
    void import_manager_previews_sample();
};

void TestZhengfangTimetable::sample_file_exists() {
    const QString path = sample_path();
    QVERIFY2(QFile::exists(path), qPrintable(path));
    QVERIFY2(QFileInfo(path).size() > 0, qPrintable(path));
}

void TestZhengfangTimetable::sniffs_zhengfang_format() {
    QFile file(sample_path());
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QByteArray data = file.readAll();
    file.close();

    QCOMPARE(Schedule::format_from_content(data), ScheduleFormat::ZhengfangHtml);
    QCOMPARE(Schedule::format_from_extension(QStringLiteral("/tmp/课表.xls")), ScheduleFormat::ZhengfangHtml);
    QCOMPARE(Schedule::format_from_extension(QStringLiteral("timetable.html")), ScheduleFormat::ZhengfangHtml);
    QCOMPARE(Schedule::format_from_string(QStringLiteral("zhengfang-html")), ScheduleFormat::ZhengfangHtml);

    ZhengfangTimetableIo io;
    QString error;
    QVERIFY2(io.can_import(sample_path(), &error), qPrintable(error));
    QCOMPARE(io.format(), ScheduleFormat::ZhengfangHtml);
    QVERIFY(io.extensions().contains(QStringLiteral(".xls")));
}

void TestZhengfangTimetable::decodes_gbk_chinese() {
    // 样本以 GBK 落盘，直接按 UTF-8 读会得到替换字符，必须走 GBK 解码
    QFile file(sample_path());
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QByteArray data = file.readAll();
    file.close();

    QVERIFY(Schedule::looks_like_utf8(data) == false);

    QString charset;
    const QString text = Schedule::decode_html_bytes(data, &charset);
    QCOMPARE(charset, QStringLiteral("gbk"));
    QVERIFY(text.contains(QStringLiteral("个人课程表")));
    QVERIFY(text.contains(QStringLiteral("大学英语(1)")));

    // 显式把一段 GBK 汉字转回来，验证码表本身
    const QByteArray gbk = QStringLiteral("高等数学").toLocal8Bit();
    QVERIFY(!Schedule::looks_like_utf8(gbk));
    QCOMPARE(Schedule::gbk_to_unicode(gbk), QStringLiteral("高等数学"));
}

void TestZhengfangTimetable::parses_semester_and_time_slots() {
    ZhengfangTimetableIo io;
    ScheduleSnapshot snapshot;
    QString error;
    QVERIFY2(io.parse(sample_path(), &snapshot, &error), qPrintable(error));

    QCOMPARE(snapshot.semester.name, QStringLiteral("2024-2025学年第一学期"));
    // 样本最晚到第 18 周；按与 CSV 导入一致的规则取「不早于 20 周」
    QCOMPARE(snapshot.semester.total_weeks, 20);
    QVERIFY(snapshot.semester.start_date.isValid());

    // unitCount = 12，作息表应覆盖 1..12 节
    QCOMPARE(snapshot.time_slots.size(), 12);
    for (int i = 0; i < snapshot.time_slots.size(); ++i) {
        QCOMPARE(snapshot.time_slots.at(i).index, i + 1);
    }
    // 前 11 节带默认作息时间，第 12 节只定义序号
    QVERIFY(snapshot.time_slots.at(0).start_time.isValid());
    QVERIFY(!snapshot.time_slots.at(11).start_time.isValid());
}

void TestZhengfangTimetable::parses_courses() {
    ZhengfangTimetableIo io;
    ScheduleSnapshot snapshot;
    QString error;
    QVERIFY2(io.parse(sample_path(), &snapshot, &error), qPrintable(error));

    QCOMPARE(snapshot.courses.size(), 7);

    QStringList names;
    for (const Course& course : snapshot.courses) {
        names.append(course.name);
    }
    names.sort();
    const QStringList expected = {
        QStringLiteral("体育(1)"),
        QStringLiteral("大学物理(1)"),
        QStringLiteral("大学英语(1)"),
        QStringLiteral("形势与政策(1)"),
        QStringLiteral("程序设计基础"),
        QStringLiteral("程序设计基础实验"),
        QStringLiteral("高等数学A(1)"),
    };
    QCOMPARE(names, expected);

    // 课程代码取的是括号内的部分，不含教学班序号
    Course english;
    QVERIFY(find_course(snapshot.courses, QStringLiteral("大学英语(1)"), &english));
    QCOMPARE(english.code, QStringLiteral("00000002.02"));

    // 每个时间段都应补齐 id，便于后续合并与冲突定位
    for (const Course& course : snapshot.courses) {
        for (const CourseSession& session : course.sessions) {
            QVERIFY(!session.id.isEmpty());
        }
    }
}

void TestZhengfangTimetable::merges_consecutive_units() {
    ZhengfangTimetableIo io;
    ScheduleSnapshot snapshot;
    QString error;
    QVERIFY2(io.parse(sample_path(), &snapshot, &error), qPrintable(error));

    // 高等数学A(1)：周一第 1-2 节 → 合并为一段
    Course maths;
    QVERIFY(find_course(snapshot.courses, QStringLiteral("高等数学A(1)"), &maths));
    QCOMPARE(maths.sessions.size(), 1);
    QCOMPARE(maths.sessions.first().day_of_week, 1);
    QCOMPARE(maths.sessions.first().start_slot, 1);
    QCOMPARE(maths.sessions.first().slot_count, 2);

    // 程序设计基础：周三第 6-9 节 → 连续四节合并为一段
    Course programming;
    QVERIFY(find_course(snapshot.courses, QStringLiteral("程序设计基础"), &programming));
    QCOMPARE(programming.sessions.size(), 1);
    QCOMPARE(programming.sessions.first().day_of_week, 3);
    QCOMPARE(programming.sessions.first().start_slot, 6);
    QCOMPARE(programming.sessions.first().slot_count, 4);
}

void TestZhengfangTimetable::splits_non_consecutive_units() {
    ZhengfangTimetableIo io;
    ScheduleSnapshot snapshot;
    QString error;
    QVERIFY2(io.parse(sample_path(), &snapshot, &error), qPrintable(error));

    // 形势与政策(1)：周三第 1 节与第 4 节，中间隔着别的安排 → 必须拆成两段
    Course policy;
    QVERIFY(find_course(snapshot.courses, QStringLiteral("形势与政策(1)"), &policy));
    QCOMPARE(policy.sessions.size(), 2);
    QCOMPARE(session_signatures(policy),
        QStringList({QStringLiteral("3|1|1|2,4,6"), QStringLiteral("3|4|1|2,4,6")}));
}

void TestZhengfangTimetable::merges_activities_of_same_course() {
    ZhengfangTimetableIo io;
    ScheduleSnapshot snapshot;
    QString error;
    QVERIFY2(io.parse(sample_path(), &snapshot, &error), qPrintable(error));

    // 大学英语(1) 有两个活动（普通教室 / 语言实验室），应聚合为一门课程
    Course english;
    QVERIFY(find_course(snapshot.courses, QStringLiteral("大学英语(1)"), &english));
    QCOMPARE(english.sessions.size(), 2);
    QCOMPARE(session_signatures(english),
        QStringList({QStringLiteral("2|3|2|1,2,3,4,5,6,7,8,10,11,12"), QStringLiteral("4|3|2|2,4,6,8,10,12")}));
}

void TestZhengfangTimetable::parses_week_bitmap() {
    ZhengfangTimetableIo io;
    ScheduleSnapshot snapshot;
    QString error;
    QVERIFY2(io.parse(sample_path(), &snapshot, &error), qPrintable(error));

    // 单周课：位图只在奇数周置位，应还原为 1,3,5,7,9,11
    Course physics;
    QVERIFY(find_course(snapshot.courses, QStringLiteral("大学物理(1)"), &physics));
    QCOMPARE(physics.sessions.size(), 1);
    QCOMPARE(physics.sessions.first().weeks.weeks(), QList<int>({1, 3, 5, 7, 9, 11}));
    QCOMPARE(physics.sessions.first().weeks.to_expression(), QStringLiteral("1-11/2"));

    // 连续周次：1..18
    Course sports;
    QVERIFY(find_course(snapshot.courses, QStringLiteral("体育(1)"), &sports));
    QCOMPARE(sports.sessions.first().weeks.first_week(), 1);
    QCOMPARE(sports.sessions.first().weeks.last_week(), 18);
    QCOMPARE(sports.sessions.first().weeks.count(), 18);

    // 带缺口的周次：1-8 与 10-12
    Course english;
    QVERIFY(find_course(snapshot.courses, QStringLiteral("大学英语(1)"), &english));
    QCOMPARE(english.sessions.first().weeks.weeks(),
        QList<int>({1, 2, 3, 4, 5, 6, 7, 8, 10, 11, 12}));
}

void TestZhengfangTimetable::keeps_teacher_and_room() {
    ZhengfangTimetableIo io;
    ScheduleSnapshot snapshot;
    QString error;
    QVERIFY2(io.parse(sample_path(), &snapshot, &error), qPrintable(error));

    // 课程级默认值取第一个时间段的值
    Course english;
    QVERIFY(find_course(snapshot.courses, QStringLiteral("大学英语(1)"), &english));
    QCOMPARE(english.teacher, QStringLiteral("王演示,李演示"));
    QCOMPARE(english.location, QStringLiteral("2J202(东校区)"));

    // 与默认值相同的时间段不再重复存储；不同教室的时间段保留覆盖值
    QCOMPARE(english.sessions.at(0).effective_teacher(english.teacher), QStringLiteral("王演示,李演示"));
    QCOMPARE(english.sessions.at(0).effective_location(english.location), QStringLiteral("2J202(东校区)"));
    QCOMPARE(english.sessions.at(1).effective_location(english.location), QStringLiteral("语言实验室-2J203(东校区)"));

    Course maths;
    QVERIFY(find_course(snapshot.courses, QStringLiteral("高等数学A(1)"), &maths));
    QCOMPARE(maths.location, QStringLiteral("1J101(主校区)"));
}

void TestZhengfangTimetable::rejects_non_zhengfang_content() {
    ZhengfangTimetableIo io;
    ScheduleSnapshot snapshot;
    QString error;

    const QByteArray json = R"({"semester":{"name":"X"},"courses":[]})";
    QVERIFY(!io.parse_data(json, QStringLiteral("memory://json"), &snapshot, &error));
    QVERIFY(!error.isEmpty());

    const QByteArray html = QByteArrayLiteral("<html><body><h1>普通网页</h1></body></html>");
    QVERIFY(!io.parse_data(html, QStringLiteral("memory://html"), &snapshot, &error));
    QVERIFY(error.contains(QStringLiteral("正方教务")));

    // 空输入
    QVERIFY(!io.parse_data(QByteArray(), QStringLiteral("memory://empty"), &snapshot, &error));
}

void TestZhengfangTimetable::export_filter_excludes_zhengfang() {
    // 正方页面是**只导入**格式：可以出现在导入提示里，但不应进入导出过滤器
    QVERIFY(Schedule::supported_file_extensions().contains(QStringLiteral(".xls")));
    QVERIFY(!Schedule::export_file_extensions().contains(QStringLiteral(".xls")));
    QVERIFY(Schedule::export_file_extensions().contains(QStringLiteral(".json")));
    QVERIFY(Schedule::export_file_extensions().contains(QStringLiteral(".csv")));
    QVERIFY(Schedule::export_file_extensions().contains(QStringLiteral(".ics")));
}

void TestZhengfangTimetable::import_manager_previews_sample() {
    // ImportManager 必须内置注册正方导入器，用户直接选 .xls 即可导入
    ImportManager manager;
    QVERIFY(manager.supported_formats().contains(ScheduleFormat::ZhengfangHtml));

    const ScheduleSnapshot empty;
    const ImportPreview preview = manager.preview(sample_path(), empty);

    QVERIFY2(preview.is_valid, qPrintable(preview.error_message));
    QCOMPARE(preview.format, ScheduleFormat::ZhengfangHtml);
    QCOMPARE(preview.courses.size(), 7);
    QCOMPARE(preview.new_course_count, 7);
    QCOMPARE(preview.duplicate_count, 0);

    // 样本内部无冲突：同一天的安排要么节次不相交，要么周次不相交
    QCOMPARE(preview.conflicts.size(), 0);
    QVERIFY(preview.summary().contains(QStringLiteral("正方教务课表")));

    // 扩展名不可信时（内容仍是正方页面）也应能正确识别
    QFile file(sample_path());
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QByteArray raw = file.readAll();
    file.close();

    const ImportPreview by_content = manager.preview_data(raw, QStringLiteral("clipboard://课表"), empty);
    QVERIFY2(by_content.is_valid, qPrintable(by_content.error_message));
    QCOMPARE(by_content.format, ScheduleFormat::ZhengfangHtml);
}

QTEST_GUILESS_MAIN(TestZhengfangTimetable)

#include "tst_zhengfang_timetable.moc"
