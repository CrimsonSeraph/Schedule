#!/usr/bin/env python3
"""生成**虚构的**华东交通大学教务综合管理系统课表样本。

用途：为 `tst_ecjtu_timetable.cpp` 提供回归样本。

隐私说明：本脚本生成的全部内容（姓名、课程、教师、教室）均为虚构演示数据，
**不包含任何真实教务数据**，也不从任何真实文件中复制内容。它只复刻该教务系统
「导出」结果的**结构**：

- `.doc`：Word 另存为网页的产物（GBK 编码、`urn:schemas-microsoft-com:office:word`
  命名空间、`MsoNormalTable` 表格）；
- `.docx`：同一个表格的 OOXML 形态（Word / LibreOffice 转出来的就是它）。

两份样本刻意保持**内容等价**，因此可以用同一个解析器断言两条路径结果一致。

表格约定（与真实导出页一致）：

- 表头 `节次 | 星期一 … 星期日`，表头之上还有一行 `colspan=8` 的标题；
- 左侧节次列形如 `1-2节`、`3-4节`；
- 单元格内每门课占三行：`课程名` / `教师 @教室` / `周次 节次`；
- 第三行的第二个字段是**该课程实际占用的节次列表**，一个活动会出现在它跨越的
  每个节次行里（`1,2,3` 就同时出现在 `1-2节` 与 `3-4节` 行）——这正是 OOXML 侧
  用 `vMerge` 表示的东西。

    python3 tools/gen_ecjtu_sample.py

输出：
    samples/schedule_sample_ecjtu.doc   导出得到的 Word 版式 HTML（GBK）
    samples/schedule_sample_ecjtu.docx  同一个表格的 OOXML 形态
    samples/schedule_sample_ecjtu.html  内嵌浏览器抓取到的页面（UTF-8 字节，但页面声明 gb2312）
"""

import os
import zipfile

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SAMPLES_DIR = os.path.join(REPO_ROOT, "samples")
DOC_OUTPUT = os.path.join(SAMPLES_DIR, "schedule_sample_ecjtu.doc")
DOCX_OUTPUT = os.path.join(SAMPLES_DIR, "schedule_sample_ecjtu.docx")
HTML_OUTPUT = os.path.join(SAMPLES_DIR, "schedule_sample_ecjtu.html")

SEMESTER = "2024-2025 第一学期"
STUDENT = "示例同学"
PRINT_DATE = "2024-09-02"

# `<title>` 刻意写成**模板残留**，与真实导出文件一致（真实样本的标题停在
# 2015-2016 学年，正文却已经是 2026-2027）。解析器必须以正文为准，否则同一份内容
# 转成 .docx（标题被挪进 docProps）后，两条路径会得出不同的学期。
STALE_TITLE = "2015-2016学年第一学期 {Name} 课表"

DAY_NAMES = ["星期一", "星期二", "星期三", "星期四", "星期五", "星期六", "星期日"]

# 节次行：显示标签 + 该行覆盖的节次（从 1 开始）
SLOT_ROWS = [
    ("1-2节", [1, 2]),
    ("3-4节", [3, 4]),
    ("5-6节", [5, 6]),
    ("7-8节", [7, 8]),
    ("9-10节", [9, 10]),
    ("11-12节", [11, 12]),
]

# 全部为虚构数据。每条 = (课程名, 教师, 教室, 周次文本, 节次文本)
# `slots` 必须与「节次文本」一致：解析器以节次文本为准，行号只决定它出现在哪几行。
ENTRIES = {
    ("1-2节", "星期一"): [
        ("高等数学(演示)", "张演示", "31-101", "1-8", "1,2"),
        ("程序设计基础(演示)", "李演示", "31-102", "10-16", "1,2"),
    ],
    ("3-4节", "星期一"): [
        ("高等数学(演示)", "张演示", "31-101", "1-8", "3,4"),
    ],
    ("1-2节", "星期三"): [
        ("大学英语(演示)", "王演示", "31-103", "1-8", "1,2"),
    ],
    ("5-6节", "星期二"): [
        ("大学物理(演示)", "陈演示", "31-106", "1-8(单)", "5,6"),
    ],
    ("7-8节", "星期四"): [
        ("数据结构(演示)", "周演示", "31-107", "1-8,10-16", "7,8"),
    ],
    # 跨 1-2 / 3-4 两行的活动：真实导出会在这两行重复同一段文本，
    # 而 OOXML 形态用 vMerge 把星期五这一格纵向合并起来。
    ("1-2节", "星期五"): [
        ("数据库(演示)", "赵演示", "31-105", "9-16", "1,2,3"),
    ],
}

# 纵向合并的单元格：列名 -> 跨越的行标签（起始行写内容，其余行留空）
VMERGE_COLUMN = "星期五"
VMERGE_ROWS = ["1-2节", "3-4节"]

# 表头之上的一行标题：整行一个单元格（`colspan=8` / `gridSpan=8`）
TITLE_ROW = "%s %s 课表" % (SEMESTER, STUDENT)


def cell_entries(row_label, day_name):
    """@return 该格子的课程条目；被纵向合并覆盖的格子返回 None。"""
    if day_name == VMERGE_COLUMN and row_label in VMERGE_ROWS[1:]:
        return None  # 合并的后续格子：内容在起始行，这里留空
    return ENTRIES.get((row_label, day_name), [])


def entry_text(entry):
    """@return 一条课程条目的三行文本。"""
    name, teacher, room, weeks, units = entry
    return [name, "%s @%s" % (teacher, room), "%s %s" % (weeks, units)]


def row_entries(row_label, day_name):
    """@return 该格子的全部行文本（含合并起始格）。"""
    entries = cell_entries(row_label, day_name)
    if entries is None:
        return None  # 纵向合并的后续格
    lines = []
    for entry in entries:
        lines.extend(entry_text(entry))
    return lines


# ----------------------------------------------------------------- 纯文本形态


def plain_table():
    """@return 二维文本表格：第一行是标题，第二行是表头，其余是节次行。"""
    rows = [[TITLE_ROW], ["节次"] + DAY_NAMES]
    for row_label, _slots in SLOT_ROWS:
        row = [row_label]
        for day_name in DAY_NAMES:
            row.append(row_entries(row_label, day_name))
        rows.append(row)
    return rows


# ------------------------------------------------------------------- Word HTML


def html_cell(lines, is_merged_continue):
    """@return 一个 `<td>`；`lines` 为 None 表示该格被上一行的 rowspan 覆盖。"""
    if is_merged_continue:
        # rowspan 的后续行不写这个格子
        return None
    if lines is None:
        return "<td></td>"
    if not lines:
        return "<td><p class=MsoNormal><span>&nbsp;</span></p></td>"
    body = "<br />".join(lines)
    return "<td><p class=MsoNormal><span>%s<br /></span></p></td>" % body


def build_html():
    """@return GBK 编码前 Word 版式 HTML 文本。"""
    parts = []
    parts.append(
        "<html xmlns:v=\"urn:schemas-microsoft-com:vml\"\r\n"
        "xmlns:o=\"urn:schemas-microsoft-com:office:office\"\r\n"
        "xmlns:w=\"urn:schemas-microsoft-com:office:word\"\r\n"
        "xmlns=\"http://www.w3.org/TR/REC-html40\">\r\n"
        "<head>\r\n"
        "<meta http-equiv=Content-Type content=\"text/html; charset=gb2312\">\r\n"
        "<meta name=ProgId content=Word.Document>\r\n"
        "<meta name=Generator content=\"Microsoft Word 11\">\r\n"
        "<title>%s</title>\r\n"
        "<style><!-- p.MsoNormal {margin:0cm;} --></style>\r\n"
        "</head>\r\n<body lang=ZH-CN>\r\n"
        "<p class=MsoNormal><span>%s</span></p>\r\n"
        "<p class=MsoNormal><span>%s</span></p>\r\n"
        "<p class=MsoNormal><span>课表</span></p>\r\n"
        "<p class=MsoNormal><span>打印日期：</span><span>%s</span></p>\r\n"
        % (STALE_TITLE, SEMESTER, STUDENT, PRINT_DATE)
    )
    parts.append(
        "<table class=MsoNormalTable border=1 cellspacing=0 cellpadding=0 width=948>\r\n"
    )

    # 标题行：整行一个格子
    parts.append(
        "<tr><td colspan=8><p class=MsoNormal align=center><span>%s</span></p></td></tr>\r\n"
        % TITLE_ROW
    )

    # 表头
    header = "".join(
        "<td><p class=MsoNormal align=center><span>%s</span></p></td>" % name
        for name in ["节次"] + DAY_NAMES
    )
    parts.append("<tr>%s</tr>\r\n" % header)

    # 节次行：第一格是左侧节次列，之后是星期一~星期日
    for row_label, _slots in SLOT_ROWS:
        cells = ["<td><p class=MsoNormal align=center><span>%s</span></p></td>" % row_label]
        for day_name in DAY_NAMES:
            merged_continue = day_name == VMERGE_COLUMN and row_label in VMERGE_ROWS[1:]
            if merged_continue:
                continue  # rowspan 覆盖，不输出这个 td
            lines = row_entries(row_label, day_name)
            if day_name == VMERGE_COLUMN and row_label == VMERGE_ROWS[0]:
                span = len(VMERGE_ROWS)
                cells.append(
                    "<td rowspan=%d><p class=MsoNormal align=center><span>%s<br /></span></p></td>"
                    % (span, "<br />".join(lines))
                )
            else:
                cells.append(html_cell(lines, False))
        parts.append("<tr>%s</tr>\r\n" % "".join(cells))

    parts.append("</table>\r\n<br />\r\n</body>\r\n</html>\r\n")
    return "".join(parts)


# ----------------------------------------------------------------------- OOXML


def xml_escape(text):
    return text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def xml_cell(lines, grid_span=1, vmerge=None):
    """@return 一个 `<w:tc>`；`lines` 为 None 表示空的合并延续格。"""
    properties = ""
    if grid_span > 1:
        properties += "<w:gridSpan w:val=\"%d\"/>" % grid_span
    if vmerge == "restart":
        properties += "<w:vMerge w:val=\"restart\"/>"
    elif vmerge == "continue":
        properties += "<w:vMerge/>"

    tc_pr = "<w:tcPr>%s</w:tcPr>" % properties if properties else ""

    if not lines:
        return "<w:tc>%s<w:p/></w:tc>" % tc_pr

    runs = []
    for index, line in enumerate(lines):
        if index > 0:
            runs.append("<w:r><w:br/></w:r>")
        runs.append("<w:r><w:t xml:space=\"preserve\">%s</w:t></w:r>" % xml_escape(line))
    return "<w:tc>%s<w:p>%s</w:p></w:tc>" % (tc_pr, "".join(runs))


def build_document_xml():
    """@return `word/document.xml` 的内容。"""
    paragraphs = "".join(
        "<w:p><w:r><w:t>%s</w:t></w:r></w:p>" % xml_escape(line)
        for line in [SEMESTER, STUDENT, "课表", "打印日期：" + PRINT_DATE]
    )

    rows = ["<w:tr>%s</w:tr>" % xml_cell([TITLE_ROW], grid_span=len(DAY_NAMES) + 1)]
    rows.append(
        "<w:tr>%s</w:tr>"
        % "".join(xml_cell([name]) for name in ["节次"] + DAY_NAMES)
    )

    for row_label, _slots in SLOT_ROWS:
        # 第一格是左侧节次列，之后是星期一~星期日
        cells = [xml_cell([row_label])]
        for day_name in DAY_NAMES:
            if day_name == VMERGE_COLUMN and row_label in VMERGE_ROWS[1:]:
                cells.append(xml_cell(None, vmerge="continue"))
                continue
            lines = row_entries(row_label, day_name)
            if day_name == VMERGE_COLUMN and row_label == VMERGE_ROWS[0]:
                cells.append(xml_cell(lines, vmerge="restart"))
            else:
                cells.append(xml_cell(lines))
        rows.append("<w:tr>%s</w:tr>" % "".join(cells))

    return (
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\r\n"
        "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
        "<w:body>%s<w:tbl>%s</w:tbl><w:sectPr/></w:body></w:document>"
        % (paragraphs, "".join(rows))
    )


CONTENT_TYPES = (
    "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\r\n"
    "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
    "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
    "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
    "<Override PartName=\"/word/document.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml\"/>"
    "</Types>"
)

ROOT_RELS = (
    "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\r\n"
    "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
    "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"word/document.xml\"/>"
    "</Relationships>"
)

DOCUMENT_RELS = (
    "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\r\n"
    "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\"/>"
)


def build_docx():
    """@return `.docx` 的 zip 字节。"""
    import io

    buffer = io.BytesIO()
    with zipfile.ZipFile(buffer, "w", zipfile.ZIP_DEFLATED) as archive:
        for name, content in [
            ("[Content_Types].xml", CONTENT_TYPES),
            ("_rels/.rels", ROOT_RELS),
            ("word/document.xml", build_document_xml()),
            ("word/_rels/document.xml.rels", DOCUMENT_RELS),
        ]:
            # 固定时间戳：zip 条目默认写当前时间，同样的内容每次生成都会得到不同的字节，
            # 样本就没法逐字节复现（回归 diff 里会混进无意义的二进制变更）
            info = zipfile.ZipInfo(name, date_time=(2024, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(info, content)
    return buffer.getvalue()


def build_captured_page():
    """@return 内嵌浏览器抓到的课表页（UTF-8 字节，但页面自己声明 gb2312）。

    这是**刻意的**：`ImportExportBridge::submit_web_capture()` 把页面的
    `document.documentElement.outerHTML` 按 `toUtf8()` 交给解析器，而 outerHTML 里
    保留了原始页面的 `<meta charset="gb2312">`。真实抓取结果就是这个样子，
    解析器必须以字节为准、不能盲信声明。

    页面里还放了一张**排在课表之前**的布局表格，用来验证解析器挑的是课表那张表。
    """
    page = (
        "<!DOCTYPE html>\n<html><head>\n"
        '<meta http-equiv="Content-Type" content="text/html; charset=gb2312">\n'
        "<title>教务综合管理系统 - 学生课表</title>\n"
        "<style>body{font-family:sans-serif}</style>\n"
        "</head><body>\n"
        '<div id="header"><table><tr><td>导航</td><td><a href="#">首页</a></td></tr></table></div>\n'
        '<h2>%s %s 课表</h2>\n'
        "<p>打印日期：%s</p>\n"
    ) % (SEMESTER, STUDENT, PRINT_DATE)

    page += "<table border=1>\n"
    page += "<tr>%s</tr>\n" % "".join(
        "<td>%s</td>" % name for name in ["节次"] + DAY_NAMES
    )
    for row_label, _slots in SLOT_ROWS:
        cells = ["<td>%s</td>" % row_label]
        for day_name in DAY_NAMES:
            lines = row_entries(row_label, day_name)
            cells.append("<td>%s</td>" % "<br>".join(lines or []))
        page += "<tr>%s</tr>\n" % "".join(cells)
    page += "</table>\n</body></html>\n"

    # 抓取结果是 UTF-8 字节；页面里的 charset 声明仍是 gb2312
    return page.encode("utf-8")


def main():
    os.makedirs(SAMPLES_DIR, exist_ok=True)

    # 真实导出页声明 gb2312；样本保持同样的编码，才能覆盖解码路径
    with open(DOC_OUTPUT, "wb") as handle:
        handle.write(build_html().encode("gbk"))
    print("已写入 %s（%d 字节，GBK）" % (DOC_OUTPUT, os.path.getsize(DOC_OUTPUT)))

    with open(DOCX_OUTPUT, "wb") as handle:
        handle.write(build_docx())
    print("已写入 %s（%d 字节，OOXML）" % (DOCX_OUTPUT, os.path.getsize(DOCX_OUTPUT)))

    with open(HTML_OUTPUT, "wb") as handle:
        handle.write(build_captured_page())
    print("已写入 %s（%d 字节，UTF-8 但声明 gb2312）" % (HTML_OUTPUT, os.path.getsize(HTML_OUTPUT)))


if __name__ == "__main__":
    main()
