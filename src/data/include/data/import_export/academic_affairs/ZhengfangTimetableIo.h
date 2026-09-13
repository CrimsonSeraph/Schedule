#pragma once

#include "data/import_export/IScheduleImporter.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace Schedule {

    /**
     * @brief 正方教务系统（zfn / zfsoft V9 及同源系统）课表导入器。
     *
     * ## 识别的文件形态
     *
     * 教务系统「导出 / 打印」出的课表文件通常命名为 `课表.xls`，但内容**并不是**
     * 真正的 Excel 二进制，而是 Excel 兼容的 HTML 页面：外层是可打印的表格
     * （`<table id="manualArrangeCourseTable">`，单元格 id 形如 `TD{列}_{行}`），
     * 内层 `<script>` 里是页面的数据源：
     *
     * ```javascript
     * var table0 = new CourseTable(2026, 84);
     * var unitCount = 12;
     * var actTeachers = [{id: 6017, name: "徐一达", lab: false}, ...];
     * activity = new TaskActivity(actTeacherId.join(','), actTeacherName.join(','),
     *     "19626(04311190.02)", "劳动教育实践——蛋糕面包制作(04311190.02)",
     *     "1766", "老图书馆北楼-食品专业实验室（1）-103(主校区)",
     *     "00001000000000000000000000000000000000000000000000000", null, null,
     *     assistantName, "", "");
     * index = 5 * unitCount + 0;
     * table0.activities[index][table0.activities[index].length] = activity;
     * ```
     *
     * 同一份页面也会被教务系统直接渲染成 HTML 表格（内置浏览器抓取到的即是这种
     * 形态）。两种形态的**有效信息完全一致**，本导入器统一以 `<script>` 中的
     * `TaskActivity` 为准：只有它带有逐周的位图，而 HTML 表格里的周次是折叠后的
     * 文本（形如 `(1-8 10-14,5J310)`），无法无损还原。
     *
     * ## 关键解析约定（已用样本文件逐格交叉验证）
     *
     * | 约定 | 说明 |
     * | ---- | ---- |
     * | 单元格下标 | `index = 星期 * unitCount + 节次`，**星期从 0 开始**（0=周一），**节次从 0 开始** |
     * | 周次位图 | 第 i 个字符为 `1` 表示**第 i 周**有课（下标 0 不使用，故第 1 周对应下标 1） |
     * | 连续节次 | 一个 `TaskActivity` 可挂在多个 `index` 上，需按“同一天 + 节次连续”合并为一个时间段 |
     * | 教师名 | 不在 `TaskActivity` 参数里，而来自紧邻其上的 `actTeachers = [{name:"…"}]` |
     * | 课程代码 | 形如 `19626(04311190.02)`，括号内 `04311190.02` 为课程代码，括号外为教学班序号 |
     * | 编码 | 页面声明 `<meta charset="GBK">`，经 `decode_html_bytes()` 嗅探后解码 |
     *
     * ## 分层与职责
     *
     * 本类只负责「字节 → `ScheduleSnapshot`」，不合并、不落库；冲突检测与合并策略
     * 由 `ImportManager` 统一处理，与 JSON / CSV / ICS 导入完全一致。
     * 解析过程**不访问网络、不读取任何凭证**。
     */
    class ZhengfangTimetableIo : public IScheduleImporter {
    public:
        ScheduleFormat format() const override;

        QString display_name() const override;

        QStringList extensions() const override;

        bool can_import(const QString& file_path, QString* error_message = nullptr) const override;

        bool parse(const QString& file_path, ScheduleSnapshot* out_snapshot, QString* error_message = nullptr) const override;

        bool parse_data(const QByteArray& data,
            const QString& source_name,
            ScheduleSnapshot* out_snapshot,
            QString* error_message = nullptr) const override;
    };

} // namespace Schedule
