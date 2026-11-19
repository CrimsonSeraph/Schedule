#pragma once

#include "core/model/Todo.h"

#include <QAbstractListModel>
#include <QHash>
#include <QList>
#include <QVariantMap>

namespace Schedule {

    /**
     * @brief 待办列表模型：把 QList<Todo> 暴露给 QML 的 ListView。
     *
     * 角色：todoId / title / notes / dueAt（ISO）/ dueText（MM-dd HH:mm）/ overdue / done。
     * 数据由 ScheduleBridge 在加载或增删后整体替换。
     */
    class TodoListModel : public QAbstractListModel {
        Q_OBJECT

    public:
        enum Role {
            TodoIdRole = Qt::UserRole + 1,
            TitleRole,
            NotesRole,
            DueAtRole,
            DueTextRole,
            OverdueRole,
            DoneRole,
        };

        explicit TodoListModel(QObject* parent = nullptr);

        int rowCount(const QModelIndex& parent = QModelIndex()) const override;

        QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

        QHash<int, QByteArray> roleNames() const override;

        /** @brief 整体替换待办列表；会发出 reset 信号。 */
        void set_todos(const QList<Todo>& todos);

        /** @return 当前待办列表的副本。 */
        QList<Todo> todos() const;

        /** @brief 按行读取待办（供编辑器回填）。 */
        Q_INVOKABLE QVariantMap get(int row) const;

        /** @return 指定 id 的行号；未找到返回 -1。 */
        Q_INVOKABLE int index_of(const QString& todo_id) const;

    private:
        QList<Todo> m_todos;
    };

} // namespace Schedule
