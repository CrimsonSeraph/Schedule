#include "core/model/Todo.h"

#include <QUuid>

namespace Schedule {

    bool Todo::is_valid(QString* error_message) const {
        if (title.trimmed().isEmpty()) {
            if (error_message) {
                *error_message = QStringLiteral("待办标题不能为空");
            }
            return false;
        }
        if (!due_at.isValid() && !due_at.isNull()) {
            if (error_message) {
                *error_message = QStringLiteral("待办截止时间无效");
            }
            return false;
        }
        if (error_message) {
            error_message->clear();
        }
        return true;
    }

    Todo Todo::create(const QString& title, const QString& notes, const QDateTime& due_at) {
        Todo todo;
        todo.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        todo.title = title.trimmed();
        todo.notes = notes;
        todo.due_at = due_at;
        todo.done = false;
        todo.created_at = QDateTime::currentDateTime();
        todo.updated_at = todo.created_at;
        return todo;
    }

    bool Todo::operator==(const Todo& other) const {
        return id == other.id && title == other.title && notes == other.notes
            && due_at == other.due_at && done == other.done
            && created_at == other.created_at && updated_at == other.updated_at;
    }

    bool Todo::operator!=(const Todo& other) const {
        return !(*this == other);
    }

} // namespace Schedule
