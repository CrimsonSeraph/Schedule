#include "engine/AppBridge.h"

#include <QDebug>

namespace Schedule {

    AppBridge::AppBridge(QObject* parent)
        : QObject(parent) {
    }

    QString AppBridge::version() const {
        return m_data_engine.get_version();
    }

    void AppBridge::test_button_clicked() {
        qDebug() << "Test button clicked!";
        emit test_signal(QStringLiteral("test_button_clicked"));
    }

} // namespace Schedule
