#pragma once

#include <QDateTime>
#include <QString>

namespace Schedule {

    /**
     * @brief 一条待办 / 日程。
     *
     * 与课表数据不同，待办是全局的（不挂在某个学期下），因此不进入
     * ScheduleSnapshot，而由仓库的 todos 表单独持久化。
     */
    struct Todo {
        /** 唯一标识（UUID，无花括号）。 */
        QString id;

        /** 标题；必填。 */
        QString title;

        /** 备注，可为空。 */
        QString notes;

        /** 截止时间；无效表示没有截止时间。 */
        QDateTime due_at;

        /** 是否已完成。 */
        bool done = false;

        QDateTime created_at;
        QDateTime updated_at;

        bool is_valid(QString* error_message = nullptr) const;

        /** @return 由标题 / 备注 / 截止时间创建一条新待办。 */
        static Todo create(const QString& title, const QString& notes, const QDateTime& due_at);

        bool operator==(const Todo& other) const;
        bool operator!=(const Todo& other) const;
    };

} // namespace Schedule
