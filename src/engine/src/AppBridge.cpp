#include "engine/AppBridge.h"

#include <QDebug>

namespace Schedule {

    AppBridge::AppBridge(QObject* parent)
        : QObject(parent) {
    }

    QString AppBridge::version() const {
        return m_dataEngine.getVersion();
    }

    void AppBridge::testButtonClicked() {
        qDebug() << "Test button clicked!";
        emit testSignal(QStringLiteral("testButtonClicked"));
    }

} // namespace Schedule
