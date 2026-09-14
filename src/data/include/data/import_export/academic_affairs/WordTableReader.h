#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

namespace Schedule {

    /**
     * @brief 从 Word 文档里读出来的一张表。
     *
     * `rows` 是**规范化后的矩形网格**：每行长度相同，被合并覆盖的格子取合并起始格
     * 的文本（见下）。这样调用方不必再关心 `gridSpan` / `vMerge` / `colspan` /
     * `rowspan`，只管按下标取格子。
     */
    struct WordTable {
        /** 规范化后的网格：`rows[行][列]`，越界/被合并覆盖读出空串。 */
        QList<QStringList> rows;

        /** 表格**之前**的正文（已去标签、已解码），用于提取学期、姓名等。 */
        QString context_text;

        /** @return 是否没有读到任何行。 */
        bool is_empty() const {
            return rows.isEmpty();
        }
    };

    /**
     * @brief Word 表格读取器：把 `.docx` 或 Word 版式 HTML 里的表格读成 `WordTable`。
     *
     * ## 支持的两种输入
     *
     * | 输入 | 读取方式 |
     * | --- | --- |
     * | `.docx`（OOXML 包） | 解出 `word/document.xml`，按 `w:tbl` / `w:tr` / `w:tc` 解析 |
     * | Word 版式 HTML（`.doc` 的常见真实形态、教务系统页面） | 按 `table` / `tr` / `td` 解析 |
     *
     * 两条路径的**输出语义完全一致**：同样的表格内容得到同样的 `WordTable`。
     *
     * ## 合并单元格的处理
     *
     * | 形态 | 处理 |
     * | --- | --- |
     * | OOXML `w:gridSpan` / HTML `colspan` | 该格横跨多列，内容记在第一列，其余列为空 |
     * | OOXML `w:vMerge` / HTML `rowspan` | 纵向合并；**延续行继承起始行的文本** |
     *
     * 让延续行“继承”而不是留空，是因为合并单元格在语义上就是一个格子：教务系统把
     * 一门跨节次的课合并成竖格时，把它当成“只有第一行有课”会直接丢课。调用方若担心
     * 重复，可以按内容去重（`EcjtuTimetableIo` 就是这么做的）。
     *
     * ## 换行
     *
     * 单元格内的视觉换行（OOXML `<w:br/>`、HTML `<br>`、段落结束）统一还原为 `\n`，
     * 因为教务课表正是用换行分隔「课程名 / 教师 @教室 / 周次 节次」的。
     *
     * ## 关于 `private/qzipreader_p.h`
     *
     * Qt 6 **没有公开的 zip 读取 API**（`QZipReader` 一直是私有实现）。这里直接使用它，
     * 理由：`.docx` 就是一个 zip，自己写 ZIP 目录解析 + DEFLATE 解压需要三百行左右的
     * 精细代码，出错风险远高于依赖一个 Qt 自带、多年未变、且被 Qt 自己的工具使用的实现。
     * 代价是构建需要 `Qt6::CorePrivate`（随官方安装包与各发行版 dev 包一同提供）。
     * 若将来 Qt 移除它，**唯一需要改的地方是 `.cpp` 里的 `read_document_xml()`**。
     */
    class WordTableReader {
    public:
        /**
         * @brief 读取 `.docx`（OOXML 包）里的第一张表。
         *
         * @param file_path     源文件绝对路径
         * @param out_table     输出表格
         * @param error_message 失败原因（中文）
         * @return 是否成功；失败时 `error_message` 可面向用户
         */
        static bool read_docx(const QString& file_path, WordTable* out_table, QString* error_message = nullptr);

        /**
         * @brief 读取 Word 版式 HTML 里的第一张**课表形态**的表格。
         *
         * 会先按声明/内容解码字节（GBK / UTF-8），再挑出包含「节次 + 星期」的那张表，
         * 因此同一份数据既可以是教务系统导出的 `.doc`，也可以是内嵌浏览器抓到的页面。
         *
         * @param data          原始字节（可能是 GBK）
         * @param out_table     输出表格
         * @param error_message 失败原因（中文）
         * @return 是否成功
         */
        static bool read_word_html(const QByteArray& data, WordTable* out_table, QString* error_message = nullptr);

        /**
         * @brief 读取 HTML **文本**（已解码）里的第一张课表形态的表格。
         * @param html          已解码的 HTML 文本
         */
        static bool read_html_text(const QString& html, WordTable* out_table, QString* error_message = nullptr);

        /**
         * @brief 解析 `word/document.xml` 的内容。
         *
         * 单独暴露出来是为了让单元测试可以直接喂 XML 字符串，不必先造一个 zip。
         */
        static bool parse_document_xml(const QString& xml, WordTable* out_table, QString* error_message = nullptr);
    };

} // namespace Schedule
