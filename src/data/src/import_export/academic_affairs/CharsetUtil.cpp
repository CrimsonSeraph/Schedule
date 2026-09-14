#include "data/import_export/academic_affairs/CharsetUtil.h"

#include "data/import_export/academic_affairs/GbkTable.h"

#include <QRegularExpression>
#include <QStringConverter>
#include <QStringDecoder>

namespace Schedule {

    namespace {

        /** 嗅探声明字符集时扫描的最大字节数。 */
        constexpr int SNIFF_LIMIT = 4096;

        /** GBK 前导字节数量：0x81..0xFE。 */
        constexpr int GBK_LEAD_COUNT = 0xFE - 0x81 + 1;

        /** 每个前导字节对应的尾字节数量：0x40..0x7E 与 0x80..0xFE。 */
        constexpr int GBK_TRAIL_COUNT = 190;

        /** CP936 中单字节 0x80 表示欧元符号。 */
        constexpr ushort CP936_EURO = 0x20AC;

        /** @return 字节串里是否含有非 ASCII 字节（>= 0x80）。 */
        bool contains_non_ascii(const QByteArray& data) {
            for (const char byte : data) {
                if (static_cast<unsigned char>(byte) >= 0x80) {
                    return true;
                }
            }
            return false;
        }

        /** @brief 把尾字节映射为 0..189 的紧凑下标。
         * @return 非法尾字节返回 -1
         */
        int trail_offset(int trail) {
            if (trail >= 0x40 && trail <= 0x7E) {
                return trail - 0x40;
            }
            if (trail >= 0x80 && trail <= 0xFE) {
                return trail - 0x41;
            }
            return -1;
        }

    } // namespace

    // 码表尺寸与下标公式必须严格一致，否则会读到表外内存
    static_assert(GBK_LEAD_COUNT * GBK_TRAIL_COUNT == GBK_TABLE_SIZE,
        "GbkTable 尺寸与 CharsetUtil 的下标公式不一致，请重新运行 tools/gen_gbk_table.py");

    QString gbk_to_unicode(const QByteArray& data) {
        QString result;
        result.reserve(data.size());

        const int size = static_cast<int>(data.size());
        int position = 0;
        while (position < size) {
            const int lead = static_cast<unsigned char>(data.at(position));

            // ASCII：直接映射
            if (lead < 0x80) {
                result.append(QChar(lead));
                ++position;
                continue;
            }

            // CP936 把单字节 0x80 定义为欧元符号；0xFF 未定义
            if (lead == 0x80) {
                result.append(QChar(CP936_EURO));
                ++position;
                continue;
            }
            if (lead == 0xFF) {
                result.append(QChar::ReplacementCharacter);
                ++position;
                continue;
            }

            // 落单的前导字节
            if (position + 1 >= size) {
                result.append(QChar::ReplacementCharacter);
                ++position;
                continue;
            }

            const int trail = static_cast<unsigned char>(data.at(position + 1));
            const int offset = trail_offset(trail);
            if (offset < 0) {
                // 尾字节非法：只吃掉前导字节，让后续字节重新参与判定
                result.append(QChar::ReplacementCharacter);
                ++position;
                continue;
            }

            const int index = (lead - 0x81) * GBK_TRAIL_COUNT + offset;
            const ushort code = (index >= 0 && index < GBK_TABLE_SIZE) ? GBK_TO_UNICODE[index] : 0;
            result.append(code != 0 ? QChar(code) : QChar::ReplacementCharacter);
            position += 2;
        }

        return result;
    }

    bool looks_like_utf8(const QByteArray& data) {
        QStringDecoder decoder(QStringConverter::Utf8);
        // decode() 会消费整个输入；hasError() 在遇到非法序列后保持为真
        const QString decoded = decoder.decode(data);
        Q_UNUSED(decoded);
        return !decoder.hasError();
    }

    QString sniff_declared_charset(const QByteArray& data) {
        const QByteArray head = data.left(SNIFF_LIMIT);
        // 在字节层面匹配，避免先解码再嗅探造成的循环依赖
        static const QRegularExpression pattern(
            QStringLiteral(R"(charset\s*=\s*["']?\s*([A-Za-z0-9_\-]+))"),
            QRegularExpression::CaseInsensitiveOption);

        const QRegularExpressionMatch match = pattern.match(QString::fromLatin1(head));
        if (!match.hasMatch()) {
            return QString();
        }
        return match.captured(1).trimmed().toLower();
    }

    QString decode_html_bytes(const QByteArray& data, QString* charset_used) {
        const auto report = [charset_used](const QString& name) {
            if (charset_used) {
                *charset_used = name;
            }
        };

        if (data.isEmpty()) {
            report(QStringLiteral("utf-8"));
            return QString();
        }

        const QString declared = sniff_declared_charset(data);
        const bool declares_gbk = declared.startsWith(QStringLiteral("gb"));
        const bool declares_utf = declared.startsWith(QStringLiteral("utf"));

        if (declares_gbk) {
            // 声明不可全信的另一面：内嵌浏览器抓到的页面是 JS 字符串转成的 **UTF-8**
            // 字节（见 ImportExportBridge::submit_web_capture），而 outerHTML 里保留的
            // `<meta charset="gb2312">` 还是原来那个。此时照 GBK 解就会整页乱码，
            // 连格式嗅探都认不出来。纯 ASCII 两种解法的结果相同，只有确实含非 ASCII
            // 字节时才需要区分：合法 UTF-8 优先。
            if (looks_like_utf8(data) && contains_non_ascii(data)) {
                report(QStringLiteral("utf-8"));
                return QString::fromUtf8(data);
            }
            report(QStringLiteral("gbk"));
            return gbk_to_unicode(data);
        }

        if (declares_utf) {
            // 声明不可全信：字节非法时说明对方实际输出的是 GBK
            if (looks_like_utf8(data)) {
                report(QStringLiteral("utf-8"));
                return QString::fromUtf8(data);
            }
            report(QStringLiteral("gbk"));
            return gbk_to_unicode(data);
        }

        if (looks_like_utf8(data)) {
            report(QStringLiteral("utf-8"));
            return QString::fromUtf8(data);
        }
        report(QStringLiteral("gbk"));
        return gbk_to_unicode(data);
    }

} // namespace Schedule
