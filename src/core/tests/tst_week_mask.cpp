#include "core/model/WeekMask.h"

#include <QTest>

using Schedule::WeekMask;

namespace {

    /** @return 表达式的周次列表；解析失败时返回空列表并写入错误。 */
    QList<int> weeks_of(const QString& expression, int total_weeks) {
        const WeekMask mask = WeekMask::from_expression(expression, total_weeks);
        return mask.weeks();
    }

} // namespace

/**
 * @brief WeekMask 周次表达式解析与集合运算的单元测试。
 */
class TestWeekMask : public QObject {
    Q_OBJECT

private slots:
    /** 解析闭区间 `1-16`。 */
    void parses_closed_range();

    /** 解析带步长区间 `1-16/2`（单周）与 `2-16/2`（双周）。 */
    void parses_step_range();

    /** 解析“从某周起按步长直到学期末”的 `A/S` 形式。 */
    void parses_open_step();

    /** 解析 `*` / `all` / `odd` / `even` 关键字。 */
    void parses_keywords();

    /** 解析逗号分隔的混合片段。 */
    void parses_mixed_segments();

    /** 兼容中文逗号 / 分号与空格分隔。 */
    void parses_chinese_separators();

    /** 空表达式合法且结果为空掩码。 */
    void accepts_empty_expression();

    /** 非法表达式必须失败并给出中文原因。 */
    void rejects_invalid_expressions();

    /** 规范化表达式输出可再次解析且语义一致。 */
    void round_trips_expression();

    /** 人类可读文本包含单双周提示。 */
    void formats_display_string();

    /** 集合运算：并、交、差、包含。 */
    void performs_set_operations();

    /** 边界：MAX_WEEKS 内合法，超出即失败。 */
    void enforces_max_weeks_boundary();
};

void TestWeekMask::parses_closed_range() {
    QString error;
    const WeekMask mask = WeekMask::from_expression(QStringLiteral("1-16"), 16, &error);

    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(mask.count(), 16);
    QCOMPARE(mask.first_week(), 1);
    QCOMPARE(mask.last_week(), 16);
    QVERIFY(mask.contains(1));
    QVERIFY(mask.contains(16));
    QVERIFY(!mask.contains(17));
}

void TestWeekMask::parses_step_range() {
    const QList<int> odd_weeks = weeks_of(QStringLiteral("1-16/2"), 16);
    QCOMPARE(odd_weeks, QList<int>({1, 3, 5, 7, 9, 11, 13, 15}));

    const QList<int> even_weeks = weeks_of(QStringLiteral("2-16/2"), 16);
    QCOMPARE(even_weeks, QList<int>({2, 4, 6, 8, 10, 12, 14, 16}));
}

void TestWeekMask::parses_open_step() {
    // `2/2` 表示从第 2 周起、步长 2、直到学期末（共 16 周）
    const QList<int> weeks = weeks_of(QStringLiteral("2/2"), 16);
    QCOMPARE(weeks, QList<int>({2, 4, 6, 8, 10, 12, 14, 16}));
}

void TestWeekMask::parses_keywords() {
    QCOMPARE(weeks_of(QStringLiteral("*"), 16).size(), 16);
    QCOMPARE(weeks_of(QStringLiteral("all"), 16).size(), 16);
    QCOMPARE(weeks_of(QStringLiteral("odd"), 16), QList<int>({1, 3, 5, 7, 9, 11, 13, 15}));
    QCOMPARE(weeks_of(QStringLiteral("even"), 16), QList<int>({2, 4, 6, 8, 10, 12, 14, 16}));
    QCOMPARE(weeks_of(QStringLiteral("单周"), 16), QList<int>({1, 3, 5, 7, 9, 11, 13, 15}));
    QCOMPARE(weeks_of(QStringLiteral("双周"), 16), QList<int>({2, 4, 6, 8, 10, 12, 14, 16}));
}

void TestWeekMask::parses_mixed_segments() {
    QCOMPARE(weeks_of(QStringLiteral("1-4,6,8-10"), 16), QList<int>({1, 2, 3, 4, 6, 8, 9, 10}));
    QCOMPARE(weeks_of(QStringLiteral("3"), 16), QList<int>({3}));
}

void TestWeekMask::parses_chinese_separators() {
    // 兼容从 Excel / 网页复制来的全角标点与空格分隔
    QCOMPARE(weeks_of(QStringLiteral("1-4，6；8-10"), 16), QList<int>({1, 2, 3, 4, 6, 8, 9, 10}));
    QCOMPARE(weeks_of(QStringLiteral("1-4 6 8-10"), 16), QList<int>({1, 2, 3, 4, 6, 8, 9, 10}));
}

void TestWeekMask::accepts_empty_expression() {
    QString error;
    const WeekMask mask = WeekMask::from_expression(QString(), 16, &error);

    QVERIFY(error.isEmpty());
    QVERIFY(mask.is_empty());
    QVERIFY(WeekMask::validate_expression(QString(), 16));
}

void TestWeekMask::rejects_invalid_expressions() {
    const QStringList invalid = {
        QStringLiteral("0-5"),    // 周次必须从 1 开始
        QStringLiteral("5-3"),    // 起止颠倒
        QStringLiteral("abc"),    // 无法识别的片段
        QStringLiteral("1-16/0"), // 步长必须 > 0
        QStringLiteral("17"),     // 超出学期总周数
        QStringLiteral("1-70"),   // 超出 MAX_WEEKS
    };

    for (const QString& expression : invalid) {
        QString error;
        QVERIFY2(!WeekMask::validate_expression(expression, 16, &error), qPrintable(expression));
        QVERIFY2(!error.isEmpty(), qPrintable(expression));
    }

    // 尾随分隔符只产生空片段，会被忽略：`1-16,` 等价于 `1-16`
    QString error;
    QVERIFY(WeekMask::validate_expression(QStringLiteral("1-16,"), 16, &error));
    QCOMPARE(WeekMask::from_expression(QStringLiteral("1-16,"), 16), WeekMask::from_expression(QStringLiteral("1-16"), 16));
}

void TestWeekMask::round_trips_expression() {
    const QList<QPair<QString, QString>> cases = {
        {QStringLiteral("1-16"), QStringLiteral("1-16")},
        {QStringLiteral("1-16/2"), QStringLiteral("1-15/2")},
        {QStringLiteral("1-4,6,8-10"), QStringLiteral("1-4,6,8-10")},
        {QStringLiteral("3"), QStringLiteral("3")},
    };

    for (const auto& test_case : cases) {
        const WeekMask mask = WeekMask::from_expression(test_case.first, 16);
        const QString expression = mask.to_expression();
        QCOMPARE(expression, test_case.second);

        // 规范化结果必须能再次解析出同一集合（幂等）
        QCOMPARE(WeekMask::from_expression(expression, 16), mask);
    }

    QCOMPARE(WeekMask().to_expression(), QString());
}

void TestWeekMask::formats_display_string() {
    QCOMPARE(WeekMask().to_display_string(), QStringLiteral("无"));
    QCOMPARE(WeekMask::from_expression(QStringLiteral("1-15/2"), 16).to_display_string(),
        QStringLiteral("第 1-15/2 周（单周）"));
    QCOMPARE(WeekMask::from_expression(QStringLiteral("1-16"), 16).to_display_string(),
        QStringLiteral("第 1-16 周"));
}

void TestWeekMask::performs_set_operations() {
    const WeekMask left = WeekMask::from_expression(QStringLiteral("1-8"), 16);
    const WeekMask right = WeekMask::from_expression(QStringLiteral("5-12"), 16);

    QCOMPARE(left.united(right), WeekMask::from_expression(QStringLiteral("1-12"), 16));
    QCOMPARE(left.intersected(right), WeekMask::from_expression(QStringLiteral("5-8"), 16));
    QCOMPARE(left.subtracted(right), WeekMask::from_expression(QStringLiteral("1-4"), 16));
    QVERIFY(left.intersects(right));

    const WeekMask disjoint = WeekMask::from_expression(QStringLiteral("9-12"), 16);
    QVERIFY(!left.intersects(disjoint));
    QVERIFY(left.contains_all(WeekMask::from_expression(QStringLiteral("2-4"), 16)));
    QVERIFY(!left.contains_all(right));

    // 空集语义：不与任何集合相交，且被任何集合包含
    QVERIFY(!left.intersects(WeekMask()));
    QVERIFY(left.contains_all(WeekMask()));
}

void TestWeekMask::enforces_max_weeks_boundary() {
    QString error;
    const WeekMask boundary = WeekMask::from_expression(QStringLiteral("1-64"), 0, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(boundary.count(), WeekMask::MAX_WEEKS);

    QVERIFY(!WeekMask::validate_expression(QStringLiteral("1-65"), 0, &error));
    QVERIFY(!error.isEmpty());

    // 越界周次通过 add() 添加时被静默忽略，不会污染位图
    WeekMask mask = WeekMask();
    mask.add(0);
    mask.add(65);
    QVERIFY(mask.is_empty());
}

QTEST_GUILESS_MAIN(TestWeekMask)

#include "tst_week_mask.moc"
