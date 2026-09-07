#pragma once

#include <QString>

namespace myapp {

    /**
     * @brief 核心数据引擎。
     *
     * 纯 C++ 逻辑层，仅依赖 Qt6::Core，不含任何 GUI 相关代码，
     * 可被 engine / ui / app 各层安全引用。
     */
    class DataEngine {
    public:
        DataEngine() = default;

        /** @return 当前应用版本号，如 "1.0.0"。 */
        QString getVersion() const;
    };

} // namespace myapp
