#pragma once

#include "core/model/Conflict.h"
#include "core/model/Course.h"
#include "core/model/ScheduleSnapshot.h"
#include "core/model/Semester.h"
#include "core/model/TimeSlot.h"

#include <QDate>
#include <QList>
#include <QObject>
#include <QString>

namespace Schedule {

    /**
     * @brief 课表核心服务：内存中的课表状态 + 业务操作入口。
     *
     * 职责边界：
     *  - 持有**当前学期**的完整状态（学期元数据、作息表、课程列表）；
     *  - 提供课程增删改查、当前周计算、冲突检测；
     *  - 通过信号通知上层状态变化（上层据此刷新 QML 模型与界面）。
     *
     * 明确**不负责**：读写数据库、解析文件、界面展示。持久化由 `data` 层的
     * `IScheduleRepository` 完成，导入导出由 `ImportManager` / `ExportManager` 完成。
     *
     * @note 本类只管理“当前学期”的工作集。多学期的切换由上层先调用
     *       `load_snapshot()` 换入目标学期数据，再写回仓库。
     */
    class ScheduleService : public QObject {
        Q_OBJECT

    public:
        /**
         * @brief 已排布的一次课：把课程信息与具体时间段合并后的视图数据。
         *
         * 周视图 / 日视图渲染需要“某周某天有哪些课”，逐条携带课程名与颜色可避免
         * 上层反复做 id → 课程 的二次查找。
         */
        struct PlacedSession {
            /** 所属课程 id。 */
            QString course_id;

            /** 课程名称。 */
            QString course_name;

            /** 课卡颜色（`#RRGGBB`，可能由课程名派生）。 */
            QString color;

            /** 生效的授课教师（时间段覆盖优先）。 */
            QString teacher;

            /** 生效的上课地点（时间段覆盖优先）。 */
            QString location;

            /** 原始时间段数据。 */
            CourseSession session;

            /** 该排布对应的周次。 */
            int week = 0;

            /** 该排布对应的具体日期。 */
            QDate date;

            /** 起始节次定义（可能为默认构造，表示作息表未定义该节）。 */
            TimeSlot start_slot_definition;

            /** 结束节次定义。 */
            TimeSlot end_slot_definition;

            /** @return 起始时间；无作息定义时返回无效 QTime。 */
            QTime start_time() const;

            /** @return 结束时间；无作息定义时返回无效 QTime。 */
            QTime end_time() const;
        };

        /** @param parent 父对象，便于 Qt 父子生命周期管理。 */
        explicit ScheduleService(QObject* parent = nullptr);

        ~ScheduleService() override;

        // ---------------------------------------------------------------- 学期

        /** @return 当前学期（可能为默认构造的无效值）。 */
        Semester semester() const;

        /** @return 是否已设置有效学期。 */
        bool has_semester() const;

        /**
         * @brief 换入一个学期。
         *
         * 学期 id 变化时会**清空**已加载的课程与作息表，避免旧学期数据串到新学期；
         * 若仅修改名称 / 周数等元数据（id 不变），课程与作息表保持不变。
         *
         * @param semester 目标学期
         * @return 是否成功（学期非法时返回 false 并发出 error_occurred）
         */
        bool set_semester(const Semester& semester);

        // -------------------------------------------------------------- 作息表

        /** @return 当前作息表。 */
        QList<TimeSlot> time_slots() const;

        /** @brief 覆盖作息表（按 index 升序规整）；发出 time_slots_changed()。 */
        void set_time_slots(const QList<TimeSlot>& time_slots);

        /** @return 作息表最大节次序号；空表返回 0。 */
        int max_slot_index() const;

        /** @return 指定节次序号的定义；未找到返回默认构造的 TimeSlot。 */
        TimeSlot time_slot(int index) const;

        // ---------------------------------------------------------------- 课程

        /** @return 当前学期全部课程（保持插入顺序）。 */
        QList<Course> courses() const;

        /** @return 课程数量。 */
        int course_count() const;

        /** @return 按 id 查找课程；未找到返回 false。 */
        bool find_course(const QString& course_id, Course* out_course) const;

        /** @return 按 id 查找课程下标；未找到返回 -1。 */
        int index_of_course(const QString& course_id) const;

        /**
         * @brief 新增课程。
         *
         * 自动补齐：空 id → 生成 UUID；空 semester_id → 采用当前学期 id；
         * 时间段缺失 id → 生成 UUID；空颜色 → 由课程名派生。
         *
         * @param course        课程（按值传入，函数内部按需修改）
         * @param error_message 可选输出，失败时写入中文原因
         * @return 是否添加成功
         *
         * @note **允许**添加与已有课程时间冲突的课程（学生可能需要先记录再调整），
         *       冲突通过 `detect_conflicts()` 单独查询。
         */
        bool add_course(Course course, QString* error_message = nullptr);

        /**
         * @brief 更新课程（按 id 匹配）。
         * @return 是否更新成功（id 不存在或数据非法时 false）
         */
        bool update_course(const Course& course, QString* error_message = nullptr);

        /** @brief 删除课程；发出 courses_changed()。 @return 是否存在并删除。 */
        bool remove_course(const QString& course_id);

        /** @brief 整体替换课程列表（导入 / 加载时使用）；发出 courses_changed()。 */
        void set_courses(const QList<Course>& courses);

        // ---------------------------------------------------------------- 快照

        /** @return 当前状态的完整快照（可直接交给仓库或导出器）。 */
        ScheduleSnapshot snapshot() const;

        /**
         * @brief 用快照整体替换当前状态。
         *
         * 会依次发出 semester_changed / time_slots_changed / courses_changed。
         * 作息表为空时回退为 `TimeSlot::default_slots()` 的前 `total_weeks` 不适用，
         * 而是采用默认作息表全量（11 节），保证界面始终可渲染。
         */
        void load_snapshot(const ScheduleSnapshot& snapshot);

        /** @brief 清空课程与作息表，保留学期元数据。 */
        void clear_courses();

        // -------------------------------------------------------------- 周次查询

        /** @return 当前周（相对今天）；不在学期内返回 0。 */
        int current_week(const QDate& today = QDate::currentDate()) const;

        /** @return 指定日期所属周次；不在学期内返回 0。 */
        int week_of(const QDate& date) const;

        /** @return 学期总周数。 */
        int total_weeks() const;

        /** @return 指定周的周一。 */
        QDate week_start_date(int week) const;

        // -------------------------------------------------------------- 课表查询

        /**
         * @brief 查询某周某天的全部课程排布。
         * @param day_of_week 1=周一 ... 7=周日
         * @param week        周次（1 起）
         * @return 按“起始节次 → 课程名”排序的排布列表。
         */
        QList<PlacedSession> sessions_at(int day_of_week, int week) const;

        /** @brief 查询某日期当天的全部课程排布（内部换算周次与星期）。 */
        QList<PlacedSession> sessions_on_date(const QDate& date) const;

        /** @brief 查询某周的全部课程排布（周视图使用）。 */
        QList<PlacedSession> sessions_in_week(int week) const;

        /** @brief 查询某课程在某周的全部排布。 */
        QList<PlacedSession> sessions_of_course(const QString& course_id, int week) const;

        // -------------------------------------------------------------- 冲突检测

        /** @brief 对当前全部课程做冲突检测。 */
        QList<Conflict> detect_conflicts() const;

        /** @brief 只检测“候选课程 vs 现有课程”的冲突（编辑器中实时提示用）。 */
        QList<Conflict> detect_conflicts_for(const Course& candidate) const;

    signals:
        /** 学期元数据变化后发出（名称、起始日期、总周数等）。 */
        void semester_changed();

        /** 作息表被替换后发出。 */
        void time_slots_changed();

        /** 课程列表发生任何增删改后发出（QML 模型据此重载）。 */
        void courses_changed();

        /**
         * @brief 操作失败时发出。
         * @param message 面向用户的中文错误信息
         */
        void error_occurred(const QString& message);

    private:
        /** 为课程补齐 id / semester_id / 时间段 id / 颜色。 */
        void normalise_course(Course* course) const;

        /** 当前学期。 */
        Semester m_semester;

        /** 当前作息表。 */
        QList<TimeSlot> m_time_slots;

        /** 当前学期的课程。 */
        QList<Course> m_courses;
    };

} // namespace Schedule
