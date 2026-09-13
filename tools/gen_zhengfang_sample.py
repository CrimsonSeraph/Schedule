#!/usr/bin/env python3
"""生成**虚构的**正方教务课表样本（GBK 编码的 HTML + JS）。

用途：为 `tst_zhengfang_timetable.cpp` 提供回归样本。

隐私说明：本脚本生成的全部内容（学号、姓名、班级、课程、教师、教室、课程代码）
均为虚构演示数据，**不包含任何真实教务数据**，也不从任何真实文件中复制内容。
结构上刻意与教务系统导出的页面保持一致（`<table id="manualArrangeCourseTable">`
外壳 + 内嵌 `TaskActivity` 脚本），以便覆盖解析器的真实输入形态。

    python3 tools/gen_zhengfang_sample.py

输出：samples/schedule_sample_zhengfang.xls
"""

import os

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUTPUT = os.path.join(REPO_ROOT, "samples", "schedule_sample_zhengfang.xls")

UNIT_COUNT = 12
TOTAL_CELLS = UNIT_COUNT * 7
SEMESTER = "2024-2025学年第一学期"
DAY_NAMES = ["星期一", "星期二", "星期三", "星期四", "星期五", "星期六", "星期日"]

# 全部为虚构数据。cells 中的 (day, unit) 均从 0 开始：
# day 0=周一，unit 0=第 1 节。
COURSES = [
    {
        # 单一活动跨连续两节：应合并为“周一 1-2 节”一段
        "code": "00000001.01",
        "name": "高等数学A(1)",
        "teachers": ["张演示"],
        "activities": [
            {"room_code": "101", "room": "1J101(主校区)", "weeks": list(range(1, 17)),
             "cells": [(0, 0), (0, 1)]},
        ],
    },
    {
        # 同一门课的两个活动（不同教室、不同周次）：应合并为同一门课程的两段时间段
        "code": "00000002.02",
        "name": "大学英语(1)",
        "teachers": ["王演示", "李演示"],
        "activities": [
            {"room_code": "202", "room": "2J202(东校区)", "weeks": [1, 2, 3, 4, 5, 6, 7, 8, 10, 11, 12],
             "cells": [(1, 2), (1, 3)]},
            {"room_code": "203", "room": "语言实验室-2J203(东校区)", "weeks": [2, 4, 6, 8, 10, 12],
             "cells": [(3, 2), (3, 3)]},
        ],
    },
    {
        # 跨四节的连续活动：应合并为一段 slot_count=4
        "code": "00000003.03",
        "name": "程序设计基础",
        "teachers": ["赵演示"],
        "activities": [
            {"room_code": "303", "room": "3J303(主校区)", "weeks": list(range(1, 16)),
             "cells": [(2, 5), (2, 6), (2, 7), (2, 8)]},
        ],
    },
    {
        "code": "00000004.04",
        "name": "程序设计基础实验",
        "teachers": ["赵演示"],
        "activities": [
            {"room_code": "304", "room": "计算机实验室-3J304(主校区)", "weeks": list(range(4, 16)),
             "cells": [(4, 5), (4, 6)]},
        ],
    },
    {
        "code": "00000005.05",
        "name": "体育(1)",
        "teachers": ["孙演示"],
        "activities": [
            {"room_code": "0", "room": "第一运动场(主校区)", "weeks": list(range(1, 19)),
             "cells": [(3, 4), (3, 5)]},
        ],
    },
    {
        # 单周上课：验证按周次位图还原出奇数周
        "code": "00000006.06",
        "name": "大学物理(1)",
        "teachers": ["周演示"],
        "activities": [
            {"room_code": "404", "room": "4J404(东校区)", "weeks": [1, 3, 5, 7, 9, 11],
             "cells": [(1, 0), (1, 1)]},
        ],
    },
    {
        # 同一活动挂在**不连续**的两节上：应拆成两段时间段
        "code": "00000007.07",
        "name": "形势与政策(1)",
        "teachers": ["吴演示"],
        "activities": [
            {"room_code": "505", "room": "5J505(主校区)", "weeks": [2, 4, 6],
             "cells": [(2, 0), (2, 3)]},
        ],
    },
]


def week_bitmap(weeks):
    """把周次列表转成页面使用的位图：下标 i 为 1 表示第 i 周有课。"""
    length = 53
    bits = ["0"] * length
    for week in weeks:
        if 1 <= week < length:
            bits[week] = "1"
    return "".join(bits)


def build_activities_script():
    """生成与真实导出页等价的 <script> 片段。"""
    lines = [
        '<script language="JavaScript">',
        "\t// function CourseTable in TaskActivity.js",
        '\tvar language = "zh";',
        "\tvar table0 = new CourseTable(2025,%d);" % TOTAL_CELLS,
        "\tvar unitCount = %d;" % UNIT_COUNT,
        "\tvar index=0;",
        "\tvar activity=null;",
    ]

    for course in COURSES:
        for activity in course["activities"]:
            # 教师数组：真实页面在此处声明，TaskActivity 只引用 join 结果
            teacher_entries = ",".join(
                '{id:%d,name:"%s",lab:false}' % (100 + i, name)
                for i, name in enumerate(activity.get("teachers", course["teachers"]))
            )
            lines += [
                "\t\tvar teachers = [%s];" % teacher_entries,
                "\t\tvar actTeachers = [%s];" % teacher_entries,
                "\t\tvar actTeacherId = [];",
                "\t\tvar actTeacherName = [];",
                "\t\tfor (var i = 0; i < actTeachers.length; i++) {",
                "\t\t\tactTeacherId.push(actTeachers[i].id);",
                "\t\t\tactTeacherName.push(actTeachers[i].name);",
                "\t\t}",
            ]

            code_raw = "9000%s" % course["code"]
            args = ",".join(
                [
                    "actTeacherId.join(',')",
                    "actTeacherName.join(',')",
                    '"%s(%s)"' % (code_raw, course["code"]),
                    '"%s(%s)"' % (course["name"], course["code"]),
                    '"%s"' % activity["room_code"],
                    '"%s"' % activity["room"],
                    '"%s"' % week_bitmap(activity["weeks"]),
                    "null",
                    "null",
                    '""',
                    '""',
                    '""',
                ]
            )
            lines.append("\t\t\tactivity = new TaskActivity(%s);" % args)

            for day, unit in activity["cells"]:
                lines.append("\t\t\tindex =%d*unitCount+%d;" % (day, unit))
                lines.append("\t\t\ttable0.activities[index][table0.activities[index].length]=activity;")

    lines.append("</script>")
    return "\n".join(lines)


def build_course_table_html():
    """生成与脚本同源的 HTML 课表网格（单元格 id 为 TD{星期*unitCount+节次}_0）。"""
    # (day, unit) -> (course, activity)
    occupied = {}
    for course in COURSES:
        for activity in course["activities"]:
            for cell in activity["cells"]:
                occupied[cell] = (course, activity)

    rows = []
    covered = set()
    for unit in range(UNIT_COUNT):
        cells = []
        for day in range(7):
            if (day, unit) in covered:
                continue
            entry = occupied.get((day, unit))
            if entry is None:
                cells.append("          <td></td>")
                continue

            course, activity = entry
            # 竖直方向合并同一活动的连续节次
            span = 1
            while occupied.get((day, unit + span)) == entry:
                covered.add((day, unit + span))
                span += 1

            weeks = activity["weeks"]
            week_spec = " ".join(
                "%d-%d" % (run[0], run[-1]) if len(run) > 1 else str(run[0])
                for run in _runs(weeks)
            )
            title = "(%s,%s" % (week_spec, activity["room"])
            body = "%s(%s) (%s)" % (course["name"], course["code"], ",".join(course["teachers"]))
            span_attr = ' rowspan="%d"' % span if span > 1 else ""
            cells.append(
                '          <td id="TD%d_0"%s class="infoTitle" title="%s">%s</td>'
                % (day * UNIT_COUNT + unit, span_attr, title, body)
            )
        rows.append("      <tr>\n" + "\n".join(cells) + "\n      </tr>")

    header = "\n".join(
        '          <th style="background-color:#DEEDF7;"><font size="2px">%s</font></th>' % name
        for name in DAY_NAMES
    )

    return """    <table width="100%%" id="manualArrangeCourseTable" align="center" class="gridtable" style="text-align:center" border="1">
      <thead>
      <tr>
          <th style="background-color:#DEEDF7;" height="10px" width="80px">节次/周次
          </th>
%s
      </tr>
      </thead>
      <tbody>
%s
      </tbody>
    </table>""" % (
        header,
        "\n".join(rows),
    )


def _runs(weeks):
    """把升序周次列表切成连续区间。"""
    runs = []
    for week in weeks:
        if runs and week == runs[-1][-1] + 1:
            runs[-1].append(week)
        else:
            runs.append([week])
    return runs


def build_document():
    return """<html xmlns:x="urn:schemas-microsoft-com:office:excel">
<head>
<meta http-equiv="Content-Type" content="text/html; charset=GBK" />
<!--[if gte mso 9]><xml>
    <x:ExcelWorkbook>
        <x:ExcelWorksheets>
            <x:ExcelWorksheet>
                <x:Name>个人课程表</x:Name>
            </x:ExcelWorksheet>
        </x:ExcelWorksheets>
    </x:ExcelWorkbook>
</xml>
<![endif]-->
</head>

<body>

<div id="ExportA" width="100%%" align="center" cellpadding="0" cellspacing="0">
    <h1 align="center">个人课程表</h1>
    <h3 align="center">%s</h3>
    <div style="text-align: left;margin-left: 15px;">
        学号:2023000001&nbsp;&nbsp;&nbsp; 学生姓名:示例同学&nbsp;&nbsp;&nbsp;所属班级: 示例2401班&nbsp;&nbsp;&nbsp;总学分:20
    </div>
    <pre>课表格式说明：教师姓名 课程名称(序号) (第n周-第m周,教室)</pre>
%s
</div>

%s

</body>
</html>
""" % (
        SEMESTER,
        build_course_table_html(),
        build_activities_script(),
    )


def main():
    document = build_document()
    os.makedirs(os.path.dirname(OUTPUT), exist_ok=True)
    # 真实导出页是 GBK；样本保持同样的编码，才能覆盖解码路径
    with open(OUTPUT, "wb") as handle:
        handle.write(document.encode("gbk"))
    print("已写入 %s（%d 字节，GBK）" % (OUTPUT, os.path.getsize(OUTPUT)))


if __name__ == "__main__":
    main()
