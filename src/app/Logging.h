#pragma once

#include <QString>

namespace Schedule::Logging {

/**
 * @brief 进程级日志级别。
 *
 * 对应 Qt 的 Debug / Info / Warning / Critical 四档；低于当前级别的消息不落盘。
 */
enum class Level {
    Debug = 0,
    Info = 1,
    Warning = 2,
    Critical = 3,
};

/// @brief 安装全局 Qt 消息处理器，并把日志写入 directory 下按天切分的文件。
void install(const QString& directory);

/// @brief 卸载消息处理器并关闭文件。
void shutdown();

/// @brief 设置落盘级别；默认 Debug（诊断期尽量全量记录）。
void set_level(Level level);

/// @return 当前落盘级别。
Level level();

/// @return 日志目录（未安装时为空）。
QString log_directory();

/// @brief 把日志目录下全部日志按时间顺序合并导出到 target_directory。
/// @param target_directory 目标目录（不存在则创建）。
/// @param error_message 可选错误输出。
/// @return 导出文件路径；失败返回空串。
QString export_logs(const QString& target_directory, QString* error_message = nullptr);

} // namespace Schedule::Logging
