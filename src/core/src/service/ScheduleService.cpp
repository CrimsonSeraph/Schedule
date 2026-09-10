#include "core/service/ScheduleService.h"

#include "core/service/ConflictDetector.h"
#include "core/service/WeekCalculator.h"

#include <QUuid>

#include <algorithm>

namespace Schedule {

    namespace {

        /** @return 新建对象的 UUID 字符串（无花括号，便于直接作为主键）。 */
        QString new_id() {
            return QUuid::createUuid().toString(QUuid::WithoutBraces);
        }

        /** 把排布按“起始节次 → 课程名 → id”排序，保证渲染顺序稳定。 */
        bool placed_less_than(const ScheduleService::PlacedSession& left, const ScheduleService::PlacedSession& right) {
            if (left.session.start_slot != right.session.start_slot) {
                return left.session.start_slot < right.session.start_slot;
            }
            if (left.course_name != right.course_name) {
                return left.course_name < right.course_name;
            }
            return left.course_id < right.course_id;
        }

    } // namespace

    QTime ScheduleService::PlacedSession::start_time() const {
        return start_slot_definition.start_time;
    }

    QTime ScheduleService::PlacedSession::end_time() const {
        return end_slot_definition.end_time;
    }

    ScheduleService::ScheduleService(QObject* parent)
        : QObject(parent) {
    }

    ScheduleService::~ScheduleService() = default;

    // -------------------------------------------------------------------- 学期

    Semester ScheduleService::semester() const {
        return m_semester;
    }

    bool ScheduleService::has_semester() const {
        return m_semester.is_valid(nullptr);
    }

    bool ScheduleService::set_semester(const Semester& semester) {
        QString error;
        if (!semester.is_valid(&error)) {
            emit error_occurred(error);
            return false;
        }

        const bool semester_switched = m_semester.id != semester.id && !m_semester.id.isEmpty();
        m_semester = semester;
        if (semester_switched) {
            // 切换到另一学期：旧课程与作息表不再适用，必须清空，
            // 否则会出现“课程带着上学期周次渲染到本学期”的错乱。
            m_courses.clear();
            m_time_slots.clear();
            emit time_slots_changed();
            emit courses_changed();
        }
        emit semester_changed();
        return true;
    }

    // ------------------------------------------------------------------ 作息表

    QList<TimeSlot> ScheduleService::time_slots() const {
        return m_time_slots;
    }

    void ScheduleService::set_time_slots(const QList<TimeSlot>& time_slots) {
        m_time_slots = time_slots;
        std::stable_sort(m_time_slots.begin(), m_time_slots.end(), [](const TimeSlot& left, const TimeSlot& right) {
            return left.index < right.index;
        });
        emit time_slots_changed();
    }

    int ScheduleService::max_slot_index() const {
        return ConflictDetector::max_slot_index(m_time_slots);
    }

    TimeSlot ScheduleService::time_slot(int index) const {
        for (const TimeSlot& slot : m_time_slots) {
            if (slot.index == index) {
                return slot;
            }
        }
        return TimeSlot();
    }

    // -------------------------------------------------------------------- 课程

    QList<Course> ScheduleService::courses() const {
        return m_courses;
    }

    int ScheduleService::course_count() const {
        return static_cast<int>(m_courses.size());
    }

    int ScheduleService::index_of_course(const QString& course_id) const {
        for (int i = 0; i < m_courses.size(); ++i) {
            if (m_courses.at(i).id == course_id) {
                return i;
            }
        }
        return -1;
    }

    bool ScheduleService::find_course(const QString& course_id, Course* out_course) const {
        const int index = index_of_course(course_id);
        if (index < 0) {
            return false;
        }
        if (out_course) {
            *out_course = m_courses.at(index);
        }
        return true;
    }

    void ScheduleService::normalise_course(Course* course) const {
        if (!course) {
            return;
        }
        if (course->id.isEmpty()) {
            course->id = new_id();
        }
        if (course->semester_id.isEmpty()) {
            course->semester_id = m_semester.id;
        }
        course->ensure_session_ids();
        if (course->color.isEmpty()) {
            course->color = Course::default_color_for(course->name.isEmpty() ? course->id : course->name);
        }
    }

    bool ScheduleService::add_course(Course course, QString* error_message) {
        if (!has_semester()) {
            const QString error = QStringLiteral("尚未设置学期，无法添加课程");
            if (error_message) {
                *error_message = error;
            }
            emit error_occurred(error);
            return false;
        }

        normalise_course(&course);
        if (!course.semester_id.isEmpty() && course.semester_id != m_semester.id) {
            // 防止把别的学期课程写进当前学期。
            course.semester_id = m_semester.id;
        }

        QString error;
        if (!course.is_valid(max_slot_index(), &error)) {
            if (error_message) {
                *error_message = error;
            }
            emit error_occurred(error);
            return false;
        }
        if (index_of_course(course.id) >= 0) {
            const QString message = QStringLiteral("课程 id 已存在：%1").arg(course.id);
            if (error_message) {
                *error_message = message;
            }
            emit error_occurred(message);
            return false;
        }

        m_courses.append(course);
        emit courses_changed();
        return true;
    }

    bool ScheduleService::update_course(const Course& course, QString* error_message) {
        const int index = index_of_course(course.id);
        if (index < 0) {
            const QString error = QStringLiteral("未找到要更新的课程：%1").arg(course.id);
            if (error_message) {
                *error_message = error;
            }
            emit error_occurred(error);
            return false;
        }

        Course candidate = course;
        normalise_course(&candidate);
        QString error;
        if (!candidate.is_valid(max_slot_index(), &error)) {
            if (error_message) {
                *error_message = error;
            }
            emit error_occurred(error);
            return false;
        }

        m_courses[index] = candidate;
        emit courses_changed();
        return true;
    }

    bool ScheduleService::remove_course(const QString& course_id) {
        const int index = index_of_course(course_id);
        if (index < 0) {
            return false;
        }
        m_courses.removeAt(index);
        emit courses_changed();
        return true;
    }

    void ScheduleService::set_courses(const QList<Course>& courses) {
        m_courses = courses;
        for (Course& course : m_courses) {
            normalise_course(&course);
        }
        emit courses_changed();
    }

    // -------------------------------------------------------------------- 快照

    ScheduleSnapshot ScheduleService::snapshot() const {
        ScheduleSnapshot snapshot;
        snapshot.semester = m_semester;
        snapshot.time_slots = m_time_slots;
        snapshot.courses = m_courses;
        return snapshot;
    }

    void ScheduleService::load_snapshot(const ScheduleSnapshot& snapshot) {
        m_semester = snapshot.semester;
        m_time_slots = snapshot.time_slots;
        if (m_time_slots.isEmpty()) {
            // 没有作息表时回退到内置默认作息，保证周视图仍有节次标题可渲染。
            m_time_slots = TimeSlot::default_slots();
        }
        std::stable_sort(m_time_slots.begin(), m_time_slots.end(), [](const TimeSlot& left, const TimeSlot& right) {
            return left.index < right.index;
        });

        m_courses = snapshot.courses;
        for (Course& course : m_courses) {
            normalise_course(&course);
        }

        emit semester_changed();
        emit time_slots_changed();
        emit courses_changed();
    }

    void ScheduleService::clear_courses() {
        m_courses.clear();
        m_time_slots.clear();
        emit time_slots_changed();
        emit courses_changed();
    }

    // ---------------------------------------------------------------- 周次查询

    int ScheduleService::current_week(const QDate& today) const {
        return WeekCalculator::current_week(m_semester, today);
    }

    int ScheduleService::week_of(const QDate& date) const {
        return WeekCalculator::week_of(m_semester, date);
    }

    int ScheduleService::total_weeks() const {
        // 未设置学期时返回 0，便于上层直接用它判断“是否已有可展示的课表”。
        return has_semester() ? m_semester.total_weeks : 0;
    }

    QDate ScheduleService::week_start_date(int week) const {
        return WeekCalculator::week_start_date(m_semester, week);
    }

    // ---------------------------------------------------------------- 课表查询

    QList<ScheduleService::PlacedSession> ScheduleService::sessions_at(int day_of_week, int week) const {
        QList<PlacedSession> result;
        if (day_of_week < 1 || day_of_week > WeekCalculator::DAYS_PER_WEEK || week < 1) {
            return result;
        }

        const QDate date = WeekCalculator::date_of(m_semester, week, day_of_week);
        for (const Course& course : m_courses) {
            for (const CourseSession& session : course.sessions) {
                if (session.day_of_week != day_of_week || !session.weeks.contains(week)) {
                    continue;
                }
                PlacedSession placed;
                placed.course_id = course.id;
                placed.course_name = course.name;
                placed.color = course.display_color();
                placed.teacher = session.effective_teacher(course.teacher);
                placed.location = session.effective_location(course.location);
                placed.session = session;
                placed.week = week;
                placed.date = date;
                placed.start_slot_definition = time_slot(session.start_slot);
                placed.end_slot_definition = time_slot(session.end_slot());
                result.append(placed);
            }
        }

        std::stable_sort(result.begin(), result.end(), placed_less_than);
        return result;
    }

    QList<ScheduleService::PlacedSession> ScheduleService::sessions_on_date(const QDate& date) const {
        const int week = week_of(date);
        if (week <= 0) {
            return QList<PlacedSession>();
        }
        // 日期 → 星期：以“该周起始日”为第 1 天做偏移，避免依赖 start_date 是否为周一。
        const QDate week_begin = week_start_date(week);
        const int day_of_week = static_cast<int>(week_begin.daysTo(date)) + 1;
        return sessions_at(day_of_week, week);
    }

    QList<ScheduleService::PlacedSession> ScheduleService::sessions_in_week(int week) const {
        QList<PlacedSession> result;
        for (int day = 1; day <= WeekCalculator::DAYS_PER_WEEK; ++day) {
            result.append(sessions_at(day, week));
        }
        return result;
    }

    QList<ScheduleService::PlacedSession> ScheduleService::sessions_of_course(const QString& course_id, int week) const {
        QList<PlacedSession> result;
        Course course;
        if (!find_course(course_id, &course)) {
            return result;
        }

        for (const CourseSession& session : course.sessions) {
            // week <= 0 表示“不按周过滤”，用于课程编辑器展示全部时间段。
            if (week > 0 && !session.weeks.contains(week)) {
                continue;
            }
            PlacedSession placed;
            placed.course_id = course.id;
            placed.course_name = course.name;
            placed.color = course.display_color();
            placed.teacher = session.effective_teacher(course.teacher);
            placed.location = session.effective_location(course.location);
            placed.session = session;
            placed.week = week;
            placed.date = week > 0 ? WeekCalculator::date_of(m_semester, week, session.day_of_week) : QDate();
            placed.start_slot_definition = time_slot(session.start_slot);
            placed.end_slot_definition = time_slot(session.end_slot());
            result.append(placed);
        }

        std::stable_sort(result.begin(), result.end(), placed_less_than);
        return result;
    }

    // ---------------------------------------------------------------- 冲突检测

    QList<Conflict> ScheduleService::detect_conflicts() const {
        return ConflictDetector::detect(m_courses, m_semester, m_time_slots);
    }

    QList<Conflict> ScheduleService::detect_conflicts_for(const Course& candidate) const {
        QList<Conflict> conflicts = ConflictDetector::detect_in_course(candidate, m_semester, m_time_slots);

        // 与现有课程逐一比较；更新场景下跳过自身，避免把自己判成冲突。
        for (const Course& existing : m_courses) {
            if (!candidate.id.isEmpty() && existing.id == candidate.id) {
                continue;
            }
            conflicts.append(ConflictDetector::detect_between(candidate, existing, m_semester));
        }

        return conflicts;
    }

} // namespace Schedule
