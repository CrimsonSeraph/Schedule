#include "Logging.h"

#include <QDate>
#include <QDateTime>
#include <QRegularExpression>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QTextStream>
#include <QtGlobal>

namespace Schedule::Logging {

    namespace {

        constexpr qint64 kMaxBytes = 5LL * 1024 * 1024;

        QMutex g_mutex;
        QFile g_file;
        QString g_dir;
        QString g_date;
        qint64 g_bytes = 0;
        Level g_level = Level::Debug;
        bool g_installed = false;
        bool g_open_failure_reported = false;
        QtMessageHandler g_previous = nullptr;

        /** @return 单字符级别标记。 */
        const char* level_tag(Level level) {
            switch (level) {
            case Level::Debug:
                return "D";
            case Level::Info:
                return "I";
            case Level::Warning:
                return "W";
            case Level::Critical:
                return "C";
            }
            return "?";
        }

        /** @brief 把 QtMsgType 映射到内部级别。 */
        Level level_of(QtMsgType type) {
            switch (type) {
            case QtDebugMsg:
                return Level::Debug;
            case QtInfoMsg:
                return Level::Info;
            case QtWarningMsg:
                return Level::Warning;
            case QtCriticalMsg:
            case QtFatalMsg:
                return Level::Critical;
            }
            return Level::Debug;
        }

        /**
         * @brief 把日志文件打不开的原因交给上一级处理器，同一次连续失败只输出一条。
         *
         * 不走 qWarning：本函数由 write_line 调用，而 write_line 自己就是当前安装的处理器，
         * 再发 qWarning 会原地递归。直接转发给 g_previous（Qt 默认处理器 / WebEngine 之后
         * 的链条），消息会出现在 stderr、OutputDebugString 或调试器输出里。
         * 日志文件本身就是坏的那一端，这条提示只能走文件之外的通路，因此不受“日志格式”约束。
         */
        void report_open_failure_locked(const QString& file_path) {
            if (g_open_failure_reported || !g_previous) {
                return;
            }
            g_open_failure_reported = true;
            const QMessageLogContext context;
            g_previous(QtWarningMsg, context,
                QStringLiteral("[log] 无法写入日志文件 %1：%2（写不进去时会一直重试，本条提示只输出一次）")
                    .arg(file_path, g_file.errorString()));
        }

        /** @brief 确保当前日期对应的日志文件已打开（调用方需持有 g_mutex）。 */
        void ensure_file_locked() {
            const QString date = QDate::currentDate().toString(QStringLiteral("yyyyMMdd"));
            if (g_file.isOpen() && g_date == date) {
                return;
            }
            if (g_file.isOpen()) {
                g_file.close();
            }
            if (g_dir.isEmpty()) {
                return;
            }
            QDir().mkpath(g_dir);
            g_file.setFileName(QDir(g_dir).filePath(QStringLiteral("schedule-%1.log").arg(date)));
            if (g_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
                g_date = date;
                g_bytes = g_file.size();
                g_open_failure_reported = false;
            }
            else {
                report_open_failure_locked(g_file.fileName());
            }
        }

        /** @brief 写入一行日志并转发给上一级处理器（Android 上仍会进 logcat）。 */
        void write_line(QtMsgType type, const QMessageLogContext& context, const QString& message) {
            const Level message_level = level_of(type);
            if (message_level >= g_level) {
                const QString category = context.category ? QString::fromUtf8(context.category) : QStringLiteral("default");
                const QString location = (message_level == Level::Debug && context.file)
                    ? QStringLiteral(" (%1:%2)").arg(QFileInfo(QString::fromUtf8(context.file)).fileName()).arg(context.line)
                    : QString();
                const QString line = QStringLiteral("[%1] [%2] [%3] %4%5\n")
                    .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")))
                    .arg(QString::fromLatin1(level_tag(message_level)))
                    .arg(category)
                    .arg(message)
                    .arg(location);

                QMutexLocker lock(&g_mutex);
                ensure_file_locked();
                if (g_file.isOpen()) {
                    const QByteArray utf8 = line.toUtf8();
                    if (g_bytes + utf8.size() > kMaxBytes) {
                        const QString current = g_file.fileName();
                        g_file.close();
                        QFile::remove(current + QStringLiteral(".1"));
                        QFile::rename(current, current + QStringLiteral(".1"));
                        g_date.clear();
                        ensure_file_locked();
                    }
                    if (g_file.write(utf8) == utf8.size()) {
                        g_bytes += utf8.size();
                        g_file.flush();
                    }
                }
            }

            if (g_previous) {
                g_previous(type, context, message);
            }
        }

    } // namespace

    void install(const QString& directory) {
        QMutexLocker lock(&g_mutex);
        g_dir = directory;
        QDir().mkpath(g_dir);
        if (!g_installed) {
            g_previous = qInstallMessageHandler(write_line);
            g_installed = true;
        }
    }

    void shutdown() {
        QMutexLocker lock(&g_mutex);
        if (g_file.isOpen()) {
            g_file.flush();
            g_file.close();
        }
        if (g_installed) {
            qInstallMessageHandler(g_previous);
            g_installed = false;
            g_previous = nullptr;
        }
    }

    void set_level(Level level) {
        QMutexLocker lock(&g_mutex);
        g_level = level;
    }

    Level level() {
        QMutexLocker lock(&g_mutex);
        return g_level;
    }

    QString log_directory() {
        QMutexLocker lock(&g_mutex);
        return g_dir;
    }

    int prune_old_logs(int keep_days) {
        const int days = qBound(1, keep_days, 365);
        const QDate cutoff = QDate::currentDate().addDays(-days);
        int removed = 0;
        {
            QMutexLocker lock(&g_mutex);
            if (g_dir.isEmpty()) {
                return 0;
            }
            const QDir dir(g_dir);
            const QFileInfoList files = dir.entryInfoList(
                QStringList{QStringLiteral("schedule-*.log"), QStringLiteral("schedule-*.log.1")},
                QDir::Files, QDir::Name);
            const QRegularExpression pattern(QStringLiteral("^schedule-(\\d{8})\\.log"));
            for (const QFileInfo& info : files) {
                const QRegularExpressionMatch match = pattern.match(info.fileName());
                if (!match.hasMatch()) {
                    continue;
                }
                const QDate date = QDate::fromString(match.captured(1), QStringLiteral("yyyyMMdd"));
                if (date.isValid() && date < cutoff && QFile::remove(info.absoluteFilePath())) {
                    ++removed;
                }
            }
        }
        if (removed > 0) {
            qInfo().noquote() << "[log] 已清理" << removed << "个超过" << days << "天的日志文件";
        }
        return removed;
    }

    QString export_logs(const QString& target_directory, QString* error_message) {
        QMutexLocker lock(&g_mutex);
        if (g_file.isOpen()) {
            g_file.flush();
        }
        QDir source(g_dir);
        const QStringList files = source.entryList(QStringList{QStringLiteral("schedule-*.log")}, QDir::Files, QDir::Name);
        if (files.isEmpty()) {
            if (error_message) {
                *error_message = QStringLiteral("还没有日志可导出");
            }
            return QString();
        }
        QDir target(target_directory);
        if (!target.exists() && !QDir().mkpath(target_directory)) {
            if (error_message) {
                *error_message = QStringLiteral("无法创建导出目录：%1").arg(target_directory);
            }
            return QString();
        }
        const QString output = target.filePath(QStringLiteral("schedule-logs-%1.log")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"))));
        QFile destination(output);
        if (!destination.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            if (error_message) {
                *error_message = QStringLiteral("无法写入导出文件：%1").arg(output);
            }
            return QString();
        }
        QTextStream stream(&destination);
        stream << "Schedule logs exported at " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
        stream << "Log directory: " << g_dir << "\n";
        for (const QString& name : files) {
            QFile input(source.filePath(name));
            if (!input.open(QIODevice::ReadOnly)) {
                continue;
            }
            stream << "\n===== " << name << " =====\n";
            stream << QString::fromUtf8(input.readAll());
        }
        stream.flush();
        destination.close();
        return output;
    }

} // namespace Schedule::Logging
