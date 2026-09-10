#include "core/model/Course.h"
#include "core/model/ScheduleSnapshot.h"
#include "core/model/Semester.h"
#include "core/model/TimeSlot.h"
#include "data/AppSettings.h"
#include "data/ImportSource.h"
#include "data/SettingsKeys.h"
#include "data/SqliteScheduleRepository.h"

#include <QFile>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>

using Schedule::AppSettings;
using Schedule::Course;
using Schedule::CourseSession;
using Schedule::ImportSource;
using Schedule::ScheduleSnapshot;
using Schedule::Semester;
using Schedule::SettingsKeys;
using Schedule::SqliteScheduleRepository;
using Schedule::TimeSlot;
using Schedule::WeekMask;

namespace {

    const QDate SEMESTER_START(2024, 9, 2);

    /** @return 构造一个快照；`semester_id` 用于区分不同学期。 */
    ScheduleSnapshot make_snapshot(const QString& semester_id, const QString& semester_name, const QDate& start_date, int course_count = 2) {
        ScheduleSnapshot snapshot;
        snapshot.semester.id = semester_id;
        snapshot.semester.name = semester_name;
        snapshot.semester.start_date = start_date;
        snapshot.semester.total_weeks = 16;
        snapshot.time_slots = TimeSlot::default_slots();

        for (int i = 0; i < course_count; ++i) {
            Course course;
            course.id = QStringLiteral("%1-course-%2").arg(semester_id).arg(i);
            course.semester_id = semester_id;
            course.name = QStringLiteral("课程 %1").arg(i);
            course.teacher = QStringLiteral("教师 %1").arg(i);
            course.credits = 2.0 + i;

            CourseSession session;
            session.id = QStringLiteral("%1-session-%2").arg(semester_id).arg(i);
            session.day_of_week = 1 + i;
            session.start_slot = 1 + i * 2;
            session.slot_count = 2;
            session.weeks = WeekMask::from_expression(QStringLiteral("1-16"), 16);
            course.sessions.append(session);

            snapshot.courses.append(course);
        }
        return snapshot;
    }

} // namespace

/**
 * @brief SQLite 仓库的单元测试：建表迁移、增删改查、当前学期唯一性、
 *        设置、导入记录、备份恢复与默认路径。
 */
class TestSqliteRepository : public QObject {
    Q_OBJECT

private slots:
    /** 打开内存库并完成建表迁移。 */
    void opens_and_migrates();

    /** 重复打开幂等（迁移不会重复执行）。 */
    void migration_is_idempotent();

    /** 建表后六张业务表齐全，可以直接读写。 */
    void creates_expected_tables();

    /** 版本 0 的空库能被迁移到最新版本。 */
    void migrates_from_empty_database();

    /** 数据库版本高于本实现支持上限时拒绝打开。 */
    void rejects_newer_schema_version();

    /** 快照往返不丢数据。 */
    void round_trips_snapshot();

    /** 保存快照是“整体替换”语义。 */
    void replaces_snapshot_content();

    /** 学期列表 / upsert / 级联删除。 */
    void manages_semesters();

    /** 当前学期唯一性。 */
    void keeps_single_current_semester();

    /** 设置读写与全量读取。 */
    void reads_and_writes_settings();

    /** 导入来源记录按时间倒序。 */
    void stores_import_sources();

    /** 备份与恢复。 */
    void backs_up_and_restores();

    /** 备份文件校验。 */
    void validates_backup_file();

    /** 未打开时操作都应失败而不是崩溃。 */
    void fails_gracefully_when_closed();

    /** 默认数据库路径指向用户数据目录。 */
    void provides_default_database_path();

    /** AppSettings 的门面行为。 */
    void applies_settings_facade();
};

void TestSqliteRepository::opens_and_migrates() {
    SqliteScheduleRepository repository(QStringLiteral(":memory:"));

    QString error;
    QVERIFY2(repository.open(&error), qPrintable(error));
    QVERIFY(repository.is_open());
    QCOMPARE(repository.location(), QStringLiteral(":memory:"));
    QCOMPARE(repository.schema_version(), SqliteScheduleRepository::latest_schema_version());
    QCOMPARE(repository.schema_version(), 1);

    // 全新库应为空
    QVERIFY(repository.load_semesters(&error).isEmpty());
    QCOMPARE(error, QString());

    bool found = true;
    const Semester current = repository.current_semester(&found);
    QVERIFY(!found);
    QVERIFY(current.id.isEmpty());

    repository.close();
    QVERIFY(!repository.is_open());
}

void TestSqliteRepository::migration_is_idempotent() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("schedule.db"));

    {
        SqliteScheduleRepository repository(path);
        QString error;
        QVERIFY2(repository.open(&error), qPrintable(error));
        QVERIFY(repository.save_snapshot(make_snapshot(QStringLiteral("s1"), QStringLiteral("学期一"), SEMESTER_START), &error));
    }
    {
        // 第二次打开不应重复建表，也不应丢数据
        SqliteScheduleRepository repository(path);
        QString error;
        QVERIFY2(repository.open(&error), qPrintable(error));
        QCOMPARE(repository.schema_version(), 1);

        ScheduleSnapshot snapshot;
        QVERIFY2(repository.load_snapshot(QStringLiteral("s1"), &snapshot, &error), qPrintable(error));
        QCOMPARE(snapshot.courses.size(), 2);
    }
}

void TestSqliteRepository::creates_expected_tables() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("schedule.db"));

    SqliteScheduleRepository repository(path);
    QString error;
    QVERIFY2(repository.open(&error), qPrintable(error));

    // 直接查 sqlite_master，确认六张业务表都已建立
    {
        QSqlDatabase probe = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("probe_tables"));
        probe.setDatabaseName(path);
        QVERIFY(probe.open());

        QStringList tables;
        QSqlQuery query(probe);
        QVERIFY(query.exec(QStringLiteral("SELECT name FROM sqlite_master WHERE type = 'table' ORDER BY name")));
        while (query.next()) {
            tables.append(query.value(0).toString());
        }
        probe.close();
        QSqlDatabase::removeDatabase(QStringLiteral("probe_tables"));

        for (const QString& expected : {QStringLiteral("semesters"), QStringLiteral("time_slots"),
                 QStringLiteral("courses"), QStringLiteral("course_sessions"),
                 QStringLiteral("settings"), QStringLiteral("import_sources")}) {
            QVERIFY2(tables.contains(expected), qPrintable(QStringLiteral("缺少表：") + expected));
        }
    }
}

void TestSqliteRepository::migrates_from_empty_database() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("schedule.db"));

    // 先建一个“版本 0”的空库（只有文件头，没有任何业务表）
    {
        QSqlDatabase probe = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("probe_v0"));
        probe.setDatabaseName(path);
        QVERIFY(probe.open());
        QSqlQuery query(probe);
        QVERIFY(query.exec(QStringLiteral("PRAGMA user_version = 0")));
        probe.close();
        QSqlDatabase::removeDatabase(QStringLiteral("probe_v0"));
    }

    // 打开时应自动迁移到最新版本，并且迁移后可正常写入
    SqliteScheduleRepository repository(path);
    QString error;
    QVERIFY2(repository.open(&error), qPrintable(error));
    QCOMPARE(repository.schema_version(), SqliteScheduleRepository::latest_schema_version());

    QVERIFY2(repository.save_snapshot(make_snapshot(QStringLiteral("s1"), QStringLiteral("学期一"), SEMESTER_START), &error),
        qPrintable(error));
    ScheduleSnapshot snapshot;
    QVERIFY2(repository.load_snapshot(QStringLiteral("s1"), &snapshot, &error), qPrintable(error));
    QCOMPARE(snapshot.courses.size(), 2);
}

void TestSqliteRepository::rejects_newer_schema_version() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("schedule.db"));

    // 正常建库后再把版本号改高，模拟“用旧版应用打开新版数据库”
    {
        SqliteScheduleRepository repository(path);
        QString error;
        QVERIFY2(repository.open(&error), qPrintable(error));
    }
    {
        QSqlDatabase probe = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("probe_future"));
        probe.setDatabaseName(path);
        QVERIFY(probe.open());
        QSqlQuery query(probe);
        QVERIFY(query.exec(QStringLiteral("PRAGMA user_version = %1").arg(SqliteScheduleRepository::latest_schema_version() + 1)));
        probe.close();
        QSqlDatabase::removeDatabase(QStringLiteral("probe_future"));
    }

    SqliteScheduleRepository repository(path);
    QString error;
    QVERIFY(!repository.open(&error));
    QVERIFY(error.contains(QStringLiteral("版本")));
    QVERIFY(!repository.is_open());
}

void TestSqliteRepository::round_trips_snapshot() {
    SqliteScheduleRepository repository(QStringLiteral(":memory:"));
    QString error;
    QVERIFY2(repository.open(&error), qPrintable(error));

    ScheduleSnapshot source = make_snapshot(QStringLiteral("s1"), QStringLiteral("2024-2025 第一学期"), SEMESTER_START, 3);
    source.semester.is_current = true;
    QVERIFY2(repository.save_snapshot(source, &error), qPrintable(error));

    ScheduleSnapshot restored;
    QVERIFY2(repository.load_snapshot(QStringLiteral("s1"), &restored, &error), qPrintable(error));

    QCOMPARE(restored.semester.id, source.semester.id);
    QCOMPARE(restored.semester.name, source.semester.name);
    QCOMPARE(restored.semester.start_date, SEMESTER_START);
    QCOMPARE(restored.semester.total_weeks, 16);
    QCOMPARE(restored.time_slots.size(), source.time_slots.size());
    QCOMPARE(restored.time_slots.first().start_time, QTime(8, 0));
    QCOMPARE(restored.courses.size(), 3);

    for (int i = 0; i < source.courses.size(); ++i) {
        QCOMPARE(restored.courses.at(i).id, source.courses.at(i).id);
        QCOMPARE(restored.courses.at(i).name, source.courses.at(i).name);
        QCOMPARE(restored.courses.at(i).credits, source.courses.at(i).credits);
        QCOMPARE(restored.courses.at(i).sessions.size(), 1);
        QCOMPARE(restored.courses.at(i).sessions.first().weeks, WeekMask::from_expression(QStringLiteral("1-16"), 16));
        QCOMPARE(restored.courses.at(i).sessions.first().day_of_week, source.courses.at(i).sessions.first().day_of_week);
    }
}

void TestSqliteRepository::replaces_snapshot_content() {
    SqliteScheduleRepository repository(QStringLiteral(":memory:"));
    QString error;
    QVERIFY2(repository.open(&error), qPrintable(error));

    QVERIFY(repository.save_snapshot(make_snapshot(QStringLiteral("s1"), QStringLiteral("学期一"), SEMESTER_START, 3), &error));
    // 再次保存同一学期但课程更少：应整体替换，而不是累加
    QVERIFY(repository.save_snapshot(make_snapshot(QStringLiteral("s1"), QStringLiteral("学期一"), SEMESTER_START, 1), &error));

    ScheduleSnapshot restored;
    QVERIFY2(repository.load_snapshot(QStringLiteral("s1"), &restored, &error), qPrintable(error));
    QCOMPARE(restored.courses.size(), 1);

    // 另一个学期互不影响
    QVERIFY(repository.save_snapshot(make_snapshot(QStringLiteral("s2"), QStringLiteral("学期二"), QDate(2025, 2, 17), 2), &error));
    QCOMPARE(repository.load_semesters(&error).size(), 2);
    QVERIFY2(repository.load_snapshot(QStringLiteral("s1"), &restored, &error), qPrintable(error));
    QCOMPARE(restored.courses.size(), 1);
}

void TestSqliteRepository::manages_semesters() {
    SqliteScheduleRepository repository(QStringLiteral(":memory:"));
    QString error;
    QVERIFY2(repository.open(&error), qPrintable(error));

    QVERIFY(repository.save_snapshot(make_snapshot(QStringLiteral("s2"), QStringLiteral("第二学期"), QDate(2025, 2, 17)), &error));
    QVERIFY(repository.save_snapshot(make_snapshot(QStringLiteral("s1"), QStringLiteral("第一学期"), SEMESTER_START), &error));

    // 按起始日期升序
    const QList<Semester> semesters = repository.load_semesters(&error);
    QCOMPARE(semesters.size(), 2);
    QCOMPARE(semesters.at(0).id, QStringLiteral("s1"));
    QCOMPARE(semesters.at(1).id, QStringLiteral("s2"));

    // upsert：改名字不产生新行
    Semester renamed = semesters.at(0);
    renamed.name = QStringLiteral("第一学期（改名）");
    QVERIFY2(repository.save_semester(renamed, &error), qPrintable(error));
    QCOMPARE(repository.load_semesters(&error).size(), 2);
    QCOMPARE(repository.load_semesters(&error).at(0).name, QStringLiteral("第一学期（改名）"));

    // 非法学期被拒绝
    Semester invalid;
    invalid.name = QStringLiteral("缺少日期");
    QVERIFY(!repository.save_semester(invalid, &error));
    QVERIFY(!error.isEmpty());

    // 级联删除：课程与作息表一并消失
    QVERIFY2(repository.remove_semester(QStringLiteral("s1"), &error), qPrintable(error));
    QCOMPARE(repository.load_semesters(&error).size(), 1);

    ScheduleSnapshot snapshot;
    QVERIFY(!repository.load_snapshot(QStringLiteral("s1"), &snapshot, &error));
    QVERIFY(!error.isEmpty());
}

void TestSqliteRepository::keeps_single_current_semester() {
    SqliteScheduleRepository repository(QStringLiteral(":memory:"));
    QString error;
    QVERIFY2(repository.open(&error), qPrintable(error));

    QVERIFY(repository.save_snapshot(make_snapshot(QStringLiteral("s1"), QStringLiteral("第一学期"), SEMESTER_START), &error));
    QVERIFY(repository.save_snapshot(make_snapshot(QStringLiteral("s2"), QStringLiteral("第二学期"), QDate(2025, 2, 17)), &error));

    QVERIFY2(repository.set_current_semester(QStringLiteral("s1"), &error), qPrintable(error));

    bool found = false;
    Semester current = repository.current_semester(&found);
    QVERIFY(found);
    QCOMPARE(current.id, QStringLiteral("s1"));

    QVERIFY2(repository.set_current_semester(QStringLiteral("s2"), &error), qPrintable(error));
    current = repository.current_semester(&found);
    QVERIFY(found);
    QCOMPARE(current.id, QStringLiteral("s2"));

    // 无论切换多少次，最多只有一个 is_current
    int current_count = 0;
    for (const Semester& semester : repository.load_semesters(&error)) {
        if (semester.is_current) {
            ++current_count;
        }
    }
    QCOMPARE(current_count, 1);

    // 设置项同步写入，便于不查表快速读取
    QCOMPARE(repository.setting(SettingsKeys::current_semester_id()), QStringLiteral("s2"));
}

void TestSqliteRepository::reads_and_writes_settings() {
    SqliteScheduleRepository repository(QStringLiteral(":memory:"));
    QString error;
    QVERIFY2(repository.open(&error), qPrintable(error));

    QCOMPARE(repository.setting(QStringLiteral("missing"), QStringLiteral("fallback")), QStringLiteral("fallback"));
    QCOMPARE(repository.setting(QStringLiteral("missing")), QString());

    QVERIFY2(repository.set_setting(SettingsKeys::default_export_dir(), QStringLiteral("D:/Schedule"), &error), qPrintable(error));
    QCOMPARE(repository.setting(SettingsKeys::default_export_dir()), QStringLiteral("D:/Schedule"));

    // 覆盖写
    QVERIFY(repository.set_setting(SettingsKeys::default_export_dir(), QStringLiteral("E:/Out"), &error));
    QCOMPARE(repository.setting(SettingsKeys::default_export_dir()), QStringLiteral("E:/Out"));

    QCOMPARE(repository.all_settings(&error).size(), 1);
    QVERIFY(!repository.set_setting(QString(), QStringLiteral("x"), &error));
    QVERIFY(!error.isEmpty());
}

void TestSqliteRepository::stores_import_sources() {
    SqliteScheduleRepository repository(QStringLiteral(":memory:"));
    QString error;
    QVERIFY2(repository.open(&error), qPrintable(error));

    ImportSource older;
    older.id = QStringLiteral("i1");
    older.file_path = QStringLiteral("D:/Schedule/old.json");
    older.format = QStringLiteral("json");
    older.imported_at = QDateTime(QDate(2024, 9, 1), QTime(10, 0));
    older.course_count = 3;

    ImportSource newer;
    newer.id = QStringLiteral("i2");
    newer.file_path = QStringLiteral("D:/Schedule/new.csv");
    newer.format = QStringLiteral("csv");
    newer.imported_at = QDateTime(QDate(2024, 9, 5), QTime(10, 0));
    newer.course_count = 5;
    newer.note = QStringLiteral("合并导入");

    QVERIFY2(repository.add_import_source(older, &error), qPrintable(error));
    QVERIFY2(repository.add_import_source(newer, &error), qPrintable(error));

    const QList<ImportSource> sources = repository.load_import_sources(&error);
    QCOMPARE(sources.size(), 2);
    QCOMPARE(sources.at(0).id, QStringLiteral("i2")); // 最近的在前
    QCOMPARE(sources.at(0).note, QStringLiteral("合并导入"));
    QCOMPARE(sources.at(1).format, QStringLiteral("json"));

    // 缺少 id / 时间时自动补齐
    ImportSource minimal;
    minimal.file_path = QStringLiteral("D:/Schedule/auto.ics");
    minimal.format = QStringLiteral("ics");
    QVERIFY(repository.add_import_source(minimal, &error));
    const QList<ImportSource> after = repository.load_import_sources(&error);
    QCOMPARE(after.size(), 3);
    QVERIFY(!after.first().id.isEmpty());
    QVERIFY(after.first().imported_at.isValid());
}

void TestSqliteRepository::backs_up_and_restores() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString database_path = directory.filePath(QStringLiteral("schedule.db"));
    const QString backup_path = directory.filePath(QStringLiteral("backup/schedule-backup.db"));

    SqliteScheduleRepository repository(database_path);
    QString error;
    QVERIFY2(repository.open(&error), qPrintable(error));
    QVERIFY(repository.save_snapshot(make_snapshot(QStringLiteral("s1"), QStringLiteral("学期一"), SEMESTER_START, 2), &error));

    // 备份到指定目录（父目录自动创建）
    QVERIFY2(repository.backup_to(backup_path, &error), qPrintable(error));
    QVERIFY(QFile::exists(backup_path));
    QVERIFY(!repository.backup_to(database_path, &error)); // 不能覆盖自身

    // 破坏当前数据
    QVERIFY(repository.save_snapshot(make_snapshot(QStringLiteral("s1"), QStringLiteral("学期一"), SEMESTER_START, 5), &error));
    ScheduleSnapshot snapshot;
    QVERIFY(repository.load_snapshot(QStringLiteral("s1"), &snapshot, &error));
    QCOMPARE(snapshot.courses.size(), 5);

    // 恢复后回到备份时刻的状态
    QVERIFY2(repository.restore_from(backup_path, &error), qPrintable(error));
    QVERIFY(repository.is_open());
    QVERIFY2(repository.load_snapshot(QStringLiteral("s1"), &snapshot, &error), qPrintable(error));
    QCOMPARE(snapshot.courses.size(), 2);

    // 恢复前会自动留一份 .bak
    QVERIFY(QFile::exists(database_path + QStringLiteral(".bak")));
}

void TestSqliteRepository::validates_backup_file() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    QString error;
    QVERIFY(!SqliteScheduleRepository::is_valid_database_file(directory.filePath(QStringLiteral("missing.db")), &error));
    QVERIFY(!error.isEmpty());

    // 非数据库文件
    const QString text_path = directory.filePath(QStringLiteral("not-a-db.db"));
    QFile text_file(text_path);
    QVERIFY(text_file.open(QIODevice::WriteOnly));
    text_file.write("this is not a sqlite database");
    text_file.close();
    QVERIFY(!SqliteScheduleRepository::is_valid_database_file(text_path, &error));
    QVERIFY(!error.isEmpty());

    // 正常数据库
    const QString database_path = directory.filePath(QStringLiteral("schedule.db"));
    SqliteScheduleRepository repository(database_path);
    QVERIFY2(repository.open(&error), qPrintable(error));
    repository.close();
    QVERIFY2(SqliteScheduleRepository::is_valid_database_file(database_path, &error), qPrintable(error));

    // 内存库不支持恢复
    SqliteScheduleRepository memory_repository(QStringLiteral(":memory:"));
    QVERIFY(memory_repository.open(&error));
    QVERIFY(!memory_repository.restore_from(database_path, &error));
    QVERIFY(error.contains(QStringLiteral("内存数据库")));
}

void TestSqliteRepository::fails_gracefully_when_closed() {
    SqliteScheduleRepository repository(QStringLiteral(":memory:"));
    QString error;

    QVERIFY(repository.load_semesters(&error).isEmpty());
    QVERIFY(error.contains(QStringLiteral("尚未打开")));

    QVERIFY(!repository.save_snapshot(make_snapshot(QStringLiteral("s1"), QStringLiteral("学期"), SEMESTER_START), &error));
    QVERIFY(!error.isEmpty());

    QVERIFY(!repository.save_semester(Semester::create(QStringLiteral("学期"), SEMESTER_START, 16), &error));
    QVERIFY(!repository.set_setting(QStringLiteral("k"), QStringLiteral("v"), &error));
    QVERIFY(repository.all_settings(&error).isEmpty());
    QVERIFY(repository.load_import_sources(&error).isEmpty());
    QCOMPARE(repository.schema_version(), 0);

    // 空路径与非法驱动场景（注意：直接写 QString() 会被解析成函数声明）
    const QString empty_path;
    SqliteScheduleRepository invalid_path(empty_path);
    QVERIFY(!invalid_path.open(&error));
    QVERIFY(!error.isEmpty());
}

void TestSqliteRepository::provides_default_database_path() {
    const QString path = SqliteScheduleRepository::default_database_path();
    QVERIFY(path.endsWith(QStringLiteral("schedule.db")));
    QVERIFY(!path.isEmpty());
}

void TestSqliteRepository::applies_settings_facade() {
    SqliteScheduleRepository repository(QStringLiteral(":memory:"));
    QString error;
    QVERIFY2(repository.open(&error), qPrintable(error));

    AppSettings settings(&repository);

    // 默认目录规则：Documents/Schedule
    QVERIFY(settings.default_import_dir().endsWith(QStringLiteral("Schedule")));
    QVERIFY(settings.default_export_dir().endsWith(QStringLiteral("Schedule")));
    QCOMPARE(settings.default_import_dir(), AppSettings::fallback_directory());

    QVERIFY(settings.set_default_import_dir(QStringLiteral("D:/Import"), &error));
    QCOMPARE(settings.default_import_dir(), QStringLiteral("D:/Import"));
    QCOMPARE(settings.last_import_dir(), QStringLiteral("D:/Import")); // 未记录时回退默认

    QVERIFY(settings.set_last_export_dir(QStringLiteral("E:/Export"), &error));
    QCOMPARE(settings.last_export_dir(), QStringLiteral("E:/Export"));

    // 提醒设置
    QVERIFY(settings.reminder_enabled());
    QCOMPARE(settings.reminder_minutes(), AppSettings::default_reminder_minutes());
    QVERIFY(settings.set_reminder_minutes(15, &error));
    QCOMPARE(settings.reminder_minutes(), 15);
    QVERIFY(!settings.set_reminder_minutes(7, &error)); // 只允许 5/10/15
    QVERIFY(!error.isEmpty());

    QVERIFY(settings.set_reminder_enabled(false, &error));
    QVERIFY(!settings.reminder_enabled());

    // 主题：非法值回退 system
    QVERIFY(settings.set_theme(QStringLiteral("dark"), &error));
    QCOMPARE(settings.theme(), QStringLiteral("dark"));
    QVERIFY(settings.set_theme(QStringLiteral("neon"), &error));
    QCOMPARE(settings.theme(), QStringLiteral("system"));

    // 当前学期 id
    QVERIFY(settings.set_current_semester_id(QStringLiteral("s1"), &error));
    QCOMPARE(settings.current_semester_id(), QStringLiteral("s1"));

    // 目录创建
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString created = AppSettings::ensure_directory(directory.filePath(QStringLiteral("a/b/c")), &error);
    QVERIFY(!created.isEmpty());
    QVERIFY(QFile::exists(created));

    // 没有仓库时读取返回默认值、写入失败（不崩溃）
    AppSettings detached;
    QCOMPARE(detached.reminder_minutes(), AppSettings::default_reminder_minutes());
    QVERIFY(!detached.set_reminder_minutes(5, &error));
    QVERIFY(!error.isEmpty());
}

QTEST_GUILESS_MAIN(TestSqliteRepository)

#include "tst_sqlite_repository.moc"
