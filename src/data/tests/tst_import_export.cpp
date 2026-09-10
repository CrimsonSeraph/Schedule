#include "core/model/Course.h"
#include "core/model/ScheduleSnapshot.h"
#include "core/model/Semester.h"
#include "core/model/TimeSlot.h"
#include "data/import_export/ExportManager.h"
#include "data/import_export/ImportExportTypes.h"
#include "data/import_export/ImportManager.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

using Schedule::Course;
using Schedule::CourseSession;
using Schedule::ExportManager;
using Schedule::ExportResult;
using Schedule::ImportManager;
using Schedule::ImportPreview;
using Schedule::ImportResult;
using Schedule::ImportStrategy;
using Schedule::ScheduleFormat;
using Schedule::ScheduleSnapshot;
using Schedule::Semester;
using Schedule::TimeSlot;
using Schedule::WeekMask;

namespace {

    const QDate SEMESTER_START(2024, 9, 2); // 周一

    /** @return 一个含两门课、三个时间段的测试快照。 */
    ScheduleSnapshot make_snapshot() {
        ScheduleSnapshot snapshot;
        snapshot.semester.id = QStringLiteral("semester-1");
        snapshot.semester.name = QStringLiteral("2024-2025 学年第一学期");
        snapshot.semester.start_date = SEMESTER_START;
        snapshot.semester.total_weeks = 16;
        snapshot.time_slots = TimeSlot::default_slots();

        Course math;
        math.id = QStringLiteral("math");
        math.semester_id = snapshot.semester.id;
        math.name = QStringLiteral("高等数学");
        math.code = QStringLiteral("MATH101");
        math.teacher = QStringLiteral("张老师");
        math.location = QStringLiteral("教一 101");
        math.credits = 4.0;
        math.notes = QStringLiteral("需带教材");

        CourseSession lecture;
        lecture.id = QStringLiteral("math-lecture");
        lecture.day_of_week = 1;
        lecture.start_slot = 1;
        lecture.slot_count = 2;
        lecture.weeks = WeekMask::from_expression(QStringLiteral("1-16"), 16);
        math.sessions.append(lecture);

        CourseSession lab;
        lab.id = QStringLiteral("math-lab");
        lab.day_of_week = 3;
        lab.start_slot = 5;
        lab.slot_count = 2;
        lab.weeks = WeekMask::from_expression(QStringLiteral("1-15/2"), 16);
        lab.location = QStringLiteral("实验楼 302");
        math.sessions.append(lab);

        Course english;
        english.id = QStringLiteral("english");
        english.semester_id = snapshot.semester.id;
        english.name = QStringLiteral("大学英语");
        english.location = QStringLiteral("外语楼 201");

        CourseSession english_session;
        english_session.id = QStringLiteral("english-1");
        english_session.day_of_week = 2;
        english_session.start_slot = 3;
        english_session.slot_count = 2;
        english_session.weeks = WeekMask::from_expression(QStringLiteral("1-4,6,9-10"), 16);
        english.sessions.append(english_session);

        snapshot.courses.append(math);
        snapshot.courses.append(english);
        return snapshot;
    }

    /** @return 不含任何课程的快照（用于测试导入合并）。 */
    ScheduleSnapshot make_empty_snapshot() {
        ScheduleSnapshot snapshot;
        snapshot.semester.id = QStringLiteral("semester-1");
        snapshot.semester.name = QStringLiteral("2024-2025 学年第一学期");
        snapshot.semester.start_date = SEMESTER_START;
        snapshot.semester.total_weeks = 16;
        snapshot.time_slots = TimeSlot::default_slots();
        return snapshot;
    }

} // namespace

/**
 * @brief 导入导出子系统的单元测试：格式识别、三种格式的往返、预览、冲突与合并策略。
 */
class TestImportExport : public QObject {
    Q_OBJECT

private slots:
    /** 格式名 / 扩展名 / 内容嗅探。 */
    void recognises_formats();

    /** 导出文件名规则与清洗。 */
    void generates_export_file_name();

    /** 导出到指定目录：创建目录、返回实际路径。 */
    void exports_to_directory();

    /** JSON 往返无损。 */
    void round_trips_json();

    /** CSV 往返保留课程与时间段，并可由 Excel 风格表头解析。 */
    void round_trips_csv();

    /** ICS 往返保留星期 / 节次 / 周次，并可用标准 RRULE 表达单双周。 */
    void round_trips_ics();

    /** ICS 在非等差周次时改用 RDATE。 */
    void writes_rdate_for_irregular_weeks();

    /** 预览会统计重复课程并给出提示。 */
    void previews_duplicates_and_warnings();

    /** 预览只报告“本次导入新引入的冲突”。 */
    void preview_reports_only_new_conflicts();

    /** 合并策略：同 id 更新、其余新增。 */
    void applies_merge_strategy();

    /** 去重策略：重复课程跳过。 */
    void applies_skip_duplicates_strategy();

    /** 覆盖策略：清空后写入。 */
    void applies_overwrite_strategy();

    /** 错误路径：文件不存在、内容无法识别、非 UTF-8 编码。 */
    void reports_errors();

    /** 剪贴板 / 分享码入口：直接从内存解析。 */
    void parses_from_memory();
};

void TestImportExport::recognises_formats() {
    QCOMPARE(Schedule::format_to_string(ScheduleFormat::Json), QStringLiteral("json"));
    QCOMPARE(Schedule::format_from_string(QStringLiteral("CSV")), ScheduleFormat::Csv);
    QCOMPARE(Schedule::format_from_string(QStringLiteral("nope")), ScheduleFormat::Unknown);
    QCOMPARE(Schedule::file_extension(ScheduleFormat::Ics), QStringLiteral("ics"));
    QVERIFY(Schedule::supported_file_extensions().contains(QStringLiteral(".json")));

    QCOMPARE(Schedule::format_from_extension(QStringLiteral("a/b/c.json")), ScheduleFormat::Json);
    QCOMPARE(Schedule::format_from_extension(QStringLiteral("a.CSV")), ScheduleFormat::Csv);
    QCOMPARE(Schedule::format_from_extension(QStringLiteral("a.ics")), ScheduleFormat::Ics);
    QCOMPARE(Schedule::format_from_extension(QStringLiteral("a.pdf")), ScheduleFormat::Unknown);

    // 内容嗅探优先于扩展名：内容才是事实
    QCOMPARE(Schedule::format_from_content(QByteArray("{\"format\":\"schedule\"}")), ScheduleFormat::Json);
    QCOMPARE(Schedule::format_from_content(QByteArray("BEGIN:VCALENDAR\r\n")), ScheduleFormat::Ics);
    QCOMPARE(Schedule::format_from_content(QByteArray("课程名称,星期\n")), ScheduleFormat::Csv);
    QCOMPARE(Schedule::format_from_content(QByteArray("random text")), ScheduleFormat::Unknown);
    QCOMPARE(Schedule::format_from_content(QByteArray()), ScheduleFormat::Unknown);

    QCOMPARE(ImportManager::strategy_names().size(), 3);
    QCOMPARE(ImportManager::strategy_at(1), ImportStrategy::SkipDuplicates);
    QCOMPARE(ImportManager::index_of_strategy(ImportStrategy::Overwrite), 2);
}

void TestImportExport::generates_export_file_name() {
    const QDateTime now(QDate(2024, 9, 10), QTime(12, 0, 0));

    QCOMPARE(ExportManager::suggested_file_name(QStringLiteral("2024-2025 学年第一学期"), ScheduleFormat::Json, now),
        QStringLiteral("Schedule_2024-2025_学年第一学期_20240910_120000.json"));
    QCOMPARE(ExportManager::suggested_file_name(QString(), ScheduleFormat::Csv, now),
        QStringLiteral("Schedule_20240910_120000.csv"));

    // 路径分隔符与 Windows 保留字符必须被清洗
    QCOMPARE(ExportManager::sanitize_file_component(QStringLiteral("2024/2025:第一*学期?")), QStringLiteral("2024_2025_第一_学期"));
    QCOMPARE(ExportManager::sanitize_file_component(QStringLiteral("   ")), QString());
    QCOMPARE(ExportManager::sanitize_file_component(QStringLiteral(".hidden.")), QStringLiteral("hidden"));

    QVERIFY(ExportManager::file_dialog_filter().contains(QStringLiteral("*.ics")));
}

void TestImportExport::exports_to_directory() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ExportManager manager;
    const ScheduleSnapshot snapshot = make_snapshot();

    // 目标目录尚不存在：应当被自动创建
    const QString target = directory.filePath(QStringLiteral("out/nested"));
    const ExportResult result = manager.export_to_directory(target, snapshot, ScheduleFormat::Json);

    QVERIFY2(result.success, qPrintable(result.error_message));
    QVERIFY(result.file_path.startsWith(QDir(target).absolutePath()));
    QVERIFY(result.file_path.endsWith(QStringLiteral(".json")));
    QVERIFY(QFile::exists(result.file_path));
    QCOMPARE(result.course_count, 2);
    QVERIFY(result.bytes > 0);
    QVERIFY(result.summary().contains(result.file_path));

    // 空目录参数报错
    const ExportResult invalid = manager.export_to_directory(QString(), snapshot, ScheduleFormat::Json);
    QVERIFY(!invalid.success);
    QVERIFY(!invalid.error_message.isEmpty());

    // 指定文件名主体
    const ExportResult named = manager.export_to_directory(target, snapshot, ScheduleFormat::Csv, QStringLiteral("custom.csv"));
    QVERIFY(named.success);
    QVERIFY(named.file_path.endsWith(QStringLiteral("custom.csv")));
}

void TestImportExport::round_trips_json() {
    ExportManager exporter;
    ImportManager importer;
    const ScheduleSnapshot source = make_snapshot();

    QString error;
    const QByteArray document = exporter.serialize(source, ScheduleFormat::Json, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QVERIFY(!document.isEmpty());

    const ImportPreview preview = importer.preview_data(document, QStringLiteral("memory://json"), make_empty_snapshot());
    QVERIFY2(preview.is_valid, qPrintable(preview.error_message));
    QCOMPARE(preview.format, ScheduleFormat::Json);
    QCOMPARE(preview.courses.size(), 2);

    const Course& math = preview.courses.first();
    QCOMPARE(math.id, QStringLiteral("math"));
    QCOMPARE(math.name, QStringLiteral("高等数学"));
    QCOMPARE(math.credits, 4.0);
    QCOMPARE(math.notes, QStringLiteral("需带教材"));
    QCOMPARE(math.sessions.size(), 2);
    QCOMPARE(math.sessions.at(1).weeks, WeekMask::from_expression(QStringLiteral("1-15/2"), 16));

    // 无损：再次导出后课程内容应完全一致（`exported_at` 是导出时刻，不参与比较）
    ScheduleSnapshot restored;
    restored.semester = source.semester;
    restored.time_slots = preview.time_slots;
    restored.courses = preview.courses;

    const QJsonObject before = QJsonDocument::fromJson(document).object();
    const QJsonObject after = QJsonDocument::fromJson(exporter.serialize(restored, ScheduleFormat::Json, &error)).object();
    QCOMPARE(after.value(QStringLiteral("courses")).toArray(), before.value(QStringLiteral("courses")).toArray());
    QCOMPARE(after.value(QStringLiteral("semester")).toObject(), before.value(QStringLiteral("semester")).toObject());
}

void TestImportExport::round_trips_csv() {
    ExportManager exporter;
    ImportManager importer;
    const ScheduleSnapshot source = make_snapshot();

    QString error;
    const QByteArray csv = exporter.serialize(source, ScheduleFormat::Csv, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QVERIFY(csv.startsWith(QByteArray::fromHex("EFBBBF"))); // UTF-8 BOM，Excel 友好

    const ImportPreview preview = importer.preview_data(csv, QStringLiteral("memory://csv"), make_empty_snapshot());
    QVERIFY2(preview.is_valid, qPrintable(preview.error_message));
    QCOMPARE(preview.format, ScheduleFormat::Csv);
    QCOMPARE(preview.courses.size(), 2);

    const Course& math = preview.courses.first();
    QCOMPARE(math.name, QStringLiteral("高等数学"));
    QCOMPARE(math.code, QStringLiteral("MATH101"));
    QCOMPARE(math.teacher, QStringLiteral("张老师"));
    QCOMPARE(math.sessions.size(), 2);
    QCOMPARE(math.sessions.at(0).day_of_week, 1);
    QCOMPARE(math.sessions.at(0).start_slot, 1);
    QCOMPARE(math.sessions.at(0).slot_count, 2);
    QCOMPARE(math.sessions.at(0).weeks, WeekMask::from_expression(QStringLiteral("1-16"), 16));
    QCOMPARE(math.sessions.at(1).day_of_week, 3);
    QCOMPARE(math.sessions.at(1).weeks, WeekMask::from_expression(QStringLiteral("1-15/2"), 16));
    QCOMPARE(math.sessions.at(1).location, QStringLiteral("实验楼 302"));

    // 手工整理的 CSV：列顺序打乱、含引号与逗号、使用英文列名
    const QByteArray handcrafted =
        "name,weeks,day,start_slot,slot_count,teacher,location\n"
        "\"数据结构, 上\",1-8,周三,5,2,\"李, 老师\",\"A楼, 301\"\n";
    const ImportPreview handcrafted_preview = importer.preview_data(handcrafted, QStringLiteral("memory://hand"), make_empty_snapshot());
    QVERIFY2(handcrafted_preview.is_valid, qPrintable(handcrafted_preview.error_message));
    QCOMPARE(handcrafted_preview.courses.size(), 1);
    QCOMPARE(handcrafted_preview.courses.first().name, QStringLiteral("数据结构, 上"));
    QCOMPARE(handcrafted_preview.courses.first().teacher, QStringLiteral("李, 老师"));
    QCOMPARE(handcrafted_preview.courses.first().sessions.first().day_of_week, 3);
}

void TestImportExport::round_trips_ics() {
    ExportManager exporter;
    ImportManager importer;
    const ScheduleSnapshot source = make_snapshot();

    QString error;
    const QByteArray ics = exporter.serialize(source, ScheduleFormat::Ics, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QVERIFY(ics.contains("BEGIN:VCALENDAR"));
    QVERIFY(ics.contains("BEGIN:VEVENT"));
    QVERIFY(ics.contains("X-SCHEDULE-WEEKS:1-15/2"));
    // 单双周应表达为带 INTERVAL 的 RRULE，便于系统日历正确展开
    QVERIFY(ics.contains("INTERVAL=2"));

    const ImportPreview preview = importer.preview_data(ics, QStringLiteral("memory://ics"), make_empty_snapshot());
    QVERIFY2(preview.is_valid, qPrintable(preview.error_message));
    QCOMPARE(preview.format, ScheduleFormat::Ics);
    QCOMPARE(preview.courses.size(), 2);
    QCOMPARE(preview.semester.name, QStringLiteral("2024-2025 学年第一学期"));

    const Course& math = preview.courses.first();
    QCOMPARE(math.name, QStringLiteral("高等数学"));
    QCOMPARE(math.teacher, QStringLiteral("张老师"));
    QCOMPARE(math.sessions.size(), 2);
    QCOMPARE(math.sessions.at(0).day_of_week, 1);
    QCOMPARE(math.sessions.at(0).start_slot, 1);
    QCOMPARE(math.sessions.at(0).slot_count, 2);
    QCOMPARE(math.sessions.at(0).weeks, WeekMask::from_expression(QStringLiteral("1-16"), 16));
    QCOMPARE(math.sessions.at(1).day_of_week, 3);
    QCOMPARE(math.sessions.at(1).weeks, WeekMask::from_expression(QStringLiteral("1-15/2"), 16));
    QCOMPARE(math.sessions.at(1).location, QStringLiteral("实验楼 302"));

    // 作息表由出现过的上课时间合成
    QCOMPARE(preview.time_slots.size(), 3);
    QCOMPARE(preview.time_slots.first().start_time, QTime(8, 0));
    QCOMPARE(preview.time_slots.last().start_time, QTime(14, 0));
}

void TestImportExport::writes_rdate_for_irregular_weeks() {
    ExportManager exporter;
    ImportManager importer;
    const ScheduleSnapshot source = make_snapshot();

    QString error;
    const QByteArray ics = exporter.serialize(source, ScheduleFormat::Ics, &error);
    // “大学英语”的周次是 1-4,6,9-10（非等差），必须退化为 RDATE 列表
    QVERIFY(ics.contains("RDATE:"));

    const ImportPreview preview = importer.preview_data(ics, QStringLiteral("memory://ics"), make_empty_snapshot());
    QVERIFY2(preview.is_valid, qPrintable(preview.error_message));

    Course english;
    for (const Course& course : preview.courses) {
        if (course.name == QStringLiteral("大学英语")) {
            english = course;
        }
    }
    QCOMPARE(english.name, QStringLiteral("大学英语"));
    QCOMPARE(english.sessions.first().weeks, WeekMask::from_expression(QStringLiteral("1-4,6,9-10"), 16));
}

void TestImportExport::previews_duplicates_and_warnings() {
    ExportManager exporter;
    ImportManager importer;
    const ScheduleSnapshot source = make_snapshot();

    QString error;
    const QByteArray document = exporter.serialize(source, ScheduleFormat::Json, &error);

    // 当前课表已包含同样的课程 → 全部算重复
    const ImportPreview preview = importer.preview_data(document, QStringLiteral("memory://json"), source);
    QVERIFY2(preview.is_valid, qPrintable(preview.error_message));
    QCOMPARE(preview.duplicate_count, 2);
    QCOMPARE(preview.new_course_count, 0);
    QVERIFY(preview.summary().contains(QStringLiteral("2 门课程")));
    // 与自身导入不应产生任何新冲突
    QVERIFY(!preview.has_blocking_conflict());

    // 文件来自其它学期 → 给出提示
    ScheduleSnapshot other_current = make_empty_snapshot();
    other_current.semester.id = QStringLiteral("semester-2");
    other_current.semester.name = QStringLiteral("2025 春季学期");
    const ImportPreview other_preview = importer.preview_data(document, QStringLiteral("memory://json"), other_current);
    QVERIFY(other_preview.is_valid);
    QVERIFY(!other_preview.warnings.isEmpty());
    QVERIFY(other_preview.warnings.first().contains(QStringLiteral("其它学期")));
}

void TestImportExport::preview_reports_only_new_conflicts() {
    ExportManager exporter;
    ImportManager importer;

    // 现有课表：周一 1-2 节有“现有课”
    ScheduleSnapshot current = make_empty_snapshot();
    Course existing;
    existing.id = QStringLiteral("existing");
    existing.name = QStringLiteral("现有课");
    CourseSession existing_session;
    existing_session.id = QStringLiteral("existing-1");
    existing_session.day_of_week = 1;
    existing_session.start_slot = 1;
    existing_session.slot_count = 2;
    existing_session.weeks = WeekMask::from_expression(QStringLiteral("1-16"), 16);
    existing.sessions.append(existing_session);
    current.courses.append(existing);
    QVERIFY(current.detect_conflicts().isEmpty());

    // 待导入文件：两门课，其中一门与现有课冲突
    ScheduleSnapshot incoming = make_empty_snapshot();
    Course conflicting;
    conflicting.id = QStringLiteral("new-conflicting");
    conflicting.name = QStringLiteral("冲突课");
    CourseSession conflicting_session;
    conflicting_session.id = QStringLiteral("new-conflicting-1");
    conflicting_session.day_of_week = 1;
    conflicting_session.start_slot = 2;
    conflicting_session.slot_count = 2;
    conflicting_session.weeks = WeekMask::from_expression(QStringLiteral("1-16"), 16);
    conflicting.sessions.append(conflicting_session);
    incoming.courses.append(conflicting);

    Course clean;
    clean.id = QStringLiteral("new-clean");
    clean.name = QStringLiteral("无冲突课");
    clean.sessions.append(existing_session);
    clean.sessions.first().id = QStringLiteral("new-clean-1");
    clean.sessions.first().day_of_week = 5;
    incoming.courses.append(clean);

    QString error;
    const QByteArray document = exporter.serialize(incoming, ScheduleFormat::Json, &error);

    const ImportPreview preview = importer.preview_data(document, QStringLiteral("memory://json"), current);
    QVERIFY2(preview.is_valid, qPrintable(preview.error_message));
    QCOMPARE(preview.conflicts.size(), 1);
    QCOMPARE(preview.conflicts.first().type, Schedule::Conflict::Type::TimeOverlap);
    QVERIFY(preview.has_blocking_conflict());
    QCOMPARE(preview.new_course_count, 2);
    QCOMPARE(preview.duplicate_count, 0);
}

void TestImportExport::applies_merge_strategy() {
    ExportManager exporter;
    ImportManager importer;

    const ScheduleSnapshot source = make_snapshot();
    QString error;
    const QByteArray document = exporter.serialize(source, ScheduleFormat::Json, &error);
    const ImportPreview preview = importer.preview_data(document, QStringLiteral("memory://json"), make_empty_snapshot());
    QVERIFY(preview.is_valid);

    // 空课表 + 合并 → 全部新增
    ScheduleSnapshot target = make_empty_snapshot();
    const ImportResult first = importer.apply(preview, ImportStrategy::Merge, &target);
    QVERIFY2(first.success, qPrintable(first.error_message));
    QCOMPARE(first.imported_count, 2);
    QCOMPARE(first.updated_count, 0);
    QCOMPARE(first.skipped_count, 0);
    QCOMPARE(target.courses.size(), 2);
    QVERIFY(first.summary().contains(QStringLiteral("新增 2")));

    // 再次合并同一份文件 → 按 id 更新，不产生重复
    const ImportResult second = importer.apply(preview, ImportStrategy::Merge, &target);
    QVERIFY(second.success);
    QCOMPARE(second.imported_count, 0);
    QCOMPARE(second.updated_count, 2);
    QCOMPARE(target.courses.size(), 2);
}

void TestImportExport::applies_skip_duplicates_strategy() {
    ExportManager exporter;
    ImportManager importer;

    const ScheduleSnapshot source = make_snapshot();
    QString error;
    const QByteArray document = exporter.serialize(source, ScheduleFormat::Json, &error);

    ScheduleSnapshot target = make_empty_snapshot();
    const ImportPreview preview = importer.preview_data(document, QStringLiteral("memory://json"), target);
    QVERIFY(preview.is_valid);

    // 第一次导入：全部新增
    QVERIFY(importer.apply(preview, ImportStrategy::SkipDuplicates, &target).success);
    QCOMPARE(target.courses.size(), 2);

    // 第二次导入：全部因重复被跳过
    const ImportResult result = importer.apply(preview, ImportStrategy::SkipDuplicates, &target);
    QVERIFY(result.success);
    QCOMPARE(result.imported_count, 0);
    QCOMPARE(result.skipped_count, 2);
    QCOMPARE(target.courses.size(), 2);

    // CSV 无 id，改用“名称 + 代码”判定重复
    const QByteArray csv = exporter.serialize(source, ScheduleFormat::Csv, &error);
    const ImportPreview csv_preview = importer.preview_data(csv, QStringLiteral("memory://csv"), target);
    QVERIFY(csv_preview.is_valid);
    const ImportResult csv_result = importer.apply(csv_preview, ImportStrategy::SkipDuplicates, &target);
    QVERIFY(csv_result.success);
    QCOMPARE(csv_result.skipped_count, 2);
    QCOMPARE(target.courses.size(), 2);
}

void TestImportExport::applies_overwrite_strategy() {
    ExportManager exporter;
    ImportManager importer;

    // 目标课表里先放一门“旧课”
    ScheduleSnapshot target = make_empty_snapshot();
    Course legacy;
    legacy.id = QStringLiteral("legacy");
    legacy.name = QStringLiteral("旧课");
    target.courses.append(legacy);

    const ScheduleSnapshot source = make_snapshot();
    QString error;
    const QByteArray document = exporter.serialize(source, ScheduleFormat::Json, &error);
    const ImportPreview preview = importer.preview_data(document, QStringLiteral("memory://json"), target);
    QVERIFY(preview.is_valid);

    const ImportResult result = importer.apply(preview, ImportStrategy::Overwrite, &target);
    QVERIFY2(result.success, qPrintable(result.error_message));
    QCOMPARE(result.imported_count, 2);
    QCOMPARE(target.courses.size(), 2);

    Course found;
    QVERIFY(!target.find_course(QStringLiteral("legacy"), &found));
    QVERIFY(target.find_course(QStringLiteral("math"), &found));

    // 覆盖导入时课程归属会被改写为当前学期
    QCOMPARE(found.semester_id, target.semester.id);

    // 无效预览被拒绝
    ImportPreview invalid;
    const ImportResult rejected = importer.apply(invalid, ImportStrategy::Merge, &target);
    QVERIFY(!rejected.success);
    QVERIFY(!rejected.error_message.isEmpty());
}

void TestImportExport::reports_errors() {
    ImportManager importer;

    const ImportPreview missing = importer.preview(QStringLiteral("Z:/definitely/not/here.json"), make_empty_snapshot());
    QVERIFY(!missing.is_valid);
    QVERIFY(missing.error_message.contains(QStringLiteral("不存在")));

    const ImportPreview empty = importer.preview(QString(), make_empty_snapshot());
    QVERIFY(!empty.is_valid);

    const ImportPreview unknown = importer.preview_data(QByteArray("just some text"), QStringLiteral("memory://x"), make_empty_snapshot());
    QVERIFY(!unknown.is_valid);
    QVERIFY(unknown.error_message.contains(QStringLiteral("无法识别")));

    // 非法 UTF-8（模拟 GBK 另存的文件）：必须给出可操作的提示
    QByteArray gbk_like;
    gbk_like.append(char(0xBF));
    gbk_like.append(char(0xCE));
    gbk_like.append(char(0xC3));
    gbk_like.append(',');
    gbk_like.append(char(0xD0));
    gbk_like.append(char(0xC7));
    gbk_like.append(char(0xC6));
    gbk_like.append(char(0xDA));
    gbk_like.append('\n');
    const ImportPreview bad_encoding = importer.preview_data(gbk_like, QStringLiteral("memory://gbk"), make_empty_snapshot());
    QVERIFY(!bad_encoding.is_valid);
    QVERIFY(bad_encoding.error_message.contains(QStringLiteral("UTF-8")));

    // CSV 缺少必需列
    const ImportPreview missing_columns = importer.preview_data(QByteArray("姓名,年龄\n张三,20\n"),
        QStringLiteral("memory://csv"),
        make_empty_snapshot());
    QVERIFY(!missing_columns.is_valid);
    QVERIFY(missing_columns.error_message.contains(QStringLiteral("缺少必需的列")));

    // ICS 缺少 VCALENDAR
    const ImportPreview not_ics = importer.preview_data(QByteArray("BEGIN:VEVENT\nEND:VEVENT\n"),
        QStringLiteral("memory://ics"),
        make_empty_snapshot());
    QVERIFY(!not_ics.is_valid);
}

void TestImportExport::parses_from_memory() {
    // 剪贴板 / 分享码导入预留入口：直接解析内存中的 JSON 文本
    ExportManager exporter;
    ImportManager importer;

    QString error;
    const QByteArray document = exporter.serialize(make_snapshot(), ScheduleFormat::Json, &error);
    const QString clipboard_text = QString::fromUtf8(document);

    const ImportPreview preview = importer.preview_data(clipboard_text.toUtf8(),
        QStringLiteral("clipboard://"),
        make_empty_snapshot());
    QVERIFY2(preview.is_valid, qPrintable(preview.error_message));
    QCOMPARE(preview.source_path, QStringLiteral("clipboard://"));
    QCOMPARE(preview.courses.size(), 2);

    // 注册自定义导入器后应能被识别（预留扩展点）
    QVERIFY(importer.supported_formats().size() >= 3);
    QCOMPARE(importer.importer_for_format(ScheduleFormat::Ics) != nullptr, true);
    QCOMPARE(importer.importer_for_format(ScheduleFormat::Unknown), nullptr);
}

QTEST_GUILESS_MAIN(TestImportExport)

#include "tst_import_export.moc"
