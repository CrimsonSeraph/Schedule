#include "core/model/Conflict.h"

namespace Schedule {

    QString Conflict::type_name() const {
        switch (type) {
        case Type::TimeOverlap:
            return QStringLiteral("时间冲突");
        case Type::SelfOverlap:
            return QStringLiteral("课程内部冲突");
        case Type::DuplicateCourse:
            return QStringLiteral("重复课程");
        case Type::InvalidWeek:
            return QStringLiteral("周次非法");
        case Type::OutOfRangeSlot:
            return QStringLiteral("节次越界");
        case Type::MissingSession:
            return QStringLiteral("缺少上课时间");
        case Type::InvalidSession:
            return QStringLiteral("上课时间非法");
        }
        return QStringLiteral("未知问题");
    }

    bool Conflict::is_blocking() const {
        switch (type) {
        case Type::DuplicateCourse:
            // 重修 / 同名不同代码的情况真实存在，因此只提示不阻断。
            return false;
        default:
            return true;
        }
    }

    QString Conflict::to_display_string() const {
        if (message.isEmpty()) {
            return QStringLiteral("[%1] 未提供描述").arg(type_name());
        }
        return QStringLiteral("[%1] %2").arg(type_name(), message);
    }

} // namespace Schedule
