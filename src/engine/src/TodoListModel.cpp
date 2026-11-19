#include "engine/TodoListModel.h"

#include <QDateTime>

namespace Schedule {

    namespace {

        /** @return 截止时间的展示文本；没有截止时间返回空串。 */
        QString due_text(const QDateTime& due_at) {
            return due_at.isValid() ? due_at.toString(QStringLiteral("MM-dd HH:mm")) : QString();
        }

        /** @return 是否已过截止时间且未完成。 */
        bool is_overdue(const Todo& todo) {
            return !todo.done && todo.due_at.isValid() && todo.due_at < QDateTime::currentDateTime();
        }

    } // namespace

    TodoListModel::TodoListModel(QObject* parent)
        : QAbstractListModel(parent) {
    }

    int TodoListModel::rowCount(const QModelIndex& parent) const {
        return parent.isValid() ? 0 : m_todos.size();
    }

    QVariant TodoListModel::data(const QModelIndex& index, int role) const {
        if (!index.isValid() || index.row() < 0 || index.row() >= m_todos.size()) {
            return QVariant();
        }
        const Todo& todo = m_todos.at(index.row());
        switch (role) {
        case Qt::DisplayRole:
        case TitleRole:
            return todo.title;
        case TodoIdRole:
            return todo.id;
        case NotesRole:
            return todo.notes;
        case DueAtRole:
            return todo.due_at.isValid() ? todo.due_at.toString(Qt::ISODate) : QString();
        case DueTextRole:
            return due_text(todo.due_at);
        case OverdueRole:
            return is_overdue(todo);
        case DoneRole:
            return todo.done;
        default:
            return QVariant();
        }
    }

    QHash<int, QByteArray> TodoListModel::roleNames() const {
        return {
            { TodoIdRole, "todoId" },
            { TitleRole, "title" },
            { NotesRole, "notes" },
            { DueAtRole, "dueAt" },
            { DueTextRole, "dueText" },
            { OverdueRole, "overdue" },
            { DoneRole, "done" },
        };
    }

    void TodoListModel::set_todos(const QList<Todo>& todos) {
        beginResetModel();
        m_todos = todos;
        endResetModel();
    }

    QList<Todo> TodoListModel::todos() const {
        return m_todos;
    }

    QVariantMap TodoListModel::get(int row) const {
        if (row < 0 || row >= m_todos.size()) {
            return {};
        }
        const Todo& todo = m_todos.at(row);
        QVariantMap map;
        map.insert(QStringLiteral("todoId"), todo.id);
        map.insert(QStringLiteral("title"), todo.title);
        map.insert(QStringLiteral("notes"), todo.notes);
        map.insert(QStringLiteral("dueAt"), todo.due_at.isValid() ? todo.due_at.toString(Qt::ISODate) : QString());
        map.insert(QStringLiteral("dueText"), due_text(todo.due_at));
        map.insert(QStringLiteral("done"), todo.done);
        return map;
    }

    int TodoListModel::index_of(const QString& todo_id) const {
        for (int i = 0; i < m_todos.size(); ++i) {
            if (m_todos.at(i).id == todo_id) {
                return i;
            }
        }
        return -1;
    }

} // namespace Schedule
