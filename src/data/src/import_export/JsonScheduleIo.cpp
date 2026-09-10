#include "data/import_export/JsonScheduleIo.h"

#include "data/ScheduleJson.h"

#include <QFile>

namespace Schedule {

    ScheduleFormat JsonScheduleIo::format() const {
        return ScheduleFormat::Json;
    }

    QString JsonScheduleIo::display_name() const {
        return format_display_name(ScheduleFormat::Json);
    }

    QStringList JsonScheduleIo::extensions() const {
        return QStringList{QStringLiteral(".json")};
    }

    bool JsonScheduleIo::can_import(const QString& file_path, QString* error_message) const {
        if (format_from_extension(file_path) != ScheduleFormat::Json) {
            if (error_message) {
                *error_message = QStringLiteral("不是 JSON 文件：%1").arg(file_path);
            }
            return false;
        }
        if (error_message) {
            error_message->clear();
        }
        return true;
    }

    bool JsonScheduleIo::parse(const QString& file_path, ScheduleSnapshot* out_snapshot, QString* error_message) const {
        return ScheduleJson::read_file(file_path, out_snapshot, error_message);
    }

    bool JsonScheduleIo::parse_data(const QByteArray& data,
        const QString& source_name,
        ScheduleSnapshot* out_snapshot,
        QString* error_message) const {
        if (!ScheduleJson::from_document(data, out_snapshot, error_message)) {
            if (error_message) {
                *error_message = QStringLiteral("%1：%2").arg(source_name, *error_message);
            }
            return false;
        }
        return true;
    }

    QString JsonScheduleIo::extension() const {
        return file_extension(ScheduleFormat::Json);
    }

    QByteArray JsonScheduleIo::serialize(const ScheduleSnapshot& snapshot, QString* error_message) const {
        Q_UNUSED(error_message);
        return ScheduleJson::to_document(snapshot, true);
    }

    bool JsonScheduleIo::write(const QString& file_path, const ScheduleSnapshot& snapshot, QString* error_message) const {
        return ScheduleJson::write_file(file_path, snapshot, error_message);
    }

} // namespace Schedule
