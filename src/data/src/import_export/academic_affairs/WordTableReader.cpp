#include "data/import_export/academic_affairs/WordTableReader.h"

#include "data/import_export/academic_affairs/CharsetUtil.h"

// Qt 6 没有公开的 zip 读取 API；理由与替代方案见头文件注释
#include <private/qzipreader_p.h>

#include <QHash>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>

namespace Schedule {

    namespace {

        /** 解析 OOXML / HTML 时通用的正则选项。 */
        constexpr QRegularExpression::PatternOptions MARKUP_OPTIONS =
            QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption;

        /** @brief 统一设置错误。 */
        bool fail(QString* error_message, const QString& message) {
            if (error_message) {
                *error_message = message;
            }
            return false;
        }

        /** @return 展开 `&amp;` `&#39;` `&#x4e2d;` 这类实体；无法识别时原样保留。 */
        QString unescape_entities(const QString& text) {
            if (!text.contains(QLatin1Char('&'))) {
                return text;
            }

            QString result;
            result.reserve(text.size());

            int index = 0;
            while (index < text.size()) {
                const QChar character = text.at(index);
                if (character != QLatin1Char('&')) {
                    result.append(character);
                    ++index;
                    continue;
                }

                // 实体名不会太长；找不到分号或过长就当作普通字符，避免误吞正文
                const int semicolon = text.indexOf(QLatin1Char(';'), index + 1);
                if (semicolon < 0 || semicolon - index > 12) {
                    result.append(character);
                    ++index;
                    continue;
                }

                const QString entity = text.mid(index + 1, semicolon - index - 1);
                QString replacement;
                if (entity == QLatin1String("amp")) {
                    replacement = QStringLiteral("&");
                }
                else if (entity == QLatin1String("lt")) {
                    replacement = QStringLiteral("<");
                }
                else if (entity == QLatin1String("gt")) {
                    replacement = QStringLiteral(">");
                }
                else if (entity == QLatin1String("quot")) {
                    replacement = QStringLiteral("\"");
                }
                else if (entity == QLatin1String("apos")) {
                    replacement = QStringLiteral("'");
                }
                else if (entity == QLatin1String("nbsp")) {
                    replacement = QStringLiteral(" ");
                }
                else if (entity.startsWith(QLatin1Char('#'))) {
                    const bool hexadecimal = entity.startsWith(QLatin1String("#x"), Qt::CaseInsensitive);
                    bool ok = false;
                    const uint code = hexadecimal ? entity.mid(2).toUInt(&ok, 16) : entity.mid(1).toUInt(&ok, 10);
                    // 排除代理区与非字符，避免造出非法 QString
                    if (ok && code > 0 && code <= 0x10FFFF && (code < 0xD800 || code > 0xDFFF)) {
                        const char32_t codepoint = static_cast<char32_t>(code);
                        replacement = QString::fromUcs4(&codepoint, 1);
                    }
                }

                if (replacement.isNull()) {
                    result.append(character);
                    ++index;
                    continue;
                }
                result.append(replacement);
                index = semicolon + 1;
            }
            return result;
        }

        /**
         * @brief 取出一段 OOXML 片段里的可见文本。
         *
         * `<w:br/>`、`<w:tab/>` 与段落结束都还原成空白字符：教务课表正是用换行分隔
         * 「课程名 / 教师 @教室 / 周次 节次」的，丢掉换行就等于丢掉字段边界。
         */
        QString text_from_ooxml(const QString& fragment) {
            // 模式中含 `"`，用自定义分隔符 RX，避免原始字符串提前结束
            static const QRegularExpression token(
                QStringLiteral(R"RX(<w:t(?:\s[^>]*)?>(.*?)</w:t>|<w:br(?:\s[^>]*)?/>|<w:tab(?:\s[^>]*)?/>|</w:p>)RX"),
                QRegularExpression::DotMatchesEverythingOption);

            QString result;
            QRegularExpressionMatchIterator matches = token.globalMatch(fragment);
            while (matches.hasNext()) {
                const QRegularExpressionMatch match = matches.next();
                if (match.capturedStart(1) >= 0) {
                    result.append(unescape_entities(match.captured(1)));
                    continue;
                }
                result.append(match.captured(0).startsWith(QLatin1String("<w:tab"), Qt::CaseInsensitive)
                        ? QLatin1Char('\t')
                        : QLatin1Char('\n'));
            }
            return result;
        }

        /** @brief 取出一段 HTML 片段里的可见文本。 */
        QString text_from_html(const QString& fragment) {
            static const QRegularExpression line_break(QStringLiteral(R"(<br\b[^>]*>)"), MARKUP_OPTIONS);
            static const QRegularExpression paragraph_end(QStringLiteral(R"(</p\s*>)"), MARKUP_OPTIONS);
            static const QRegularExpression tag(QStringLiteral(R"(<[^>]*>)"));

            QString text = fragment;
            text.replace(line_break, QStringLiteral("\n"));
            text.replace(paragraph_end, QStringLiteral("\n"));
            text.remove(tag);
            return unescape_entities(text);
        }

        /** @return 取出整数属性值（`colspan=2` / `colspan="2"` 都认）；缺失或非法返回 fallback。 */
        int attribute_as_int(const QString& attributes, const QString& name, int fallback) {
            const QRegularExpression pattern(
                QStringLiteral(R"(\b%1\s*=\s*["']?(\d+))").arg(QRegularExpression::escape(name)),
                QRegularExpression::CaseInsensitiveOption);
            const QRegularExpressionMatch match = pattern.match(attributes);
            if (!match.hasMatch()) {
                return fallback;
            }
            bool ok = false;
            const int value = match.captured(1).toInt(&ok);
            return ok && value > 0 ? value : fallback;
        }

        /** @brief 去掉每行首尾空白、丢掉空行。 */
        QString tidy_cell_text(const QString& text) {
            QStringList lines;
            for (const QString& line : text.split(QLatin1Char('\n'))) {
                const QString trimmed = line.trimmed();
                if (!trimmed.isEmpty()) {
                    lines.append(trimmed);
                }
            }
            return lines.join(QLatin1Char('\n'));
        }

        /** @brief 把各行补齐到等长，保证网格是矩形。 */
        void pad_to_rectangle(QList<QStringList>& rows) {
            int width = 0;
            for (const QStringList& row : rows) {
                width = qMax(width, static_cast<int>(row.size()));
            }
            for (QStringList& row : rows) {
                while (row.size() < width) {
                    row.append(QString());
                }
            }
        }

        /**
         * @brief 去掉 `<head>` / `<script>` / `<style>` / 注释，只留下正文。
         *
         * 这一步是必要的：教务系统导出的 `.doc` 里 `<title>` 常常是**模板残留**
         * （真实样本的标题停在「2015-2016学年第一学期 {Name} 课表」，正文却已经是
         * 「2026-2027 第一学期」）。若把标题也算进“表格之前的正文”，就会拿残留年份
         * 当学期；而且同一份内容转成 `.docx` 后标题被挪进 docProps，
         * 两条路径还会得出不同的学期。
         */
        QString strip_non_body_markup(const QString& html) {
            static const QRegularExpression comment(QStringLiteral(R"(<!--.*?-->)"), MARKUP_OPTIONS);
            static const QRegularExpression script(QStringLiteral(R"(<script\b[^>]*>.*?</script\s*>)"), MARKUP_OPTIONS);
            static const QRegularExpression style(QStringLiteral(R"(<style\b[^>]*>.*?</style\s*>)"), MARKUP_OPTIONS);
            static const QRegularExpression head(QStringLiteral(R"(<head\b[^>]*>.*?</head\s*>)"), MARKUP_OPTIONS);

            QString result = html;
            result.remove(comment);
            result.remove(script);
            result.remove(style);
            result.remove(head);
            return result;
        }

        /** @return 文本里是否出现了课表表头（`节次` 且带星期列）。 */
        bool looks_like_timetable(const QString& text) {
            const bool has_slot_header = text.contains(QStringLiteral("节次"));
            const bool has_day_header = text.contains(QStringLiteral("星期一")) || text.contains(QStringLiteral("周一"));
            return has_slot_header && has_day_header;
        }

        // ------------------------------------------------------------ OOXML 路径

        /** OOXML 单元格（`<w:tc>`）。 */
        struct OoxmlCell {
            int span = 1;
            bool merge_restart = false;
            bool merge_continue = false;
            QString text;
        };

        /** @brief 解析一个 `<w:tr>` 里的全部 `<w:tc>`。 */
        QList<OoxmlCell> parse_ooxml_row(const QString& row_fragment) {
            static const QRegularExpression cell_pattern(
                QStringLiteral(R"RX(<w:tc(?:\s[^>]*)?>(.*?)</w:tc>)RX"),
                QRegularExpression::DotMatchesEverythingOption);
            static const QRegularExpression properties_pattern(
                QStringLiteral(R"RX(<w:tcPr(?:\s[^>]*)?>(.*?)</w:tcPr>)RX"),
                QRegularExpression::DotMatchesEverythingOption);
            static const QRegularExpression span_pattern(
                QStringLiteral(R"RX(<w:gridSpan\b[^>]*\bw:val\s*=\s*"(\d+)")RX"),
                QRegularExpression::CaseInsensitiveOption);
            static const QRegularExpression merge_pattern(
                QStringLiteral(R"RX(<w:vMerge\b[^>]*>)RX"),
                QRegularExpression::CaseInsensitiveOption);
            static const QRegularExpression merge_value_pattern(
                QStringLiteral(R"RX(<w:vMerge\b[^>]*\bw:val\s*=\s*"(\w+)")RX"),
                QRegularExpression::CaseInsensitiveOption);

            QList<OoxmlCell> cells;
            QRegularExpressionMatchIterator matches = cell_pattern.globalMatch(row_fragment);
            while (matches.hasNext()) {
                const QString body = matches.next().captured(1);

                OoxmlCell cell;
                const QRegularExpressionMatch properties = properties_pattern.match(body);
                if (properties.hasMatch()) {
                    const QString attributes = properties.captured(1);

                    const QRegularExpressionMatch span = span_pattern.match(attributes);
                    if (span.hasMatch()) {
                        cell.span = qMax(1, span.captured(1).toInt());
                    }

                    if (merge_pattern.match(attributes).hasMatch()) {
                        // `<w:vMerge/>`（无 val）按 OOXML 规范等同于 continue
                        const QRegularExpressionMatch value = merge_value_pattern.match(attributes);
                        const QString kind = value.hasMatch() ? value.captured(1).toLower() : QStringLiteral("continue");
                        cell.merge_restart = kind == QLatin1String("restart");
                        cell.merge_continue = !cell.merge_restart;
                    }
                }
                cell.text = tidy_cell_text(text_from_ooxml(body));
                cells.append(cell);
            }
            return cells;
        }

        /** @brief 解析 `word/document.xml`，填充 `WordTable`。 */
        bool build_ooxml_table(const QString& xml, WordTable* out_table, QString* error_message) {
            static const QRegularExpression row_pattern(
                QStringLiteral(R"RX(<w:tr(?:\s[^>]*)?>(.*?)</w:tr>)RX"),
                QRegularExpression::DotMatchesEverythingOption);
            static const QRegularExpression table_pattern(
                QStringLiteral(R"RX(<w:tbl(?:\s[^>]*)?>(.*?)</w:tbl>)RX"),
                QRegularExpression::DotMatchesEverythingOption);

            // 选表规则与 HTML 路径保持一致：优先“看起来像课表”的那张，否则用第一张
            QString table_body;
            QRegularExpressionMatchIterator tables = table_pattern.globalMatch(xml);
            while (tables.hasNext()) {
                const QString candidate = tables.next().captured(1);
                if (table_body.isEmpty()) {
                    table_body = candidate;
                }
                if (looks_like_timetable(text_from_ooxml(candidate))) {
                    table_body = candidate;
                    break;
                }
            }
            if (table_body.isEmpty()) {
                return fail(error_message, QStringLiteral("文档里没有表格（未找到 <w:tbl>）"));
            }

            const int table_start = xml.indexOf(QStringLiteral("<w:tbl"));
            out_table->context_text = tidy_cell_text(text_from_ooxml(xml.left(table_start)));

            QList<QStringList> rows;
            QHash<int, QString> open_merge; // 列 -> 纵向合并起始格的文本
            QSet<int> open_columns;         // 仍在纵向合并中的列

            QRegularExpressionMatchIterator row_matches = row_pattern.globalMatch(table_body);
            while (row_matches.hasNext()) {
                const QList<OoxmlCell> cells = parse_ooxml_row(row_matches.next().captured(1));

                QStringList row;
                QSet<int> still_open;
                int column = 0;

                for (const OoxmlCell& cell : cells) {
                    const int cell_column = column;
                    QString text = cell.text;

                    if (cell.merge_restart) {
                        open_merge.insert(cell_column, text);
                        still_open.insert(cell_column);
                    }
                    else if (cell.merge_continue) {
                        // 合并格在语义上就是一个格子：延续行继承起始行的内容
                        text = open_merge.value(cell_column);
                        if (open_columns.contains(cell_column)) {
                            still_open.insert(cell_column);
                        }
                    }

                    while (row.size() < cell_column) {
                        row.append(QString());
                    }
                    row.append(text);
                    for (int extra = 1; extra < cell.span; ++extra) {
                        row.append(QString());
                    }
                    column = cell_column + cell.span;
                }

                open_columns = still_open;
                rows.append(row);
            }

            if (rows.isEmpty()) {
                return fail(error_message, QStringLiteral("文档里没有表格行（未找到 <w:tr>）"));
            }

            pad_to_rectangle(rows);
            out_table->rows = rows;
            return true;
        }

        // ------------------------------------------------------------- HTML 路径

        /** HTML 单元格（`<td>` / `<th>`）。 */
        struct HtmlCell {
            int span = 1;
            int row_span = 1;
            QString text;
        };

        /** @brief 解析一个 `<tr>` 里的全部 `<td>` / `<th>`。 */
        QList<HtmlCell> parse_html_row(const QString& row_fragment) {
            static const QRegularExpression cell_pattern(
                QStringLiteral(R"(<t[dh]\b([^>]*)>(.*?)</t[dh]\s*>)"),
                MARKUP_OPTIONS);

            QList<HtmlCell> cells;
            QRegularExpressionMatchIterator matches = cell_pattern.globalMatch(row_fragment);
            while (matches.hasNext()) {
                const QRegularExpressionMatch match = matches.next();

                HtmlCell cell;
                cell.span = attribute_as_int(match.captured(1), QStringLiteral("colspan"), 1);
                cell.row_span = attribute_as_int(match.captured(1), QStringLiteral("rowspan"), 1);
                cell.text = tidy_cell_text(text_from_html(match.captured(2)));
                cells.append(cell);
            }
            return cells;
        }

        /** @brief 解析（已解码的）HTML 文本，填充 `WordTable`。 */
        bool build_html_table(const QString& raw_html, WordTable* out_table, QString* error_message) {
            static const QRegularExpression table_pattern(
                QStringLiteral(R"(<table\b[^>]*>(.*?)</table\s*>)"),
                MARKUP_OPTIONS);
            static const QRegularExpression row_pattern(
                QStringLiteral(R"(<tr\b[^>]*>(.*?)</tr\s*>)"),
                MARKUP_OPTIONS);

            // 先剥掉 head / script / style / 注释：它们既不是正文，
            // 也会让“表格之前的正文”掺进模板残留的标题
            const QString html = strip_non_body_markup(raw_html);

            QString table_body;
            QRegularExpressionMatchIterator tables = table_pattern.globalMatch(html);
            while (tables.hasNext()) {
                const QString candidate = tables.next().captured(1);
                if (table_body.isEmpty()) {
                    table_body = candidate;
                }
                if (looks_like_timetable(text_from_html(candidate))) {
                    table_body = candidate;
                    break;
                }
            }
            if (table_body.isEmpty()) {
                return fail(error_message, QStringLiteral("页面里没有表格（未找到 <table>）"));
            }

            const int table_start = html.indexOf(QStringLiteral("<table"), 0, Qt::CaseInsensitive);
            out_table->context_text = tidy_cell_text(text_from_html(html.left(table_start)));

            // HTML 的纵向合并用 rowspan 表示：延续行**不写**那个 <td>，
            // 所以要按列维护“还剩几行、内容是什么”，遇到被覆盖的列就跳过
            struct Carry {
                int remaining = 0;
                QString text;
            };
            QHash<int, Carry> carries;

            QList<QStringList> rows;
            QRegularExpressionMatchIterator row_matches = row_pattern.globalMatch(table_body);
            while (row_matches.hasNext()) {
                const QList<HtmlCell> cells = parse_html_row(row_matches.next().captured(1));

                QStringList row;
                int column = 0;

                // 把当前列上尚未结束的 rowspan 落进本行，然后前进到下一列
                const auto drain_carries = [&]() {
                    while (true) {
                        auto carry = carries.find(column);
                        if (carry == carries.end() || carry->remaining <= 0) {
                            return;
                        }
                        while (row.size() <= column) {
                            row.append(QString());
                        }
                        row[column] = carry->text;
                        if (--carry->remaining <= 0) {
                            carries.erase(carry);
                        }
                        ++column;
                    }
                };

                for (const HtmlCell& cell : cells) {
                    drain_carries();
                    const int cell_column = column;

                    while (row.size() < cell_column) {
                        row.append(QString());
                    }
                    row.append(cell.text);
                    for (int extra = 1; extra < cell.span; ++extra) {
                        row.append(QString());
                    }
                    if (cell.row_span > 1) {
                        Carry carry;
                        carry.remaining = cell.row_span - 1;
                        carry.text = cell.text;
                        carries.insert(cell_column, carry);
                    }
                    column = cell_column + cell.span;
                }
                drain_carries();

                rows.append(row);
            }

            if (rows.isEmpty()) {
                return fail(error_message, QStringLiteral("表格里没有行（未找到 <tr>）"));
            }

            pad_to_rectangle(rows);
            out_table->rows = rows;
            return true;
        }

    } // namespace

    bool WordTableReader::parse_document_xml(const QString& xml, WordTable* out_table, QString* error_message) {
        if (!out_table) {
            return fail(error_message, QStringLiteral("输出表格为空"));
        }
        if (xml.trimmed().isEmpty()) {
            return fail(error_message, QStringLiteral("word/document.xml 内容为空"));
        }
        return build_ooxml_table(xml, out_table, error_message);
    }

    bool WordTableReader::read_docx(const QString& file_path, WordTable* out_table, QString* error_message) {
        if (!out_table) {
            return fail(error_message, QStringLiteral("输出表格为空"));
        }

        QZipReader reader(file_path);
        if (reader.status() != QZipReader::NoError) {
            return fail(error_message,
                QStringLiteral("%1 不是有效的 .docx（无法作为 zip 打开）；"
                               "若是 Word 97-2003 的 .doc，请先转换或改用 Word 版式导入")
                    .arg(file_path));
        }

        const QByteArray document = reader.fileData(QStringLiteral("word/document.xml"));
        if (document.isEmpty()) {
            return fail(error_message, QStringLiteral("%1 缺少 word/document.xml，不是 Word 文档").arg(file_path));
        }
        return parse_document_xml(QString::fromUtf8(document), out_table, error_message);
    }

    bool WordTableReader::read_html_text(const QString& html, WordTable* out_table, QString* error_message) {
        if (!out_table) {
            return fail(error_message, QStringLiteral("输出表格为空"));
        }
        if (html.trimmed().isEmpty()) {
            return fail(error_message, QStringLiteral("内容为空"));
        }
        return build_html_table(html, out_table, error_message);
    }

    bool WordTableReader::read_word_html(const QByteArray& data, WordTable* out_table, QString* error_message) {
        if (data.isEmpty()) {
            return fail(error_message, QStringLiteral("内容为空"));
        }
        // 教务系统导出的是 GB2312 / GBK，抓到的页面可能是 UTF-8；由声明优先、内容兜底
        return read_html_text(decode_html_bytes(data, nullptr), out_table, error_message);
    }

} // namespace Schedule
