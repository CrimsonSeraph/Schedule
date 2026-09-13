#!/usr/bin/env python3
"""生成 GBK → Unicode 映射表（C++ 源文件）。

Qt 6 的 QStringConverter 只支持 UTF / Latin-1 / System，**不包含 GBK**；
Qt6Core5Compat（提供 QTextCodec）也不在依赖清单里。而正方教务系统导出的课表
一律是 GBK（`<meta charset="GBK">`），课程名、教师名、教室名都是 GBK 汉字，
因此必须在仓库内自带一张码表。

本脚本用 Python 内置的 gbk 编解码器生成该表，保证内容可复现、可审计：

    python3 tools/gen_gbk_table.py

用法：
    python3 tools/gen_gbk_table.py            # 写入默认输出文件
    python3 tools/gen_gbk_table.py --check     # 只校验已生成的文件是否为最新

码表布局（与 `GbkCodec.cpp` 中的查表逻辑一一对应）：

    index = (lead - 0x81) * 190 + offset(trail)
    offset(trail) = trail - 0x40                    当 0x40 <= trail <= 0x7E
                  = trail - 0x41                    当 0x80 <= trail <= 0xFE

共 0xFE - 0x81 + 1 = 126 个前导字节 × 190 = 23940 个码位。
未定义的码位写 0，运行时映射为 U+FFFD。
"""

import argparse
import os
import sys

LEAD_MIN = 0x81
LEAD_MAX = 0xFE
TRAIL_RANGES = ((0x40, 0x7E), (0x80, 0xFE))
TRAIL_COUNT = sum(hi - lo + 1 for lo, hi in TRAIL_RANGES)  # 190

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_OUTPUT = os.path.join(
    REPO_ROOT, "src", "data", "src", "import_export", "academic_affairs", "GbkTable.cpp"
)


def trail_offset(trail: int) -> int:
    """把 trail 字节映射到 0..189 的紧凑下标。"""
    if 0x40 <= trail <= 0x7E:
        return trail - 0x40
    return trail - 0x41


def iter_trails():
    for lo, hi in TRAIL_RANGES:
        for trail in range(lo, hi + 1):
            yield trail


def build_table():
    """返回 23940 个 Unicode 码位（未定义处为 0，BMP 之外折叠为 0）。"""
    table = []
    for lead in range(LEAD_MIN, LEAD_MAX + 1):
        for trail in iter_trails():
            try:
                char = bytes((lead, trail)).decode("gbk")
            except UnicodeDecodeError:
                table.append(0)
                continue
            code = ord(char)
            # WeekMask / QString 只需要 BMP；GBK 本身不产生 BMP 之外的字符，
            # 这里仍然防御性地折叠，保证生成的表是 ushort。
            table.append(code if code <= 0xFFFF else 0)
    assert len(table) == (LEAD_MAX - LEAD_MIN + 1) * TRAIL_COUNT
    return table


def render(table) -> str:
    lines = [
        "// 本文件由 tools/gen_gbk_table.py 自动生成，请勿手工修改。",
        "// 重新生成：python3 tools/gen_gbk_table.py",
        "//",
        "// GBK -> Unicode（BMP）映射表；0 表示该码位未定义，运行时映射为 U+FFFD。",
        "// 下标公式见 GbkCodec.cpp：index = (lead - 0x81) * 190 + offset(trail)。",
        "",
        '#include "data/import_export/academic_affairs/GbkTable.h"',
        "",
        "namespace Schedule {",
        "",
        "    // clang-format off",
        "    const unsigned short GBK_TO_UNICODE[GBK_TABLE_SIZE] = {",
    ]

    per_line = 8
    for start in range(0, len(table), per_line):
        chunk = table[start : start + per_line]
        body = " ".join("0x%04X," % code for code in chunk)
        lines.append("        " + body)

    lines += [
        "    };",
        "    // clang-format on",
        "",
        "} // namespace Schedule",
        "",
    ]
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description="生成 GBK → Unicode 码表")
    parser.add_argument("--check", action="store_true", help="只校验已生成文件是否为最新")
    parser.add_argument("-o", "--output", default=DEFAULT_OUTPUT, help="输出文件路径")
    args = parser.parse_args()

    content = render(build_table())

    if args.check:
        if not os.path.exists(args.output):
            print("缺失：%s" % args.output, file=sys.stderr)
            return 1
        with open(args.output, "r", encoding="utf-8", newline="") as handle:
            existing = handle.read()
        if existing != content:
            print("已过期：%s（请重新运行 tools/gen_gbk_table.py）" % args.output, file=sys.stderr)
            return 1
        print("已是最新：%s" % args.output)
        return 0

    os.makedirs(os.path.dirname(args.output), exist_ok=True)
    # 统一 LF，符合仓库的 .gitattributes 约定
    with open(args.output, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(content)
    print("已写入 %s（%d 个码位）" % (args.output, 126 * TRAIL_COUNT))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
