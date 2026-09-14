#pragma once

#include "data/import_export/IScheduleImporter.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace Schedule {

    /**
     * @brief 华东交通大学教务综合管理系统导出的 Word 课表导入器。
     *
     * ## 输入形态
     *
     * 该系统「导出」得到的课表是一张 Word 表格，实际有三种落地形态，本导入器都支持：
     *
     * | 形态 | 判断依据 | 处理 |
     * | --- | --- | --- |
     * | Word 版式 HTML（最常见，扩展名常是 `.doc`） | `<meta name=ProgId content=Word.Document>`、`MsoNormalTable` | 直接按 HTML 表格解析 |
     * | 真正的 `.docx`（OOXML 包） | zip 魔数 `PK\x03\x04` | 解出 `word/document.xml` 解析 |
     * | Word 97-2003 二进制 `.doc`（OLE 复合文档） | `D0 CF 11 E0` | 先经 `DocConvertUtil` 转成 `.docx`，再解析 |
     *
     * 第三条依赖本机装有 Word 或 LibreOffice；前两条不需要任何外部程序。
     *
     * ## 表格结构（已用真实导出文件逐格核对）
     *
     * ```text
     * ┌───────┬────────────┬────────────┬─ … ─┐
     * │ 节次  │  星期一    │  星期二    │     │   ← 表头
     * ├───────┼────────────┼────────────┼─ … ─┤
     * │ 1-2节 │ 高等数学   │            │     │   ← 左侧是节次区间
     * │       │ 张老师 @31-101          │     │   ← 教师 @教室
     * │       │ 1-8 1,2                 │     │   ← 周次 节次
     * ├───────┼────────────┼────────────┼─ … ─┤
     * │ 3-4节 │ …          │            │     │
     * └───────┴────────────┴────────────┴─ … ─┘
     * ```
     *
     * ## 关键解析约定
     *
     * | 约定 | 说明 |
     * | ---- | ---- |
     * | 单元格内条目顺序 | **课程名 → 教师 @教室 → 周次 节次**（与方正教务相反，方正没有 @教室 这一行） |
     * | 一个格子多门课 | 按三行一组重复；用「含 `@` 的行」当锚点定位其余两行 |
     * | 第三行的两个字段 | 第一个是**周次**（`4-19`、`1-16周(单)`、`1-8,10-16`），第二个是该活动实际占用的**节次列表**（`1,2`、`9,10,11`） |
     * | 节次列表是权威 | 一个跨节次的活动会出现在它覆盖的**每一行**里（`1,2,3` 同时出现在 `1-2节` 与 `3-4节` 行，OOXML 形态里则是一格 `vMerge`）。此处以第三行的节次列表为准，并按内容去重，避免重复计课 |
     * | 连续节次合并 | 节次列表里连续的一段合并为一个时间段；不连续则拆开 |
     * | 周次单双 | `(单)` / `(双)`、半角或全角括号都识别 |
     * | 课程聚合 | 该系统导出**不含课程代码**，因此按课程名聚合；团队授课的不同教师保留在各自时间段上 |
     *
     * ## 分层与职责
     *
     * 本类只负责「文档 → `ScheduleSnapshot`」，不合并、不落库；冲突检测与合并策略由
     * `ImportManager` 统一处理，与 JSON / CSV / ICS / 正方教务导入完全一致。
     * 解析过程**不访问网络、不读取任何凭证**。
     */
    class EcjtuTimetableIo : public IScheduleImporter {
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
