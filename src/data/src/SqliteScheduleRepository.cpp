#include "data/SqliteScheduleRepository.h"

#include "core/model/WeekMask.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QStandardPaths>
#include <QUuid>

namespace Schedule {

    namespace {

        /** 单条迁移步骤：版本号 + 需要按顺序执行的语句列表。 */
        struct MigrationStep {
            int version;
            QStringList statements;
        };

        /**
         * @brief schema 迁移脚本。
         *
         * 规则：**只追加、不修改**已发布的步骤。已发布步骤一旦改动，老用户的数据库
         * 会与 `user_version` 记录不一致，导致难以排查的“部分升级”问题。
         */
        QList<MigrationStep> migration_steps() {
            QList<MigrationStep> steps;

            MigrationStep version_1;
            version_1.version = 1;
            version_1.statements = QStringList{
                // 学期
                QStringLiteral("CREATE TABLE IF NOT EXISTS semesters ("
                               "  id TEXT PRIMARY KEY,"
                               "  name TEXT NOT NULL DEFAULT '',"
                               "  start_date TEXT NOT NULL DEFAULT '',"
                               "  total_weeks INTEGER NOT NULL DEFAULT 20,"
                               "  is_current INTEGER NOT NULL DEFAULT 0,"
                               "  created_at TEXT NOT NULL DEFAULT '',"
                               "  updated_at TEXT NOT NULL DEFAULT ''"
                               ")"),
                // 作息表：主键 (semester_id, slot_index)，随学期级联删除
                QStringLiteral("CREATE TABLE IF NOT EXISTS time_slots ("
                               "  semester_id TEXT NOT NULL,"
                               "  slot_index INTEGER NOT NULL,"
                               "  label TEXT NOT NULL DEFAULT '',"
                               "  start_time TEXT NOT NULL DEFAULT '',"
                               "  end_time TEXT NOT NULL DEFAULT '',"
                               "  PRIMARY KEY (semester_id, slot_index),"
                               "  FOREIGN KEY (semester_id) REFERENCES semesters(id) ON DELETE CASCADE"
                               ")"),
                // 课程
                QStringLiteral("CREATE TABLE IF NOT EXISTS courses ("
                               "  id TEXT PRIMARY KEY,"
                               "  semester_id TEXT NOT NULL,"
                               "  name TEXT NOT NULL DEFAULT '',"
                               "  code TEXT NOT NULL DEFAULT '',"
                               "  teacher TEXT NOT NULL DEFAULT '',"
                               "  location TEXT NOT NULL DEFAULT '',"
                               "  color TEXT NOT NULL DEFAULT '',"
                               "  credits REAL NOT NULL DEFAULT 0,"
                               "  notes TEXT NOT NULL DEFAULT '',"
                               "  FOREIGN KEY (semester_id) REFERENCES semesters(id) ON DELETE CASCADE"
                               ")"),
                QStringLiteral("CREATE INDEX IF NOT EXISTS idx_courses_semester ON courses(semester_id)"),
                // 上课时间段；week_bits 用十进制字符串保存，避免 64 位无符号数溢出有符号整型
                QStringLiteral("CREATE TABLE IF NOT EXISTS course_sessions ("
                               "  id TEXT PRIMARY KEY,"
                               "  course_id TEXT NOT NULL,"
                               "  day_of_week INTEGER NOT NULL DEFAULT 1,"
                               "  start_slot INTEGER NOT NULL DEFAULT 1,"
                               "  slot_count INTEGER NOT NULL DEFAULT 1,"
                               "  week_bits TEXT NOT NULL DEFAULT '0',"
                               "  week_expression TEXT NOT NULL DEFAULT '',"
                               "  location TEXT NOT NULL DEFAULT '',"
                               "  teacher TEXT NOT NULL DEFAULT '',"
                               "  FOREIGN KEY (course_id) REFERENCES courses(id) ON DELETE CASCADE"
                               ")"),
                QStringLiteral("CREATE INDEX IF NOT EXISTS idx_sessions_course ON course_sessions(course_id)"),
                // 键值设置
                QStringLiteral("CREATE TABLE IF NOT EXISTS settings ("
                               "  setting_key TEXT PRIMARY KEY,"
                               "  setting_value TEXT NOT NULL DEFAULT ''"
                               ")"),
                // 导入来源留痕（纯本地，不含账号 / Token）
                QStringLiteral("CREATE TABLE IF NOT EXISTS import_sources ("
                               "  id TEXT PRIMARY KEY,"
                               "  file_path TEXT NOT NULL DEFAULT '',"
                               "  format TEXT NOT NULL DEFAULT '',"
                               "  imported_at TEXT NOT NULL DEFAULT '',"
                               "  course_count INTEGER NOT NULL DEFAULT 0,"
                               "  note TEXT NOT NULL DEFAULT ''"
                               ")"),
            };
            steps.append(version_1);

            return steps;
        }

        /** @return 用单引号包裹并转义后的 SQL 字符串字面量。 */
        QString sql_quote(const QString& text) {
            QString escaped = text;
            escaped.replace(QLatin1Char('\''), QStringLiteral("''"));
            return QStringLiteral("'%1'").arg(escaped);
        }

        /** @brief 统一设置错误。 */
        bool fail(QString* error_message, const QString& message) {
            if (error_message) {
                *error_message = message;
            }
            return false;
        }

        /**
         * @brief 把 null QString 归一化为空串。
         *
         * `QString()` 是 null，Qt 的 QSQLITE 驱动会把它绑定为 SQL NULL；
         * 而本 schema 中所有 TEXT 列都声明了 `NOT NULL DEFAULT ''`，
         * 直接绑定 null 会触发 "NOT NULL constraint failed"。
         * 因此所有字符串绑定都必须经过本函数。
         */
        QString non_null(const QString& text) {
            return text.isNull() ? QStringLiteral("") : text;
        }

    } // namespace

    SqliteScheduleRepository::SqliteScheduleRepository(QString database_path, QString connection_name)
        : m_database_path(database_path)
        , m_connection_name(connection_name) {
        if (m_connection_name.isEmpty()) {
            m_connection_name = QStringLiteral("schedule_repo_%1")
                                    .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
        }
    }

    SqliteScheduleRepository::~SqliteScheduleRepository() {
        close();
    }

    // ------------------------------------------------------------------ 生命周期

    bool SqliteScheduleRepository::open(QString* error_message) {
        if (m_open) {
            return true;
        }
        if (m_database_path.isEmpty()) {
            return fail(error_message, QStringLiteral("数据库路径为空"));
        }
        if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE"))) {
            return fail(error_message, QStringLiteral("当前 Qt 未提供 QSQLITE 驱动，无法使用数据库存储"));
        }

        // 文件型数据库：确保父目录存在（首次启动时目录往往还不存在）
        if (m_database_path != QStringLiteral(":memory:")) {
            const QDir parent = QFileInfo(m_database_path).absoluteDir();
            if (!parent.exists() && !parent.mkpath(QStringLiteral("."))) {
                return fail(error_message, QStringLiteral("无法创建数据库目录：%1").arg(parent.absolutePath()));
            }
        }

        // close() 会清空连接名以便重新打开；这里在需要时重新生成，避免退化成默认连接
        if (m_connection_name.isEmpty()) {
            m_connection_name = QStringLiteral("schedule_repo_%1")
                                    .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
        }

        m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connection_name);
        m_database.setDatabaseName(m_database_path);
        if (!m_database.open()) {
            const QString reason = m_database.lastError().text();
            close();
            return fail(error_message, QStringLiteral("无法打开数据库 %1：%2").arg(m_database_path, reason));
        }

        // 外键约束默认关闭，必须显式打开才能让 ON DELETE CASCADE 生效
        if (!exec(QStringLiteral("PRAGMA foreign_keys = ON"), error_message)) {
            close();
            return false;
        }
        if (!exec(QStringLiteral("PRAGMA journal_mode = WAL"), error_message)) {
            // WAL 不可用（例如只读介质）不影响正确性，降级即可，不作为失败。
            if (error_message) {
                error_message->clear();
            }
        }

        if (!migrate(error_message)) {
            close();
            return false;
        }

        m_open = true;
        if (error_message) {
            error_message->clear();
        }
        return true;
    }

    void SqliteScheduleRepository::close() {
        if (m_transaction_active) {
            rollback_transaction();
        }
        if (m_database.isValid()) {
            m_database.close();
        }
        m_database = QSqlDatabase();
        if (!m_connection_name.isEmpty()) {
            QSqlDatabase::removeDatabase(m_connection_name);
            m_connection_name.clear();
        }
        m_open = false;
    }

    bool SqliteScheduleRepository::is_open() const {
        return m_open;
    }

    QString SqliteScheduleRepository::location() const {
        return m_database_path;
    }

    int SqliteScheduleRepository::schema_version() const {
        return m_open ? read_user_version() : 0;
    }

    int SqliteScheduleRepository::latest_schema_version() {
        int latest = 0;
        for (const MigrationStep& step : migration_steps()) {
            latest = qMax(latest, step.version);
        }
        return latest;
    }

    QString SqliteScheduleRepository::default_database_path() {
        const QString directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        if (directory.isEmpty()) {
            // 极端情况下（无 HOME 的精简容器）回退到当前目录，保证应用仍可启动
            return QDir::current().filePath(QStringLiteral("schedule.db"));
        }
        return QDir(directory).filePath(QStringLiteral("schedule.db"));
    }

    // ---------------------------------------------------------------------- SQL

    bool SqliteScheduleRepository::exec(const QString& sql, QString* error_message) const {
        QSqlQuery query(m_database);
        if (!query.exec(sql)) {
            return fail(error_message, QStringLiteral("SQL 执行失败：%1\n语句：%2").arg(query.lastError().text(), sql));
        }
        return true;
    }

    int SqliteScheduleRepository::read_user_version() const {
        QSqlQuery query(m_database);
        if (!query.exec(QStringLiteral("PRAGMA user_version")) || !query.next()) {
            return -1;
        }
        return query.value(0).toInt();
    }

    bool SqliteScheduleRepository::migrate(QString* error_message) {
        const int current = read_user_version();
        if (current < 0) {
            return fail(error_message, QStringLiteral("无法读取数据库版本号（PRAGMA user_version）"));
        }
        if (current > latest_schema_version()) {
            return fail(error_message,
                QStringLiteral("数据库版本 %1 高于当前应用支持的 %2，请升级应用后再打开")
                    .arg(current)
                    .arg(latest_schema_version()));
        }

        for (const MigrationStep& step : migration_steps()) {
            if (step.version <= current) {
                continue;
            }
            // 每个版本一个事务：中途失败即整体回滚，不会留下“半升级”的库。
            if (!m_database.transaction()) {
                return fail(error_message, QStringLiteral("无法开启迁移事务：%1").arg(m_database.lastError().text()));
            }
            for (const QString& statement : step.statements) {
                QString statement_error;
                if (!exec(statement, &statement_error)) {
                    m_database.rollback();
                    return fail(error_message, QStringLiteral("迁移到版本 %1 失败：%2").arg(step.version).arg(statement_error));
                }
            }
            if (!exec(QStringLiteral("PRAGMA user_version = %1").arg(step.version), error_message)) {
                m_database.rollback();
                return false;
            }
            if (!m_database.commit()) {
                m_database.rollback();
                return fail(error_message, QStringLiteral("提交迁移事务失败：%1").arg(m_database.lastError().text()));
            }
        }

        if (error_message) {
            error_message->clear();
        }
        return true;
    }

    bool SqliteScheduleRepository::begin_transaction(QString* error_message) {
        if (m_transaction_active) {
            // 已经在事务中：复用外层事务，由外层统一提交 / 回滚
            return true;
        }
        if (!m_database.transaction()) {
            return fail(error_message, QStringLiteral("无法开启事务：%1").arg(m_database.lastError().text()));
        }
        m_transaction_active = true;
        return true;
    }

    bool SqliteScheduleRepository::commit_transaction(QString* error_message) {
        if (!m_transaction_active) {
            // 事务由外层持有，提交交给外层
            return true;
        }
        m_transaction_active = false;
        if (!m_database.commit()) {
            const QString reason = m_database.lastError().text();
            m_database.rollback();
            return fail(error_message, QStringLiteral("提交事务失败：%1").arg(reason));
        }
        return true;
    }

    void SqliteScheduleRepository::rollback_transaction() {
        if (!m_transaction_active) {
            return;
        }
        m_transaction_active = false;
        m_database.rollback();
    }

    // -------------------------------------------------------------------- 学期

    QList<Semester> SqliteScheduleRepository::load_semesters(QString* error_message) const {
        QList<Semester> semesters;
        if (!m_open) {
            fail(error_message, QStringLiteral("数据库尚未打开"));
            return semesters;
        }

        QSqlQuery query(m_database);
        if (!query.exec(QStringLiteral("SELECT id, name, start_date, total_weeks, is_current, created_at, updated_at "
                                       "FROM semesters ORDER BY start_date ASC, name ASC"))) {
            fail(error_message, QStringLiteral("读取学期失败：%1").arg(query.lastError().text()));
            return semesters;
        }

        while (query.next()) {
            Semester semester;
            semester.id = query.value(0).toString();
            semester.name = query.value(1).toString();
            semester.start_date = QDate::fromString(query.value(2).toString(), Qt::ISODate);
            semester.total_weeks = query.value(3).toInt();
            semester.is_current = query.value(4).toInt() != 0;
            semester.created_at = QDateTime::fromString(query.value(5).toString(), Qt::ISODate);
            semester.updated_at = QDateTime::fromString(query.value(6).toString(), Qt::ISODate);
            semesters.append(semester);
        }

        if (error_message) {
            error_message->clear();
        }
        return semesters;
    }

    bool SqliteScheduleRepository::save_semester(const Semester& semester, QString* error_message) {
        if (!m_open) {
            return fail(error_message, QStringLiteral("数据库尚未打开"));
        }
        if (!semester.is_valid(error_message)) {
            return false;
        }

        Semester stored = semester;
        if (!stored.created_at.isValid()) {
            stored.created_at = QDateTime::currentDateTime();
        }
        stored.updated_at = QDateTime::currentDateTime();

        QSqlQuery query(m_database);
        query.prepare(QStringLiteral(
            "INSERT INTO semesters (id, name, start_date, total_weeks, is_current, created_at, updated_at) "
            "VALUES (:id, :name, :start_date, :total_weeks, :is_current, :created_at, :updated_at) "
            "ON CONFLICT(id) DO UPDATE SET name = excluded.name, start_date = excluded.start_date, "
            "total_weeks = excluded.total_weeks, is_current = excluded.is_current, updated_at = excluded.updated_at"));
        query.bindValue(QStringLiteral(":id"), non_null(stored.id));
        query.bindValue(QStringLiteral(":name"), non_null(stored.name));
        query.bindValue(QStringLiteral(":start_date"), non_null(stored.start_date.toString(Qt::ISODate)));
        query.bindValue(QStringLiteral(":total_weeks"), stored.total_weeks);
        query.bindValue(QStringLiteral(":is_current"), stored.is_current ? 1 : 0);
        query.bindValue(QStringLiteral(":created_at"), non_null(stored.created_at.toString(Qt::ISODate)));
        query.bindValue(QStringLiteral(":updated_at"), non_null(stored.updated_at.toString(Qt::ISODate)));

        if (!query.exec()) {
            return fail(error_message, QStringLiteral("保存学期失败：%1").arg(query.lastError().text()));
        }

        // is_current 在表级别只能有一个：交由 set_current_semester 统一维护，
        // 这里若发现本行被标记为当前学期，则把其它行清零。
        if (stored.is_current && !set_current_semester(stored.id, error_message)) {
            return false;
        }

        if (error_message) {
            error_message->clear();
        }
        return true;
    }

    bool SqliteScheduleRepository::remove_semester(const QString& semester_id, QString* error_message) {
        if (!m_open) {
            return fail(error_message, QStringLiteral("数据库尚未打开"));
        }

        if (!begin_transaction(error_message)) {
            return false;
        }

        QSqlQuery query(m_database);
        query.prepare(QStringLiteral("DELETE FROM semesters WHERE id = :id"));
        query.bindValue(QStringLiteral(":id"), non_null(semester_id));
        if (!query.exec()) {
            const QString reason = query.lastError().text();
            rollback_transaction();
            return fail(error_message, QStringLiteral("删除学期失败：%1").arg(reason));
        }

        if (!commit_transaction(error_message)) {
            return false;
        }

        if (error_message) {
            error_message->clear();
        }
        return true;
    }

    bool SqliteScheduleRepository::set_current_semester(const QString& semester_id, QString* error_message) {
        if (!m_open) {
            return fail(error_message, QStringLiteral("数据库尚未打开"));
        }

        if (!begin_transaction(error_message)) {
            return false;
        }

        // 先全部清零再置位，保证“同一时刻最多一个当前学期”这一不变量。
        if (!exec(QStringLiteral("UPDATE semesters SET is_current = 0 WHERE is_current <> 0"), error_message)) {
            rollback_transaction();
            return false;
        }

        QSqlQuery query(m_database);
        query.prepare(QStringLiteral("UPDATE semesters SET is_current = 1 WHERE id = :id"));
        query.bindValue(QStringLiteral(":id"), non_null(semester_id));
        if (!query.exec()) {
            const QString reason = query.lastError().text();
            rollback_transaction();
            return fail(error_message, QStringLiteral("设置当前学期失败：%1").arg(reason));
        }

        // 同步一份设置项，便于不查表即可快速读取
        QSqlQuery setting_query(m_database);
        setting_query.prepare(QStringLiteral("INSERT INTO settings (setting_key, setting_value) VALUES (:key, :value) "
                                             "ON CONFLICT(setting_key) DO UPDATE SET setting_value = excluded.setting_value"));
        setting_query.bindValue(QStringLiteral(":key"), non_null(QStringLiteral("schedule/current_semester_id")));
        setting_query.bindValue(QStringLiteral(":value"), non_null(semester_id));
        if (!setting_query.exec()) {
            const QString reason = setting_query.lastError().text();
            rollback_transaction();
            return fail(error_message, QStringLiteral("写入当前学期设置失败：%1").arg(reason));
        }

        if (!commit_transaction(error_message)) {
            return false;
        }

        if (error_message) {
            error_message->clear();
        }
        return true;
    }

    Semester SqliteScheduleRepository::current_semester(bool* found) const {
        if (found) {
            *found = false;
        }
        if (!m_open) {
            return Semester();
        }

        QSqlQuery query(m_database);
        if (!query.exec(QStringLiteral("SELECT id, name, start_date, total_weeks, is_current, created_at, updated_at "
                                       "FROM semesters WHERE is_current <> 0 ORDER BY start_date DESC LIMIT 1"))) {
            return Semester();
        }
        if (!query.next()) {
            return Semester();
        }

        Semester semester;
        semester.id = query.value(0).toString();
        semester.name = query.value(1).toString();
        semester.start_date = QDate::fromString(query.value(2).toString(), Qt::ISODate);
        semester.total_weeks = query.value(3).toInt();
        semester.is_current = true;
        semester.created_at = QDateTime::fromString(query.value(5).toString(), Qt::ISODate);
        semester.updated_at = QDateTime::fromString(query.value(6).toString(), Qt::ISODate);

        if (found) {
            *found = true;
        }
        return semester;
    }

    // -------------------------------------------------------------------- 快照

    bool SqliteScheduleRepository::load_snapshot(const QString& semester_id, ScheduleSnapshot* out_snapshot, QString* error_message) const {
        if (!out_snapshot) {
            return fail(error_message, QStringLiteral("输出参数为空"));
        }
        if (!m_open) {
            return fail(error_message, QStringLiteral("数据库尚未打开"));
        }

        ScheduleSnapshot snapshot;

        QSqlQuery semester_query(m_database);
        semester_query.prepare(QStringLiteral(
            "SELECT id, name, start_date, total_weeks, is_current, created_at, updated_at FROM semesters WHERE id = :id"));
        semester_query.bindValue(QStringLiteral(":id"), semester_id);
        if (!semester_query.exec()) {
            return fail(error_message, QStringLiteral("读取学期失败：%1").arg(semester_query.lastError().text()));
        }
        if (!semester_query.next()) {
            return fail(error_message, QStringLiteral("未找到学期：%1").arg(semester_id));
        }
        snapshot.semester.id = semester_query.value(0).toString();
        snapshot.semester.name = semester_query.value(1).toString();
        snapshot.semester.start_date = QDate::fromString(semester_query.value(2).toString(), Qt::ISODate);
        snapshot.semester.total_weeks = semester_query.value(3).toInt();
        snapshot.semester.is_current = semester_query.value(4).toInt() != 0;
        snapshot.semester.created_at = QDateTime::fromString(semester_query.value(5).toString(), Qt::ISODate);
        snapshot.semester.updated_at = QDateTime::fromString(semester_query.value(6).toString(), Qt::ISODate);

        // 作息表
        QSqlQuery slot_query(m_database);
        slot_query.prepare(QStringLiteral("SELECT slot_index, label, start_time, end_time FROM time_slots "
                                          "WHERE semester_id = :id ORDER BY slot_index ASC"));
        slot_query.bindValue(QStringLiteral(":id"), semester_id);
        if (!slot_query.exec()) {
            return fail(error_message, QStringLiteral("读取作息表失败：%1").arg(slot_query.lastError().text()));
        }
        while (slot_query.next()) {
            TimeSlot time_slot;
            time_slot.index = slot_query.value(0).toInt();
            time_slot.label = slot_query.value(1).toString();
            time_slot.start_time = QTime::fromString(slot_query.value(2).toString(), QStringLiteral("HH:mm"));
            time_slot.end_time = QTime::fromString(slot_query.value(3).toString(), QStringLiteral("HH:mm"));
            snapshot.time_slots.append(time_slot);
        }

        // 课程（按插入顺序，保证界面中课程排列稳定）
        QSqlQuery course_query(m_database);
        course_query.prepare(QStringLiteral("SELECT id, semester_id, name, code, teacher, location, color, credits, notes "
                                            "FROM courses WHERE semester_id = :id ORDER BY rowid ASC"));
        course_query.bindValue(QStringLiteral(":id"), semester_id);
        if (!course_query.exec()) {
            return fail(error_message, QStringLiteral("读取课程失败：%1").arg(course_query.lastError().text()));
        }
        while (course_query.next()) {
            Course course;
            course.id = course_query.value(0).toString();
            course.semester_id = course_query.value(1).toString();
            course.name = course_query.value(2).toString();
            course.code = course_query.value(3).toString();
            course.teacher = course_query.value(4).toString();
            course.location = course_query.value(5).toString();
            course.color = course_query.value(6).toString();
            course.credits = course_query.value(7).toDouble();
            course.notes = course_query.value(8).toString();
            snapshot.courses.append(course);
        }

        // 上课时间段：一次查全部，按 course_id 分派，避免 N+1 查询
        QSqlQuery session_query(m_database);
        session_query.prepare(QStringLiteral(
            "SELECT s.id, s.course_id, s.day_of_week, s.start_slot, s.slot_count, s.week_bits, s.location, s.teacher "
            "FROM course_sessions s JOIN courses c ON c.id = s.course_id "
            "WHERE c.semester_id = :id ORDER BY s.rowid ASC"));
        session_query.bindValue(QStringLiteral(":id"), semester_id);
        if (!session_query.exec()) {
            return fail(error_message, QStringLiteral("读取上课时间失败：%1").arg(session_query.lastError().text()));
        }
        while (session_query.next()) {
            const QString course_id = session_query.value(1).toString();
            CourseSession session;
            session.id = session_query.value(0).toString();
            session.day_of_week = session_query.value(2).toInt();
            session.start_slot = session_query.value(3).toInt();
            session.slot_count = session_query.value(4).toInt();
            session.weeks = WeekMask::from_bits(session_query.value(5).toString().toULongLong());
            session.location = session_query.value(6).toString();
            session.teacher = session_query.value(7).toString();

            const int index = snapshot.index_of_course(course_id);
            if (index >= 0) {
                snapshot.courses[index].sessions.append(session);
            }
        }

        *out_snapshot = snapshot;
        if (error_message) {
            error_message->clear();
        }
        return true;
    }

    bool SqliteScheduleRepository::write_snapshot_content(const ScheduleSnapshot& snapshot, QString* error_message) {
        // 作息表：整体替换
        if (!exec(QStringLiteral("DELETE FROM time_slots WHERE semester_id = %1").arg(sql_quote(snapshot.semester.id)), error_message)) {
            return false;
        }
        for (const TimeSlot& time_slot : snapshot.time_slots) {
            QSqlQuery query(m_database);
            query.prepare(QStringLiteral("INSERT INTO time_slots (semester_id, slot_index, label, start_time, end_time) "
                                         "VALUES (:semester_id, :slot_index, :label, :start_time, :end_time)"));
            query.bindValue(QStringLiteral(":semester_id"), non_null(snapshot.semester.id));
            query.bindValue(QStringLiteral(":slot_index"), time_slot.index);
            query.bindValue(QStringLiteral(":label"), non_null(time_slot.label));
            query.bindValue(QStringLiteral(":start_time"),
                non_null(time_slot.start_time.isValid() ? time_slot.start_time.toString(QStringLiteral("HH:mm")) : QString()));
            query.bindValue(QStringLiteral(":end_time"),
                non_null(time_slot.end_time.isValid() ? time_slot.end_time.toString(QStringLiteral("HH:mm")) : QString()));
            if (!query.exec()) {
                return fail(error_message, QStringLiteral("写入作息表失败：%1").arg(query.lastError().text()));
            }
        }

        // 课程：整体替换（course_sessions 通过外键级联删除）
        if (!exec(QStringLiteral("DELETE FROM courses WHERE semester_id = %1").arg(sql_quote(snapshot.semester.id)), error_message)) {
            return false;
        }
        for (const Course& course : snapshot.courses) {
            QSqlQuery query(m_database);
            query.prepare(QStringLiteral(
                "INSERT INTO courses (id, semester_id, name, code, teacher, location, color, credits, notes) "
                "VALUES (:id, :semester_id, :name, :code, :teacher, :location, :color, :credits, :notes)"));
            query.bindValue(QStringLiteral(":id"), non_null(course.id));
            query.bindValue(QStringLiteral(":semester_id"), non_null(snapshot.semester.id));
            query.bindValue(QStringLiteral(":name"), non_null(course.name));
            query.bindValue(QStringLiteral(":code"), non_null(course.code));
            query.bindValue(QStringLiteral(":teacher"), non_null(course.teacher));
            query.bindValue(QStringLiteral(":location"), non_null(course.location));
            query.bindValue(QStringLiteral(":color"), non_null(course.color));
            query.bindValue(QStringLiteral(":credits"), course.credits);
            query.bindValue(QStringLiteral(":notes"), non_null(course.notes));
            if (!query.exec()) {
                return fail(error_message, QStringLiteral("写入课程失败：%1").arg(query.lastError().text()));
            }

            for (const CourseSession& session : course.sessions) {
                QSqlQuery session_query(m_database);
                session_query.prepare(QStringLiteral(
                    "INSERT INTO course_sessions (id, course_id, day_of_week, start_slot, slot_count, week_bits, week_expression, location, teacher) "
                    "VALUES (:id, :course_id, :day_of_week, :start_slot, :slot_count, :week_bits, :week_expression, :location, :teacher)"));
                session_query.bindValue(QStringLiteral(":id"), non_null(session.id));
                session_query.bindValue(QStringLiteral(":course_id"), non_null(course.id));
                session_query.bindValue(QStringLiteral(":day_of_week"), session.day_of_week);
                session_query.bindValue(QStringLiteral(":start_slot"), session.start_slot);
                session_query.bindValue(QStringLiteral(":slot_count"), session.slot_count);
                session_query.bindValue(QStringLiteral(":week_bits"), non_null(QString::number(session.weeks.bits())));
                session_query.bindValue(QStringLiteral(":week_expression"), non_null(session.weeks.to_expression()));
                session_query.bindValue(QStringLiteral(":location"), non_null(session.location));
                session_query.bindValue(QStringLiteral(":teacher"), non_null(session.teacher));
                if (!session_query.exec()) {
                    return fail(error_message, QStringLiteral("写入上课时间失败：%1").arg(session_query.lastError().text()));
                }
            }
        }

        return true;
    }

    bool SqliteScheduleRepository::save_snapshot(const ScheduleSnapshot& snapshot, QString* error_message) {
        if (!m_open) {
            return fail(error_message, QStringLiteral("数据库尚未打开"));
        }
        if (!snapshot.semester.is_valid(error_message)) {
            return false;
        }

        if (!begin_transaction(error_message)) {
            return false;
        }

        if (!save_semester(snapshot.semester, error_message)) {
            rollback_transaction();
            return false;
        }
        if (!write_snapshot_content(snapshot, error_message)) {
            rollback_transaction();
            return false;
        }

        if (!commit_transaction(error_message)) {
            return false;
        }

        if (error_message) {
            error_message->clear();
        }
        return true;
    }

    // -------------------------------------------------------------------- 设置

    QString SqliteScheduleRepository::setting(const QString& key, const QString& default_value) const {
        if (!m_open || key.isEmpty()) {
            return default_value;
        }
        QSqlQuery query(m_database);
        query.prepare(QStringLiteral("SELECT setting_value FROM settings WHERE setting_key = :key"));
        query.bindValue(QStringLiteral(":key"), key);
        if (!query.exec() || !query.next()) {
            return default_value;
        }
        return query.value(0).toString();
    }

    bool SqliteScheduleRepository::set_setting(const QString& key, const QString& value, QString* error_message) {
        if (!m_open) {
            return fail(error_message, QStringLiteral("数据库尚未打开"));
        }
        if (key.isEmpty()) {
            return fail(error_message, QStringLiteral("设置项键名不能为空"));
        }

        QSqlQuery query(m_database);
        query.prepare(QStringLiteral("INSERT INTO settings (setting_key, setting_value) VALUES (:key, :value) "
                                     "ON CONFLICT(setting_key) DO UPDATE SET setting_value = excluded.setting_value"));
        query.bindValue(QStringLiteral(":key"), non_null(key));
        query.bindValue(QStringLiteral(":value"), non_null(value));
        if (!query.exec()) {
            return fail(error_message, QStringLiteral("写入设置失败：%1").arg(query.lastError().text()));
        }

        if (error_message) {
            error_message->clear();
        }
        return true;
    }

    QMap<QString, QString> SqliteScheduleRepository::all_settings(QString* error_message) const {
        QMap<QString, QString> settings;
        if (!m_open) {
            fail(error_message, QStringLiteral("数据库尚未打开"));
            return settings;
        }

        QSqlQuery query(m_database);
        if (!query.exec(QStringLiteral("SELECT setting_key, setting_value FROM settings ORDER BY setting_key ASC"))) {
            fail(error_message, QStringLiteral("读取设置失败：%1").arg(query.lastError().text()));
            return settings;
        }
        while (query.next()) {
            settings.insert(query.value(0).toString(), query.value(1).toString());
        }

        if (error_message) {
            error_message->clear();
        }
        return settings;
    }

    // ---------------------------------------------------------------- 导入来源

    QList<ImportSource> SqliteScheduleRepository::load_import_sources(QString* error_message) const {
        QList<ImportSource> sources;
        if (!m_open) {
            fail(error_message, QStringLiteral("数据库尚未打开"));
            return sources;
        }

        QSqlQuery query(m_database);
        if (!query.exec(QStringLiteral("SELECT id, file_path, format, imported_at, course_count, note "
                                       "FROM import_sources ORDER BY imported_at DESC"))) {
            fail(error_message, QStringLiteral("读取导入记录失败：%1").arg(query.lastError().text()));
            return sources;
        }
        while (query.next()) {
            ImportSource source;
            source.id = query.value(0).toString();
            source.file_path = query.value(1).toString();
            source.format = query.value(2).toString();
            source.imported_at = QDateTime::fromString(query.value(3).toString(), Qt::ISODate);
            source.course_count = query.value(4).toInt();
            source.note = query.value(5).toString();
            sources.append(source);
        }

        if (error_message) {
            error_message->clear();
        }
        return sources;
    }

    bool SqliteScheduleRepository::add_import_source(const ImportSource& source, QString* error_message) {
        if (!m_open) {
            return fail(error_message, QStringLiteral("数据库尚未打开"));
        }

        ImportSource stored = source;
        if (stored.id.isEmpty()) {
            stored.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        }
        if (!stored.imported_at.isValid()) {
            stored.imported_at = QDateTime::currentDateTime();
        }

        QSqlQuery query(m_database);
        query.prepare(QStringLiteral(
            "INSERT INTO import_sources (id, file_path, format, imported_at, course_count, note) "
            "VALUES (:id, :file_path, :format, :imported_at, :course_count, :note)"));
        query.bindValue(QStringLiteral(":id"), non_null(stored.id));
        query.bindValue(QStringLiteral(":file_path"), non_null(stored.file_path));
        query.bindValue(QStringLiteral(":format"), non_null(stored.format));
        query.bindValue(QStringLiteral(":imported_at"), non_null(stored.imported_at.toString(Qt::ISODate)));
        query.bindValue(QStringLiteral(":course_count"), stored.course_count);
        query.bindValue(QStringLiteral(":note"), non_null(stored.note));
        if (!query.exec()) {
            return fail(error_message, QStringLiteral("写入导入记录失败：%1").arg(query.lastError().text()));
        }

        if (error_message) {
            error_message->clear();
        }
        return true;
    }

    // -------------------------------------------------------------------- 维护

    bool SqliteScheduleRepository::is_valid_database_file(const QString& file_path, QString* error_message) {
        if (!QFile::exists(file_path)) {
            return fail(error_message, QStringLiteral("文件不存在：%1").arg(file_path));
        }

        const QString connection_name = QStringLiteral("schedule_probe_%1")
                                            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
        bool valid = false;
        {
            QSqlDatabase probe = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection_name);
            probe.setDatabaseName(file_path);
            if (!probe.open()) {
                fail(error_message, QStringLiteral("无法打开数据库文件：%1").arg(probe.lastError().text()));
            }
            else {
                // 只读探测：必须能查到 semesters 表，且版本号不高于本实现支持的上限
                QSqlQuery query(probe);
                const bool has_semesters = query.exec(QStringLiteral("SELECT name FROM sqlite_master "
                                                                     "WHERE type = 'table' AND name = 'semesters'")) &&
                                           query.next();
                int version = -1;
                if (query.exec(QStringLiteral("PRAGMA user_version")) && query.next()) {
                    version = query.value(0).toInt();
                }
                if (!has_semesters) {
                    fail(error_message, QStringLiteral("文件中缺少 semesters 表，不是本应用的课表数据库"));
                }
                else if (version < 0 || version > latest_schema_version()) {
                    fail(error_message, QStringLiteral("数据库版本 %1 不受支持（当前支持到 %2）").arg(version).arg(latest_schema_version()));
                }
                else {
                    valid = true;
                }
            }
            probe.close();
        }
        QSqlDatabase::removeDatabase(connection_name);

        if (valid && error_message) {
            error_message->clear();
        }
        return valid;
    }

    bool SqliteScheduleRepository::backup_to(const QString& target_path, QString* error_message) const {
        if (!m_open) {
            return fail(error_message, QStringLiteral("数据库尚未打开"));
        }
        if (target_path.isEmpty()) {
            return fail(error_message, QStringLiteral("备份路径为空"));
        }
        if (target_path == m_database_path) {
            return fail(error_message, QStringLiteral("备份路径不能与数据库文件相同"));
        }

        const QDir parent = QFileInfo(target_path).absoluteDir();
        if (!parent.exists() && !parent.mkpath(QStringLiteral("."))) {
            return fail(error_message, QStringLiteral("无法创建备份目录：%1").arg(parent.absolutePath()));
        }
        // 覆盖已有备份：VACUUM INTO 要求目标文件不存在
        if (QFile::exists(target_path) && !QFile::remove(target_path)) {
            return fail(error_message, QStringLiteral("无法覆盖已存在的备份文件：%1").arg(target_path));
        }

        // `VACUUM INTO` 由 SQLite 保证快照一致性，比直接复制文件更安全（无需先关闭连接）。
        if (!exec(QStringLiteral("VACUUM INTO %1").arg(sql_quote(target_path)), error_message)) {
            return false;
        }

        if (error_message) {
            error_message->clear();
        }
        return true;
    }

    bool SqliteScheduleRepository::restore_from(const QString& source_path, QString* error_message) {
        if (m_database_path == QStringLiteral(":memory:")) {
            return fail(error_message, QStringLiteral("内存数据库不支持从文件恢复，请先配置持久化路径"));
        }

        QString validation_error;
        if (!is_valid_database_file(source_path, &validation_error)) {
            return fail(error_message, QStringLiteral("备份文件不可用：%1").arg(validation_error));
        }

        // 覆盖前先自动备份当前数据，恢复失败时用户仍有退路
        if (QFile::exists(m_database_path)) {
            const QString safety_copy = m_database_path + QStringLiteral(".bak");
            QFile::remove(safety_copy);
            if (!QFile::copy(m_database_path, safety_copy)) {
                return fail(error_message, QStringLiteral("恢复前自动备份失败：%1").arg(safety_copy));
            }
        }

        close();

        // WAL 模式下还存在 -wal / -shm 附属文件，必须一并清理，否则会与新文件不匹配
        QFile::remove(m_database_path + QStringLiteral("-wal"));
        QFile::remove(m_database_path + QStringLiteral("-shm"));
        QFile::remove(m_database_path);

        if (!QFile::copy(source_path, m_database_path)) {
            return fail(error_message, QStringLiteral("复制备份文件失败：%1 → %2").arg(source_path, m_database_path));
        }

        return open(error_message);
    }

} // namespace Schedule
