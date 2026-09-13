#pragma once

namespace Schedule {

    /**
     * @brief `GBK_TO_UNICODE` 的元素个数：(0xFE - 0x81 + 1) × 190 = 23940。
     *
     * 数组本体在 `GbkTable.cpp` 中由 `tools/gen_gbk_table.py` 生成，
     * 单独拆成头文件是为了让 `GbkCodec.cpp` 能做静态断言而无需重复字面量。
     */
    inline constexpr int GBK_TABLE_SIZE = 23940;

    /** GBK → Unicode（BMP）映射表；0 表示该码位未定义。 */
    extern const unsigned short GBK_TO_UNICODE[GBK_TABLE_SIZE];

} // namespace Schedule
