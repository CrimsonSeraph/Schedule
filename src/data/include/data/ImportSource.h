#pragma once

#include <QDateTime>
#include <QString>

namespace Schedule {

    /**
     * @brief 一次课表导入的来源记录。
     *
     * 仅在**本地**留痕：记录“从哪个文件、哪种格式、什么时候、导入了多少门课”，
     * 便于用户在设置页回看历史，以及重复导入时提示“该文件已导入过”。
     *
     * 明确不记录任何账号、密码、Token 或远端地址——本项目不做账号系统。
     */
    struct ImportSource {
        /** 记录唯一标识（UUID，无花括号）。 */
        QString id;

        /** 导入文件路径（本地绝对路径）。 */
        QString file_path;

        /** 文件格式标识：`json` / `csv` / `ics` / `adapter`。 */
        QString format;

        /** 导入发生时间（本地时间）。 */
        QDateTime imported_at;

        /** 本次导入新增的课程数量。 */
        int course_count = 0;

        /** 备注，例如“合并导入”“覆盖导入”或教务适配器名称。 */
        QString note;

        bool operator==(const ImportSource& other) const;
        bool operator!=(const ImportSource& other) const;
    };

} // namespace Schedule
