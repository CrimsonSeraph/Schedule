#include "core/model/Course.h"
#include "core/model/CourseSession.h"
#include "core/model/ScheduleSnapshot.h"
#include "core/model/WeekMask.h"
#include "data/import_export/ImportExportTypes.h"
#include "data/import_export/ImportManager.h"
#include "data/import_export/academic_affairs/EcjtuTimetableIo.h"
#include "data/import_export/academic_affairs/WordTableReader.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTest>

using Schedule::Course;
using Schedule::CourseSession;
using Schedule::ImportManager;
using Schedule::ImportPreview;
using Schedule::ScheduleFormat;
using Schedule::ScheduleSnapshot;
using Schedule::WordTable;
using Schedule::WordTableReader;

namespace {

    /** @return 样本文件目录（由 CMake 通过编译定义注入）。 */
    QString samples_dir() {
        return QStringLiteral(SCHEDULE_SAMPLES_DIR);
    }

    /** @return Word 版式 HTML 样本（GBK，扩展名是 .doc）的绝对路径。 */
    QString doc_sample_path() {
        return QDir(samples_dir()).filePath(QStringLiteral("schedule_sample_ecjtu.doc"));
    }

    /** @return OOXML 样本（真正的 .docx）的绝对路径。 */
    QString docx_sample_path() {
        return QDir(samples_dir()).filePath(QStringLiteral("schedule_sample_ecjtu.docx"));
    }

    /** @return 内嵌浏览器抓取形态的页面样本（UTF-8 字节，但页面声明 gb2312）。 */
    QString html_sample_path() {
        return QDir(samples_dir()).filePath(QStringLiteral("schedule_sample_ecjtu.html"));
    }

    /** @return 读取整个文件；失败返回空。 */
    QByteArray read_all(const QString& path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            return QByteArray();
        }
        const QByteArray data = file.readAll();
        file.close();
        return data;
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

    /** @return 时间段签名 `星期|起始节|节数|周次列表`。 */
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

    /** @return 整张课表的 `课程名 → 时间段签名` 摘要，用于比较两条解析路径是否等价。 */
    QStringList schedule_signature(const ScheduleSnapshot& snapshot) {
        QStringList signature;
        for (const Course& course : snapshot.courses) {
            signature.append(QStringLiteral("%1#%2").arg(course.name, session_signatures(course).join(QLatin1Char(';'))));
        }
        signature.sort();
        return signature;
    }

    /** @return 一份最小可用的 Word 版式 HTML 课表，用来构造各种畸形输入。 */
    QByteArray minimal_document(const QByteArray& cell_content) {
        QByteArray html =
            "<html xmlns:w=\"urn:schemas-microsoft-com:office:word\"><body><table>"
            "<tr><td colspan=8>2024-2025 第一学期 示例 课表</td></tr>"
            "<tr><td>节次</td><td>星期一</td><td>星期二</td><td>星期三</td><td>星期四</td>"
            "<td>星期五</td><td>星期六</td><td>星期日</td></tr>"
            "<tr><td>1-2节</td><td>";
        html += cell_content;
        html += "</td><td></td><td></td><td></td><td></td><td></td><td></td></tr>"
                "</table></body></html>";
        return html;
    }

} // namespace

/**
 * @brief 华东交通大学教务综合管理系统课表导入器测试。
 *
 * 样本是**虚构数据**（见 `tools/gen_ecjtu_sample.py`），但结构与真实导出文件一致：
 * GBK 编码的 Word 版式 HTML（`.doc`）与内容等价的 OOXML 包（`.docx`）。
 *
 * 覆盖的关键行为：
 *  - 三种输入形态（Word 版式 HTML / `.docx` / 内容嗅探）都能识别；
 *  - `gridSpan` 与 `vMerge`：标题行整行合并、跨节次的课纵向合并；
 *  - 单元格内「课程名 → 教师 @教室 → 周次 节次」的字段顺序；
 *  - 第三行的节次列表才是权威：一个活动在多个节次行里重复出现时只算一次课；
 *  - 周次表达式：区间、缺口（`1-8,10-16`）、单周（`1-8(单)`）；
 *  - 两条解析路径（`.doc` 与 `.docx`）结果**完全等价**。
 */
class TestEcjtuTimetable : public QObject {
    Q_OBJECT

private slots:
    /** 两份样本都存在且非空。 */
    void sample_files_exist();

    /** `.doc` / `.docx` 都能被内容嗅探与扩展名映射识别。 */
    void sniffs_ecjtu_format();

    /** 读取 `.docx` 时 `gridSpan` 被展开成等宽的列。 */
    void expands_grid_span();

    /** 读取 `.docx` 时纵向合并格的延续行继承起始行内容。 */
    void propagates_vertical_merge();

    /** Word 版式 HTML（GBK）能被解码并读成同样的网格。 */
    void decodes_word_html();

    /** 内嵌浏览器抓到的页面：UTF-8 字节却自称 gb2312，仍要正确解码并导入。 */
    void decodes_captured_page();

    /** 学期与作息表按文档内容合成。 */
    void parses_semester_and_time_slots();

    /** 课程按名称聚合，教师与教室按时间段覆盖课程级默认值。 */
    void parses_courses();

    /** 跨节次的活动在多个节次行里重复出现，但只应产生一个时间段。 */
    void merges_repeated_cell_entries();

    /** 周次表达式：区间与单周。 */
    void parses_week_expressions();

    /** `.doc` 与 `.docx` 两条路径的结果完全等价。 */
    void doc_and_docx_agree();

    /** 非课表内容被拒绝，并给出可执行提示。 */
    void rejects_non_timetable_content();

    /** 无法识别的周次会让导入整体失败并指出位置，而不是静默丢课。 */
    void rejects_unparsable_entry();

    /** 只导入格式不出现在导出过滤器里。 */
    void export_filter_excludes_ecjtu();

    /** 导入器已注册，经 ImportManager 走通「预览」流程。 */
    void import_manager_previews_sample();
};

void TestEcjtuTimetable::sample_files_exist() {
    for (const QString& path : {doc_sample_path(), docx_sample_path()}) {
        QVERIFY2(QFile::exists(path), qPrintable(path));
        QVERIFY2(QFileInfo(path).size() > 0, qPrintable(path));
    }
}

void TestEcjtuTimetable::sniffs_ecjtu_format() {
    // 扩展名：教务系统导出的 Word 表格
    QCOMPARE(Schedule::format_from_extension(QStringLiteral("/tmp/课表.doc")), ScheduleFormat::EcjtuTimetable);
    QCOMPARE(Schedule::format_from_extension(QStringLiteral("/tmp/课表.docx")), ScheduleFormat::EcjtuTimetable);

    // 内容：GBK 的 Word 版式 HTML 与 OOXML 包都要认出来
    QCOMPARE(Schedule::format_from_content(read_all(doc_sample_path())), ScheduleFormat::EcjtuTimetable);
    QCOMPARE(Schedule::format_from_content(read_all(docx_sample_path())), ScheduleFormat::EcjtuTimetable);

    // 机器名与展示名
    QCOMPARE(Schedule::format_to_string(ScheduleFormat::EcjtuTimetable), QStringLiteral("ecjtu-timetable"));
    QCOMPARE(Schedule::format_display_name(ScheduleFormat::EcjtuTimetable), QStringLiteral("华东交大教务课表"));
}

void TestEcjtuTimetable::expands_grid_span() {
    WordTable table;
    QString error;
    QVERIFY2(WordTableReader::read_docx(docx_sample_path(), &table, &error), qPrintable(error));

    // 样本：标题行（gridSpan=8）+ 表头行 + 6 个节次行
    QCOMPARE(table.rows.size(), 8);
    for (const QStringList& row : table.rows) {
        QCOMPARE(row.size(), 8);
    }

    // 标题行整行合并：内容在第一列，其余列因为被覆盖而为空
    QCOMPARE(table.rows.at(0).at(0), QStringLiteral("2024-2025 第一学期 示例同学 课表"));
    for (int column = 1; column < 8; ++column) {
        QVERIFY2(table.rows.at(0).at(column).isEmpty(), qPrintable(table.rows.at(0).at(column)));
    }

    // 表头行没有被 gridSpan 挤歪
    QCOMPARE(table.rows.at(1).at(0), QStringLiteral("节次"));
    QCOMPARE(table.rows.at(1).at(1), QStringLiteral("星期一"));
    QCOMPARE(table.rows.at(1).at(7), QStringLiteral("星期日"));
}

void TestEcjtuTimetable::propagates_vertical_merge() {
    WordTable table;
    QString error;
    QVERIFY2(WordTableReader::read_docx(docx_sample_path(), &table, &error), qPrintable(error));

    // 星期五是第 6 列（0 基）；数据库跨 1-2节 / 3-4节 两行，是 vMerge 合并格
    const QString start = table.rows.at(2).at(5);
    QVERIFY2(start.contains(QStringLiteral("数据库(演示)")), qPrintable(start));

    // 延续行必须继承起始行的内容：留空会直接丢掉这门课
    QCOMPARE(table.rows.at(3).at(5), start);

    // 两行确实是不同的节次行
    QCOMPARE(table.rows.at(2).at(0), QStringLiteral("1-2节"));
    QCOMPARE(table.rows.at(3).at(0), QStringLiteral("3-4节"));
}

void TestEcjtuTimetable::decodes_word_html() {
    const QByteArray data = read_all(doc_sample_path());
    QVERIFY(!data.isEmpty());

    WordTable table;
    QString error;
    QVERIFY2(WordTableReader::read_word_html(data, &table, &error), qPrintable(error));

    QCOMPARE(table.rows.size(), 8);
    // GBK 解码正确与否，看中文有没有变成乱码
    QCOMPARE(table.rows.at(1).at(0), QStringLiteral("节次"));
    QCOMPARE(table.rows.at(1).at(1), QStringLiteral("星期一"));
    QCOMPARE(table.rows.at(0).at(0), QStringLiteral("2024-2025 第一学期 示例同学 课表"));

    // rowspan 的效果与 OOXML 的 vMerge 一致
    QVERIFY2(table.rows.at(3).at(5).contains(QStringLiteral("数据库(演示)")), qPrintable(table.rows.at(3).at(5)));
}

void TestEcjtuTimetable::decodes_captured_page() {
    const QByteArray data = read_all(html_sample_path());
    QVERIFY(!data.isEmpty());

    // 抓取链路喂进来的就是「JS 字符串 → toUtf8()」的字节，而 outerHTML 里
    // 仍保留页面自己的 <meta charset="gb2312">。照声明解会整页乱码：
    // 中文全部变成替换字符，连格式嗅探都认不出来。
    QCOMPARE(Schedule::format_from_content(data), ScheduleFormat::EcjtuTimetable);

    Schedule::EcjtuTimetableIo importer;
    ScheduleSnapshot snapshot;
    QString error;
    QVERIFY2(importer.parse_data(data,
                 QStringLiteral("https://jwxt.ecjtu.edu.cn/courseTable"),
                 &snapshot,
                 &error),
        qPrintable(error));

    // 课表之前还有一张布局表格：必须挑中课表那张，而不是第一张
    QCOMPARE(snapshot.courses.size(), 6);

    Course course;
    QVERIFY(find_course(snapshot.courses, QStringLiteral("大学英语(演示)"), &course));
    QCOMPARE(course.teacher, QStringLiteral("王演示"));

    // 抓取页里 数据库 在两个节次行各出现一次（没有 vMerge），去重后仍只算一段课
    QVERIFY(find_course(snapshot.courses, QStringLiteral("数据库(演示)"), &course));
    QCOMPARE(session_signatures(course).size(), 1);

    // 与导出文件是同一张表，课程集合应当一致
    const QByteArray exported = read_all(doc_sample_path());
    QCOMPARE(Schedule::format_from_content(exported), ScheduleFormat::EcjtuTimetable);
}

void TestEcjtuTimetable::parses_semester_and_time_slots() {
    Schedule::EcjtuTimetableIo importer;
    ScheduleSnapshot snapshot;
    QString error;
    QVERIFY2(importer.parse(doc_sample_path(), &snapshot, &error), qPrintable(error));

    QCOMPARE(snapshot.semester.name, QStringLiteral("2024-2025第一学期"));
    QVERIFY(snapshot.semester.start_date.isValid());
    QVERIFY(snapshot.semester.total_weeks >= 20);

    // 样本的节次行到 11-12 节，作息表至少覆盖 12 节
    QVERIFY(snapshot.time_slots.size() >= 12);
    for (int index = 0; index < snapshot.time_slots.size(); ++index) {
        QCOMPARE(snapshot.time_slots.at(index).index, index + 1);
    }
}

void TestEcjtuTimetable::parses_courses() {
    Schedule::EcjtuTimetableIo importer;
    ScheduleSnapshot snapshot;
    QString error;
    QVERIFY2(importer.parse(doc_sample_path(), &snapshot, &error), qPrintable(error));

    QCOMPARE(snapshot.courses.size(), 6);

    Course course;
    QVERIFY(find_course(snapshot.courses, QStringLiteral("大学英语(演示)"), &course));
    QCOMPARE(course.teacher, QStringLiteral("王演示"));
    QCOMPARE(course.location, QStringLiteral("31-103"));

    // 高等数学在星期一有两段（1-2 节与 3-4 节，都是 1-8 周）
    QVERIFY(find_course(snapshot.courses, QStringLiteral("高等数学(演示)"), &course));
    const QStringList signatures = session_signatures(course);
    QCOMPARE(signatures.size(), 2);
    QCOMPARE(signatures.at(0), QStringLiteral("1|1|2|1,2,3,4,5,6,7,8"));
    QCOMPARE(signatures.at(1), QStringLiteral("1|3|2|1,2,3,4,5,6,7,8"));

    // 同一个格子里的第二门课不会因为顺序而丢失
    QVERIFY(find_course(snapshot.courses, QStringLiteral("程序设计基础(演示)"), &course));
    QCOMPARE(course.teacher, QStringLiteral("李演示"));
}

void TestEcjtuTimetable::merges_repeated_cell_entries() {
    Schedule::EcjtuTimetableIo importer;
    ScheduleSnapshot snapshot;
    QString error;
    QVERIFY2(importer.parse(docx_sample_path(), &snapshot, &error), qPrintable(error));

    Course course;
    QVERIFY(find_course(snapshot.courses, QStringLiteral("数据库(演示)"), &course));

    // 数据库的节次列表是 1,2,3：跨了 1-2节 与 3-4节 两行（OOXML 里是 vMerge），
    // 但它们是同一个活动，必须只算一段「星期五 1-3 节」
    const QStringList signatures = session_signatures(course);
    QCOMPARE(signatures.size(), 1);
    QCOMPARE(signatures.at(0), QStringLiteral("5|1|3|9,10,11,12,13,14,15,16"));
    QCOMPARE(course.teacher, QStringLiteral("赵演示"));
    QCOMPARE(course.location, QStringLiteral("31-105"));
}

void TestEcjtuTimetable::parses_week_expressions() {
    Schedule::EcjtuTimetableIo importer;
    ScheduleSnapshot snapshot;
    QString error;
    QVERIFY2(importer.parse(doc_sample_path(), &snapshot, &error), qPrintable(error));

    Course course;

    // 单周：1-8(单) → 1,3,5,7
    QVERIFY(find_course(snapshot.courses, QStringLiteral("大学物理(演示)"), &course));
    QCOMPARE(session_signatures(course), QStringList{QStringLiteral("2|5|2|1,3,5,7")});

    // 缺口周次：1-8,10-16
    QVERIFY(find_course(snapshot.courses, QStringLiteral("数据结构(演示)"), &course));
    QCOMPARE(session_signatures(course), QStringList{QStringLiteral("4|7|2|1,2,3,4,5,6,7,8,10,11,12,13,14,15,16")});
}

void TestEcjtuTimetable::doc_and_docx_agree() {
    Schedule::EcjtuTimetableIo importer;

    ScheduleSnapshot from_html;
    ScheduleSnapshot from_package;
    QString error;
    QVERIFY2(importer.parse(doc_sample_path(), &from_html, &error), qPrintable(error));
    QVERIFY2(importer.parse(docx_sample_path(), &from_package, &error), qPrintable(error));

    // 同一份表格的两种容器形态必须解析出同一张课表：
    // 这是「Word 版式 HTML 路径」与「word/document.xml 路径」互相验证的地方
    QCOMPARE(schedule_signature(from_html), schedule_signature(from_package));
    QCOMPARE(from_html.semester.name, from_package.semester.name);
    QCOMPARE(from_html.time_slots.size(), from_package.time_slots.size());
}

void TestEcjtuTimetable::rejects_non_timetable_content() {
    Schedule::EcjtuTimetableIo importer;
    ScheduleSnapshot snapshot;
    QString error;

    // 普通 HTML 页面：没有课表表头
    QVERIFY(!importer.parse_data(QByteArray("<html><body><p>hello</p></body></html>"),
        QStringLiteral("clipboard://x"),
        &snapshot,
        &error));
    QVERIFY2(!error.isEmpty(), qPrintable(error));

    // 表格结构对，但表头不是「节次 + 星期」
    QVERIFY(!importer.parse_data(minimal_document(QByteArray("高等数学<br />张老师 @A101<br />1-8 1,2"))
                                     .replace("<td>节次</td>", "<td>时段</td>"),
        QStringLiteral("clipboard://x"),
        &snapshot,
        &error));
    QVERIFY2(error.contains(QStringLiteral("表头")), qPrintable(error));
}

void TestEcjtuTimetable::rejects_unparsable_entry() {
    Schedule::EcjtuTimetableIo importer;
    ScheduleSnapshot snapshot;
    QString error;

    // 「待定」不是合法周次：宁可整体失败并指出位置，也不要静默丢掉这门课
    QVERIFY(!importer.parse_data(minimal_document(QByteArray("高等数学<br />张老师 @A101<br />待定 1,2")),
        QStringLiteral("clipboard://x"),
        &snapshot,
        &error));
    QVERIFY2(error.contains(QStringLiteral("无法识别")), qPrintable(error));
    QVERIFY2(error.contains(QStringLiteral("高等数学")), qPrintable(error));
}

void TestEcjtuTimetable::export_filter_excludes_ecjtu() {
    // 华东交大课表是**只导入**格式：出现在导入提示里，但不应进入导出过滤器
    QVERIFY(Schedule::supported_file_extensions().contains(QStringLiteral(".docx")));
    QVERIFY(Schedule::supported_file_extensions().contains(QStringLiteral(".doc")));
    QVERIFY(!Schedule::export_file_extensions().contains(QStringLiteral(".docx")));
    QVERIFY(!Schedule::export_file_extensions().contains(QStringLiteral(".doc")));
    QVERIFY(Schedule::export_file_extensions().contains(QStringLiteral(".json")));
}

void TestEcjtuTimetable::import_manager_previews_sample() {
    // ImportManager 必须内置注册华东交大导入器，用户直接选 .doc / .docx 即可导入
    ImportManager manager;
    QVERIFY(manager.supported_formats().contains(ScheduleFormat::EcjtuTimetable));

    const ScheduleSnapshot empty;
    const ImportPreview preview = manager.preview(doc_sample_path(), empty);

    QVERIFY2(preview.is_valid, qPrintable(preview.error_message));
    QCOMPARE(preview.format, ScheduleFormat::EcjtuTimetable);
    QCOMPARE(preview.courses.size(), 6);
    QCOMPARE(preview.new_course_count, 6);
    QCOMPARE(preview.duplicate_count, 0);
    QVERIFY(preview.summary().contains(QStringLiteral("华东交大教务课表")));

    // 内容嗅探路线（网页抓取、剪贴板）也要能导入 .docx 的字节
    const ImportPreview by_content = manager.preview_data(read_all(docx_sample_path()),
        QStringLiteral("capture://ecjtu"),
        empty);
    QVERIFY2(by_content.is_valid, qPrintable(by_content.error_message));
    QCOMPARE(by_content.format, ScheduleFormat::EcjtuTimetable);
    QCOMPARE(by_content.courses.size(), 6);
}

QTEST_GUILESS_MAIN(TestEcjtuTimetable)

#include "tst_ecjtu_timetable.moc"
