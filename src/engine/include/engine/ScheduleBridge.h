#pragma once

#include "core/model/Conflict.h"
#include "core/service/ScheduleService.h"
#include "data/AppSettings.h"
#include "data/IScheduleRepository.h"
#include "engine/CourseListModel.h"
#include "engine/ImportExportBridge.h"
#include "engine/SessionListModel.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

namespace Schedule {

    /**
     * @brief 课表主桥接对象：QML 访问课表状态与操作的唯一入口。
     *
     * **职责**：
     *  - 持有并刷新两个列表模型（`CourseListModel` / `SessionListModel`）；
     *  - 把 `ScheduleService` 的状态以只读 `Q_PROPERTY` 暴露给 QML；
     *  - 提供课程增删改查、学期设置、节次设置、周次切换等槽函数；
     *  - 在数据变化后自动持久化到仓库。
     *
     * **不负责**：界面绘制、文件对话框、导入导出细节（后者见 `ImportExportBridge`）。
     *
     * **信号连接约定**：QML 不写 `onClicked` / `Connections`，所有交互由 `app` 层在
     * C++ 侧按 `objectName` 找到控件后显式连接（见 `src/app/main.cpp`）。
     */
    class ScheduleBridge : public QObject {
        Q_OBJECT

        /** 课程列表模型（QML 直接用作 `ListView` / `Repeater` 的 model）。 */
        Q_PROPERTY(QAbstractItemModel* courseModel READ course_model CONSTANT)

        /** 当前周的排布模型（可用 `selectedDay` 过滤为日视图）。 */
        Q_PROPERTY(QAbstractItemModel* sessionModel READ session_model CONSTANT)

        /** 整周排布模型（**不**受 `selectedDay` 影响，供周视图使用）。 */
        Q_PROPERTY(QAbstractItemModel* weekModel READ week_model CONSTANT)

        /** 导入导出桥接对象。 */
        Q_PROPERTY(ImportExportBridge* importExport READ import_export CONSTANT)

        /** 是否已设置有效学期。 */
        Q_PROPERTY(bool hasSemester READ has_semester NOTIFY semesterChanged)

        /** 学期名称。 */
        Q_PROPERTY(QString semesterName READ semester_name NOTIFY semesterChanged)

        /** 学期第 1 周起始日（`yyyy-MM-dd`）。 */
        Q_PROPERTY(QString semesterStartDate READ semester_start_date NOTIFY semesterChanged)

        /** 学期结束日（`yyyy-MM-dd`）。 */
        Q_PROPERTY(QString semesterEndDate READ semester_end_date NOTIFY semesterChanged)

        /** 学期总周数。 */
        Q_PROPERTY(int totalWeeks READ total_weeks NOTIFY semesterChanged)

        /** 今天所在周次；不在学期内为 0。 */
        Q_PROPERTY(int currentWeek READ current_week NOTIFY currentWeekChanged)

        /** 当前正在查看的周次。 */
        Q_PROPERTY(int selectedWeek READ selected_week NOTIFY selectedWeekChanged)

        /** 当前正在查看的星期（1..7）；0 表示整周（周视图）。 */
        Q_PROPERTY(int selectedDay READ selected_day NOTIFY selectedDayChanged)

        /** 当前查看周的日期范围文本，如 `2024-09-02 ~ 2024-09-08`。 */
        Q_PROPERTY(QString selectedWeekRange READ selected_week_range NOTIFY selectedWeekChanged)

        /** 今天日期的显示文本。 */
        Q_PROPERTY(QString todayText READ today_text NOTIFY currentWeekChanged)

        /** 课程数量。 */
        Q_PROPERTY(int courseCount READ course_count NOTIFY coursesChanged)

        /** 阻断性冲突数量（`is_blocking()` 为真的条目）。 */
        Q_PROPERTY(int conflictCount READ conflict_count NOTIFY conflictsChanged)

        /** 是否存在阻断性问题。 */
        Q_PROPERTY(bool hasBlockingConflicts READ has_blocking_conflicts NOTIFY conflictsChanged)

        /** 冲突摘要文本。 */
        Q_PROPERTY(QString conflictSummary READ conflict_summary NOTIFY conflictsChanged)

        /** 冲突明细（map 列表）。 */
        Q_PROPERTY(QVariantList conflicts READ conflicts NOTIFY conflictsChanged)

        /** 作息表（map 列表：`index` / `label` / `start` / `end` / `duration`）。 */
        Q_PROPERTY(QVariantList timeSlots READ time_slots NOTIFY timeSlotsChanged)

        /** 周次下拉选项（map 列表：`value` / `label`）。 */
        Q_PROPERTY(QVariantList weekOptions READ week_options NOTIFY semesterChanged)

        /** 星期下拉选项（map 列表：`value` / `label`）。 */
        Q_PROPERTY(QVariantList dayOptions READ day_options CONSTANT)

        /** 课表数据文件路径（展示于设置页）。 */
        Q_PROPERTY(QString databasePath READ database_path CONSTANT)

        /** 应用版本号。 */
        Q_PROPERTY(QString version READ version CONSTANT)

        /** 最近一次错误信息（面向用户的中文）。 */
        Q_PROPERTY(QString lastError READ last_error NOTIFY errorOccurred)

        /** 最近一次提示信息。 */
        Q_PROPERTY(QString lastInfo READ last_info NOTIFY infoMessage)

    public:
        /**
         * @param service    课表服务（不持有所有权）
         * @param repository 仓库（不持有所有权），可为空（表示仅内存模式）
         * @param settings   设置门面（不持有所有权），可为空
         * @param parent     父对象
         */
        ScheduleBridge(ScheduleService* service,
            IScheduleRepository* repository,
            AppSettings* settings,
            QObject* parent = nullptr);

        ~ScheduleBridge() override;

        /**
         * @brief 从仓库加载当前学期数据并初始化模型状态。
         *
         * 由 `app` 层在注入 QML **之前**调用，保证界面首次渲染即有数据。
         * @return 是否成功（仓库为空或没有学期时返回 false，但不视为错误）
         */
        bool initialize();

        // ------------------------------------------------------------ 属性读取

        QAbstractItemModel* course_model() const;
        QAbstractItemModel* session_model() const;
        QAbstractItemModel* week_model() const;
        ImportExportBridge* import_export() const;
        bool has_semester() const;
        QString semester_name() const;
        QString semester_start_date() const;
        QString semester_end_date() const;
        int total_weeks() const;
        int current_week() const;
        int selected_week() const;
        int selected_day() const;
        QString selected_week_range() const;
        QString today_text() const;
        int course_count() const;
        int conflict_count() const;
        bool has_blocking_conflicts() const;
        QString conflict_summary() const;
        QVariantList conflicts() const;
        QVariantList time_slots() const;
        QVariantList week_options() const;
        QVariantList day_options() const;
        QString database_path() const;
        QString version() const;
        QString last_error() const;
        QString last_info() const;

        /** @return 指定星期的中文名；越界返回空串。 */
        Q_INVOKABLE QString day_name(int day_of_week) const;

        /** @return 第 week 周星期 day_of_week 对应的日期文本（`MM-dd`）；越界返回空串。 */
        Q_INVOKABLE QString week_date_text(int week, int day_of_week) const;

        /** @return 指定小时 + 分钟的 `HH:mm` 文本。 */
        Q_INVOKABLE QString format_time(int hour, int minute) const;

    public slots:
        /** 重新从仓库加载（设置页“重新加载”按钮）。 */
        void reload_from_repository();

        /** 把当前内存状态整体写回仓库。 */
        void save_to_repository();

        /**
         * @brief 新建学期。
         * @param name        学期名称
         * @param start_date  第 1 周起始日（`yyyy-MM-dd`）
         * @param total_weeks 总周数（1..64）
         */
        void create_semester(const QString& name, const QString& start_date, int total_weeks);

        /**
         * @brief 修改当前学期元数据（名称 / 起始日 / 周数）。
         */
        void update_semester(const QString& name, const QString& start_date, int total_weeks);

        /** 把当前学期的作息表重置为内置默认值（11 节）。 */
        void reset_time_slots_to_default();

        /**
         * @brief 保存单个节次定义。
         * @param index 节次序号
         * @param label 显示名
         * @param start 开始时间 `HH:mm`
         * @param end   结束时间 `HH:mm`
         */
        void save_time_slot(int index, const QString& label, const QString& start, const QString& end);

        /** 切换到指定周次。 */
        void select_week(int week);

        /** 切换到当前周次。 */
        void go_to_current_week();

        /** 上一周。 */
        void previous_week();

        /** 下一周。 */
        void next_week();

        /**
         * @brief 切换查看的星期。
         * @param day_of_week 1..7；传 0 表示切回整周视图
         */
        void select_day(int day_of_week);

        /**
         * @brief 保存课程（新增或更新）。
         *
         * @param data 课程字段 map，键包括：
         *             `courseId`（空表示新增）、`name`、`code`、`teacher`、`location`、
         *             `color`、`credits`、`notes`，以及 `sessions`（时间段 map 列表，
         *             每项含 `sessionId`、`dayOfWeek`、`startSlot`、`slotCount`、`weeks`、
         *             `location`、`teacher`）。
         * @return 是否保存成功；失败时 `errorOccurred` 已携带中文原因
         */
        bool save_course(const QVariantMap& data);

        /** 删除课程。 */
        void remove_course(const QString& course_id);

        /** 重新计算冲突列表并发出信号。 */
        void refresh_conflicts();

    signals:
        /** 学期元数据变化后发出。 */
        void semesterChanged();

        /** 今天所在周次变化后发出。 */
        void currentWeekChanged();

        /** 正在查看的周次变化后发出。 */
        void selectedWeekChanged();

        /** 正在查看的星期变化后发出。 */
        void selectedDayChanged();

        /** 课程数量或内容变化后发出。 */
        void coursesChanged();

        /** 作息表变化后发出。 */
        void timeSlotsChanged();

        /** 冲突列表变化后发出。 */
        void conflictsChanged();

        /**
         * @brief 发生错误。
         * @param message 面向用户的中文错误信息
         */
        void errorOccurred(const QString& message);

        /**
         * @brief 一般提示（非错误）。
         * @param message 面向用户的中文提示
         */
        void infoMessage(const QString& message);

    private:
        /** @brief 建立内部信号连接（服务 → 模型刷新）。 */
        void connect_service();

        /** @brief 刷新全部模型与派生属性，并发出相应信号。 */
        void refresh_all();

        /** @brief 刷新课程模型与当前周模型。 */
        void refresh_models();

        /** @brief 重新计算冲突并发出信号。 */
        void recompute_conflicts();

        /** @brief 设置错误信息并发出信号。 */
        void report_error(const QString& message);

        /** @brief 设置提示信息并发出信号。 */
        void report_info(const QString& message);

        /** @return 学期快照（只读）。 */
        ScheduleSnapshot snapshot() const;

        /** 课表服务；不持有所有权。 */
        ScheduleService* m_service = nullptr;

        /** 仓库；不持有所有权，可为空。 */
        IScheduleRepository* m_repository = nullptr;

        /** 设置门面；不持有所有权，可为空。 */
        AppSettings* m_settings = nullptr;

        /** 课程列表模型（子对象）。 */
        CourseListModel* m_course_model = nullptr;

        /** 当前周排布模型（子对象）。 */
        SessionListModel* m_session_model = nullptr;

        /** 整周排布模型（子对象）。 */
        SessionListModel* m_week_model = nullptr;

        /** 导入导出桥接（子对象）。 */
        ImportExportBridge* m_import_export = nullptr;

        /** 当前查看的周次。 */
        int m_selected_week = 0;

        /** 当前查看的星期；0 表示整周。 */
        int m_selected_day = 0;

        /** 缓存的冲突列表。 */
        QList<Conflict> m_conflicts;

        /** 最近一次错误。 */
        QString m_last_error;

        /** 最近一次提示。 */
        QString m_last_info;
    };

} // namespace Schedule
