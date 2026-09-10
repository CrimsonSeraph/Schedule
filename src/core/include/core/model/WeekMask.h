#pragma once

#include <QList>
#include <QString>
#include <QtGlobal>

namespace Schedule {

    /**
     * @brief 周次掩码：以 64 位位图表示一组周次（第 1 周对应 bit 0）。
     *
     * 课表场景中一门课的“上课周次”是离散集合，例如第 1-16 周、单周、第 3,5,7 周等。
     * 本类同时承担**周次表达式解析**与**集合运算**两项职责。
     *
     * 表达式由若干片段组成，片段之间可用 `,`、`，`、`;`、`；` 或空白分隔：
     *
     * | 片段        | 含义                                  | 示例             |
     * | ----------- | ------------------------------------- | ---------------- |
     * | `N`         | 单个周次                              | `3`              |
     * | `A-B`       | 闭区间周次                            | `1-16`           |
     * | `A-B/S`     | 带步长的闭区间（S >= 1）              | `1-16/2`（单周） |
     * | `A/S`       | 从 A 开始、步长为 S 直到学期末        | `2/2`（双周）    |
     * | `*` / `all` | 学期全部周次                          | `*`              |
     * | `odd`       | 单周（1,3,5,...）                     | `odd`            |
     * | `even`      | 双周（2,4,6,...）                     | `even`           |
     *
     * 注意：单学期周数上限为 MAX_WEEKS（64），超出范围会被判定为解析错误。
     */
    class WeekMask {
    public:
        /** 单学期可表示的最大周数：受 64 位掩码限制。 */
        static constexpr int MAX_WEEKS = 64;

        /** 构造空掩码（不含任何周次）。 */
        WeekMask() = default;

        /** @param bits 原始位图，bit(week-1) 置位表示包含第 week 周。 */
        explicit WeekMask(quint64 bits);

        /** @return 由原始位图构造的掩码。 */
        static WeekMask from_bits(quint64 bits);

        /** @return 由显式周次列表构造的掩码；越界周次被静默忽略。 */
        static WeekMask from_weeks(const QList<int>& weeks);

        /** @return 覆盖 1..total_weeks 全部周次的掩码。 */
        static WeekMask all(int total_weeks);

        /** @return 1..total_weeks 中的所有单周（1,3,5,...）。 */
        static WeekMask odd(int total_weeks);

        /** @return 1..total_weeks 中的所有双周（2,4,6,...）。 */
        static WeekMask even(int total_weeks);

        /**
         * @brief 解析周次表达式。
         *
         * @param expression    周次表达式，语法见类注释；空字符串解析为空掩码。
         * @param total_weeks   学期总周数：> 0 时同时校验上界；<= 0 时仅按 MAX_WEEKS 校验。
         * @param error_message 可选输出参数，解析失败时写入中文错误原因（含出错片段）。
         * @return 解析成功返回掩码；失败返回空掩码，并保证 error_message 非空。
         *
         * @note 解析是**全有或全无**的：任一片段非法即整体失败，避免半截数据写库。
         */
        static WeekMask from_expression(const QString& expression, int total_weeks, QString* error_message = nullptr);

        /**
         * @brief 仅校验表达式合法性，不保留结果。
         * @see from_expression
         */
        static bool validate_expression(const QString& expression, int total_weeks, QString* error_message = nullptr);

        /** @return 掩码是否不含任何周次。 */
        bool is_empty() const;

        /** @return 是否包含第 week 周；越界返回 false。 */
        bool contains(int week) const;

        /** @return 包含的周次数量。 */
        int count() const;

        /** @return 最小周次；空掩码返回 0。 */
        int first_week() const;

        /** @return 最大周次；空掩码返回 0。 */
        int last_week() const;

        /** @return 原始位图，便于序列化与比较。 */
        quint64 bits() const;

        /** @return 升序排列的周次列表。 */
        QList<int> weeks() const;

        /** 加入一周；越界（<1 或 >MAX_WEEKS）时忽略。 */
        void add(int week);

        /** 移除一周；越界时忽略。 */
        void remove(int week);

        /** 清空全部周次。 */
        void clear();

        /** 用显式列表整体替换当前内容（越界元素被忽略）。 */
        void set_weeks(const QList<int>& weeks);

        /** @return 并集。 */
        WeekMask united(const WeekMask& other) const;

        /** @return 交集。 */
        WeekMask intersected(const WeekMask& other) const;

        /** @return 差集：属于 *this 但不属于 other 的周次。 */
        WeekMask subtracted(const WeekMask& other) const;

        /** @return 两者是否存在公共周次；空掩码之间视为不相交。 */
        bool intersects(const WeekMask& other) const;

        /** @return 是否完全包含 other（空集被任何掩码包含）。 */
        bool contains_all(const WeekMask& other) const;

        /**
         * @brief 输出规范化表达式。
         *
         * 若整体构成步长 > 1 的等差数列则输出 `A-B/S`（如 `1-16/2`），
         * 否则按连续区间输出（如 `1-4,6,8-10`）；空掩码返回空字符串。
         */
        QString to_expression() const;

        /** @return 人类可读文本，如 `第 1-15 周（单周）`；空掩码返回 `无`。 */
        QString to_display_string() const;

        bool operator==(const WeekMask& other) const;
        bool operator!=(const WeekMask& other) const;

    private:
        /** 位图：bit(week-1) 置位表示包含第 week 周。 */
        quint64 m_bits = 0;
    };

} // namespace Schedule
