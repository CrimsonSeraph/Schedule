#pragma once

#include "core/DataEngine.h"

#include <QObject>
#include <QString>

namespace Schedule {

    /**
     * @brief C++ <-> QML 桥接对象。
     *
     * 由应用入口（src/app/main.cpp）在 C++ 侧实例化，并通过上下文属性注入 QML，
     * 供 QML 界面读取 version 等属性。
     *
     * 与 QML 之间的信号连接统一在 C++ 侧显式建立（QObject::connect），
     * QML 中不使用 onClicked / Connections 等按名称隐式连接的写法。
     */
    class AppBridge : public QObject {
        Q_OBJECT
        Q_PROPERTY(QString version READ version CONSTANT)

    public:
        explicit AppBridge(QObject* parent = nullptr);

        /** @return 当前应用版本号（取自 DataEngine，运行期不变）。 */
        QString version() const;

    public slots:
        /**
         * @brief “测试”按钮点击处理槽。
         *
         * 由 C++ 侧将 QML Button::clicked 信号显式连接到本槽（见 src/app/main.cpp），
         * 触发后输出调试日志并发出 test_signal 信号。
         */
        void test_button_clicked();

    signals:
        /** 测试按钮被点击后发出，message 携带触发源标识。 */
        void test_signal(const QString& message);

    private:
        DataEngine m_data_engine;
    };

} // namespace Schedule
