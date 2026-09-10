#pragma once

#include "core/model/Conflict.h"
#include "core/model/Course.h"
#include "core/model/Semester.h"
#include "core/model/TimeSlot.h"

#include <QList>

namespace Schedule {

    /**
     * @brief 冲突检测器：纯函数集合，不持有任何状态。
     *
     * 检测分三个层次：
     *  1. `detect_in_course()` —— 单课程自检（数据完整性、课程内部时间段重叠）；
     *  2. `detect_between()` —— 两门课程之间的时间冲突；
     *  3. `detect()` —— 全量扫描，输出可直接展示给用户的问题列表。
     *
     * 结果按“类型 → 星期 → 周次”排序，便于界面分组展示且顺序稳定（避免每次刷新跳变）。
     */
    class ConflictDetector {
    public:
        /** 检测开关：导入外部课表时可能需要放宽部分校验。 */
        struct Options {
            /** 是否把名称 / 课程代码相同的课程标为“重复课程”（提示级）。 */
            bool check_duplicate_courses = true;

            /** 是否校验节次是否超出作息表范围。 */
            bool check_slot_range = true;

            /** 是否校验周次是否超出学期总周数。 */
            bool check_week_range = true;

            /** 是否检测课程内部多个时间段互相重叠。 */
            bool check_self_overlap = true;
        };

        /**
         * @brief 全量冲突检测（使用默认检测开关）。
         * @see detect(const QList<Course>&, const Semester&, const QList<TimeSlot>&, const Options&)
         */
        static QList<Conflict> detect(const QList<Course>& courses, const Semester& semester, const QList<TimeSlot>& time_slots);

        /**
         * @brief 全量冲突检测。
         *
         * @param courses     待检测课程（应同属 `semester`）
         * @param semester    学期（提供总周数；无效学期时跳过周次范围校验）
         * @param time_slots  作息表（提供最大节次；为空时跳过节次范围校验）
         * @param options     检测开关
         * @return 问题列表，可能为空；已排序、去重。
         *
         * @note 这里使用**重载**而不是 `const Options& options = Options()` 默认实参：
         *       `Options` 是带成员初始化器的嵌套类型，在类定义内部用作默认实参会被
         *       Clang（Android / iOS 工具链）判为非法，而 MSVC 能通过。重载写法可移植。
         */
        static QList<Conflict> detect(const QList<Course>& courses,
            const Semester& semester,
            const QList<TimeSlot>& time_slots,
            const Options& options);

        /**
         * @brief 单课程自检（使用默认检测开关）。
         */
        static QList<Conflict> detect_in_course(const Course& course, const Semester& semester, const QList<TimeSlot>& time_slots);

        /**
         * @brief 单课程自检。
         *
         * 包含：课程名称、有无时间段、每个时间段的结构合法性、周次是否越界、
         * 节次是否越界，以及课程内部时间段之间的重叠。
         */
        static QList<Conflict> detect_in_course(const Course& course,
            const Semester& semester,
            const QList<TimeSlot>& time_slots,
            const Options& options);

        /**
         * @brief 两门课程之间的冲突检测（使用默认检测开关）。
         */
        static QList<Conflict> detect_between(const Course& first, const Course& second, const Semester& semester);

        /**
         * @brief 两门课程之间的冲突检测。
         *
         * 只检测时间重叠与重复课程；不重复输出各自的单课程问题。
         */
        static QList<Conflict> detect_between(const Course& first,
            const Course& second,
            const Semester& semester,
            const Options& options);

        /**
         * @brief 判断两个时间段是否冲突。
         *
         * 语义与 `CourseSession::conflicts_with()` 一致，另提供 `Conflict` 上下文填充。
         *
         * @param first_name  第一门课程名（用于生成可读描述）
         * @param second_name 第二门课程名
         * @param conflict    可选输出：填充好的冲突项（不含课程 id）
         * @return 是否冲突
         */
        static bool sessions_conflict(const CourseSession& first,
            const CourseSession& second,
            const QString& first_name,
            const QString& second_name,
            Conflict* conflict = nullptr);

        /** @return 作息表的最大节次序号；空表返回 0。 */
        static int max_slot_index(const QList<TimeSlot>& time_slots);

        /** @return 列表中是否存在阻断性冲突。 */
        static bool has_blocking_conflict(const QList<Conflict>& conflicts);

    private:
        /** 生成“周几第几节”的可读片段，供多个检测分支复用。 */
        static QString describe_time(int day_of_week, int from_slot, int to_slot);

        /** 对结果排序并去重，保证展示顺序稳定。 */
        static void normalise(QList<Conflict>* conflicts);
    };

} // namespace Schedule
