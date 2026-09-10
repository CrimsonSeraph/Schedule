#pragma once

#include "core/model/Course.h"

#include <QAbstractListModel>
#include <QHash>
#include <QList>
#include <QVariantMap>

namespace Schedule {

    /**
     * @brief 课程列表模型：把 `QList<Course>` 暴露给 QML 的 `ListView` / `Repeater`。
     *
     * 数据由 `ScheduleBridge` 在 `ScheduleService::courses_changed()` 后整体刷新，
     * 因此这里只需要实现“整体替换 + 按行读取”，不必处理增量插入。
     *
     * 角色（role）命名面向 QML 的 `model.xxx` 访问：
     *
     * | 角色 | 类型 | 说明 |
     * | ---- | ---- | ---- |
     * | `courseId` | string | 课程 id |
     * | `name` | string | 课程名称 |
     * | `code` | string | 课程代码 |
     * | `teacher` | string | 默认教师 |
     * | `location` | string | 默认地点 |
     * | `color` | string | 课卡颜色 `#RRGGBB` |
     * | `credits` | real | 学分 |
     * | `notes` | string | 备注 |
     * | `sessionCount` | int | 上课时间段数量 |
     * | `weekExpression` | string | 全部时间段周次的并集（规范化表达式） |
     * | `weekDisplay` | string | 周次的人类可读文本 |
     * | `daySummary` | string | 形如“周一 第 1-2 节”的摘要，多段以分号连接 |
     */
    class CourseListModel : public QAbstractListModel {
        Q_OBJECT

    public:
        /** QML 可访问的角色。 */
        enum Role {
            CourseIdRole = Qt::UserRole + 1, ///< 课程 id
            NameRole,                        ///< 课程名称
            CodeRole,                        ///< 课程代码
            TeacherRole,                     ///< 默认教师
            LocationRole,                    ///< 默认地点
            ColorRole,                       ///< 课卡颜色
            CreditsRole,                     ///< 学分
            NotesRole,                       ///< 备注
            SessionCountRole,                ///< 时间段数量
            WeekExpressionRole,              ///< 周次并集表达式
            WeekDisplayRole,                 ///< 周次可读文本
            DaySummaryRole,                  ///< 时间摘要
        };

        /** @param parent 父对象 */
        explicit CourseListModel(QObject* parent = nullptr);

        ~CourseListModel() override;

        // ------------------------------------------------------ QAbstractListModel

        int rowCount(const QModelIndex& parent = QModelIndex()) const override;

        QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

        QHash<int, QByteArray> roleNames() const override;

        // ---------------------------------------------------------------- 数据操作

        /**
         * @brief 整体替换课程列表；会自动发出 reset 信号。
         * @param courses 课程列表（按显示顺序）
         */
        void set_courses(const QList<Course>& courses);

        /** @return 当前课程列表的副本。 */
        QList<Course> courses() const;

        /** @return 指定 id 的行号；未找到返回 -1。 */
        int index_of_course(const QString& course_id) const;

        /**
         * @brief 按行读取课程（供 QML 编辑器回填表单）。
         * @return 课程字段的 map；行号越界返回空 map
         */
        Q_INVOKABLE QVariantMap get(int row) const;

        /** @return 按 id 读取课程 map；未找到返回空 map。 */
        Q_INVOKABLE QVariantMap find(const QString& course_id) const;

        /** @return 课程行号；供 QML 判断“编辑”还是“新增”，未找到返回 -1。 */
        Q_INVOKABLE int index_of(const QString& course_id) const;

    private:
        /** @return 由课程生成的角色值表。 */
        static QVariantMap to_map(const Course& course);

        /** 当前课程列表。 */
        QList<Course> m_courses;
    };

} // namespace Schedule
