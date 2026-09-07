#pragma once

#include <QObject>
#include <QString>

#include "core/DataEngine.h"

namespace Schedule {

    /**
     * @brief C++ <-> QML 桥接对象。
     *
     * 通过 qmlRegisterType 注册为 QML 类型（URI: Schedule，QML 名: AppBridge），
     * 供 QML 界面读取 version 属性并调用测试槽函数。
     */
    class AppBridge : public QObject {
        Q_OBJECT
        Q_PROPERTY(QString version READ version NOTIFY versionChanged)

    public:
        explicit AppBridge(QObject* parent = nullptr);

        /** @return 当前应用版本号（取自 DataEngine）。 */
        QString version() const;

        /**
         * @brief 测试槽函数。
         *
         * 跨平台一致性考虑使用 qDebug 输出，而非平台相关的 QMessageBox。
         * 可在 QML 中通过 onClicked: appBridge.testButtonClicked() 调用。
         */
        Q_INVOKABLE void testButtonClicked();

    signals:
        /** version 属性变化时发出。 */
        void versionChanged();

        /** 供 QML 监听测试信号（可选）。 */
        void testSignal(const QString& msg);

    private:
        DataEngine m_dataEngine;
    };

} // namespace Schedule
