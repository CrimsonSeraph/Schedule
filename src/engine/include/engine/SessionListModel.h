#pragma once

#include "core/service/ScheduleService.h"

#include <QAbstractListModel>
#include <QHash>
#include <QList>
#include <QVariantMap>

namespace Schedule {

    /**
     * @brief 已排布课程模型：某周（可再按星期过滤）的全部上课时间段。
     *
     * 周视图 / 日视图直接把它当作 `Repeater` 的模型使用；切换周次只需调用
     * `set_week()`，模型会发出 reset 信号并让视图重绘。
     *
     * 角色（role）：
     *
     * | 角色 | 类型 | 说明 |
     * | ---- | ---- | ---- |
     * | `courseId` | string | 所属课程 id |
     * | `courseName` | string | 课程名称 |
     * | `sessionId` | string | 时间段 id |
     * | `dayOfWeek` | int | 1=周一 ... 7=周日 |
     * | `dayName` | string | 中文星期名 |
     * | `startSlot` / `endSlot` / `slotCount` | int | 节次信息 |
     * | `weeks` | string | 周次表达式 |
     * | `weeksDisplay` | string | 周次可读文本 |
     * | `teacher` / `location` | string | 生效的教师 / 地点（时间段覆盖优先） |
     * | `color` | string | 课卡颜色 |
     * | `startTime` / `endTime` | string | `HH:mm`，作息表未定义时为空串 |
     * | `date` | string | `yyyy-MM-dd` |
     * | `week` | int | 当前周次 |
     * | `rowSpan` | int | 占用的节次行数（等于 `slotCount`，供界面直接布局） |
     */
    class SessionListModel : public QAbstractListModel {
        Q_OBJECT

    public:
        /** QML 可访问的角色。 */
        enum Role {
            CourseIdRole = Qt::UserRole + 1, ///< 课程 id
            CourseNameRole,                  ///< 课程名称
            SessionIdRole,                   ///< 时间段 id
            DayOfWeekRole,                   ///< 星期（1..7）
            DayNameRole,                     ///< 中文星期名
            StartSlotRole,                   ///< 起始节次
            EndSlotRole,                     ///< 结束节次
            SlotCountRole,                   ///< 连续节次数
            WeeksRole,                       ///< 周次表达式
            WeeksDisplayRole,                ///< 周次可读文本
            TeacherRole,                     ///< 生效教师
            LocationRole,                    ///< 生效地点
            ColorRole,                       ///< 课卡颜色
            StartTimeRole,                   ///< 开始时间 `HH:mm`
            EndTimeRole,                     ///< 结束时间 `HH:mm`
            DateRole,                        ///< 日期 `yyyy-MM-dd`
            WeekRole,                        ///< 周次
            RowSpanRole,                     ///< 占用节次行数
        };

        /** @param parent 父对象 */
        explicit SessionListModel(QObject* parent = nullptr);

        ~SessionListModel() override;

        // ------------------------------------------------------ QAbstractListModel

        int rowCount(const QModelIndex& parent = QModelIndex()) const override;

        QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

        QHash<int, QByteArray> roleNames() const override;

        // ---------------------------------------------------------------- 过滤条件

        /** @return 当前显示的周次；0 表示未设置。 */
        Q_INVOKABLE int week() const;

        /**
         * @brief 切换显示的周次。
         * @param week 1 起的周次；<= 0 表示清空
         */
        Q_INVOKABLE void set_week(int week);

        /** @return 当前星期过滤值；0 表示不过滤（整周）。 */
        Q_INVOKABLE int day_filter() const;

        /** @brief 设置星期过滤（1..7）；传 0 表示显示整周。 */
        Q_INVOKABLE void set_day_filter(int day_of_week);

        /** @brief 注入数据源（不持有所有权）；注入后模型会自动重载。 */
        void set_service(ScheduleService* service);

        /** @brief 用最新的数据源重建模型（在课程 / 学期 / 作息表变化后调用）。 */
        void refresh();

        /** @return 当前行数（等价于 `rowCount()`）。 */
        Q_INVOKABLE int count() const;

        /** @return 指定行的数据 map；越界返回空 map。 */
        Q_INVOKABLE QVariantMap get(int row) const;

    private:
        /** 由 `ScheduleService` 重新拉取数据。 */
        void reload();

        /** 数据来源；不持有所有权。 */
        ScheduleService* m_service = nullptr;

        /** 当前周次；0 表示未设置。 */
        int m_week = 0;

        /** 星期过滤；0 表示整周。 */
        int m_day_filter = 0;

        /** 当前展示的排布数据。 */
        QList<ScheduleService::PlacedSession> m_sessions;
    };

} // namespace Schedule
