#pragma once

#include <QByteArray>
#include <QString>

namespace Schedule {

    /**
     * @brief 字符集嗅探与解码工具。
     *
     * 为什么需要它：Qt 6 的 `QStringConverter` 只覆盖 UTF / Latin-1 / System，
     * **不含 GBK**；而正方教务系统（zfn / zfsoft）导出的课表页面一律声明
     * `<meta charset="GBK">`，课程名、教师名、教室名都是 GBK 汉字。
     * 仓库内自带一张由 `tools/gen_gbk_table.py` 生成的码表（见 `GbkTable.h`），
     * 本文件负责在其上提供解码入口。
     *
     * 支持范围：ASCII + GBK 双字节区（覆盖 GB2312 / GBK 常用汉字）。
     * GB18030 的四字节扩展区不在支持范围内，遇到时按未定义码位处理。
     */

    /**
     * @brief 把 GBK / GB2312 字节串解码为 Unicode 文本。
     *
     * - ASCII 字节原样映射；
     * - 双字节序列查 `GBK_TO_UNICODE`，未定义码位映射为 `U+FFFD`；
     * - 落单的前导字节、非法尾字节同样映射为 `U+FFFD`，**不抛错、不中断**。
     *
     * @param data 原始字节串
     * @return 解码结果；输入为空时返回空串
     */
    QString gbk_to_unicode(const QByteArray& data);

    /**
     * @brief 判断字节串是否为**合法** UTF-8（纯 ASCII 视为合法）。
     *
     * 用于在没有 charset 声明时二选一：合法即按 UTF-8 解码，否则退回 GBK。
     *
     * @param data 原始字节串
     * @return 是否合法
     */
    bool looks_like_utf8(const QByteArray& data);

    /**
     * @brief 从 HTML 头部嗅探声明的字符集。
     *
     * 同时识别 `<meta charset="GBK">` 与
     * `<meta http-equiv="Content-Type" content="text/html; charset=GBK">` 两种写法。
     * 只扫描前 4 KiB，避免在大文件上做无谓的全量正则匹配。
     *
     * @param data 原始字节串
     * @return 小写的字符集名（如 `gbk` / `utf-8`）；未声明返回空串
     */
    QString sniff_declared_charset(const QByteArray& data);

    /**
     * @brief 按“声明优先、内容兜底”的策略解码 HTML 字节串。
     *
     * 判定顺序：
     *  1. 声明是 UTF 系列：先校验字节是否真为合法 UTF-8，不合法则退回 GBK
     *     （部分教务系统声称 UTF-8 实际输出 GBK）；
     *  2. 声明是 GB 系列（`gbk` / `gb2312` / `gb18030`）：按 GBK 解码；
     *  3. 无声明：合法 UTF-8 按 UTF-8，否则按 GBK。
     *
     * @param data         原始字节串
     * @param charset_used 可选输出，实际采用的字符集名（小写）
     * @return 解码后的文本
     */
    QString decode_html_bytes(const QByteArray& data, QString* charset_used = nullptr);

} // namespace Schedule
