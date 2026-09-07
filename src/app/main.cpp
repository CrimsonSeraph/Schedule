#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QUrl>
#include <QtQml/qqml.h>

#include "engine/AppBridge.h"

#if MYAPP_SELFTEST
#include <QQuickItem>
#include <QQuickWindow>
#include <QTimer>
#include <QtTest/QTest>
#endif

#if MYAPP_SELFTEST
namespace {

/**
 * 自动化 UI 自检：加载完成后向“测试”按钮发送一次鼠标左键点击，
 * 验证 QML onClicked -> AppBridge::testButtonClicked() 链路，
 * 并以退出码 0/2 表示成功/失败。
 */
void scheduleUiSelfTest(QQmlApplicationEngine &engine, QGuiApplication &app)
{
    QTimer::singleShot(1500, &app, [&engine, &app]() {
        QObject *rootObject = engine.rootObjects().value(0);
        auto *window = qobject_cast<QQuickWindow *>(rootObject);
        if (!window) {
            qWarning() << "[selftest] root object is not a QQuickWindow";
            app.exit(2);
            return;
        }

        QQuickItem *testButton = window->findChild<QQuickItem *>(QStringLiteral("testButton"));
        if (!testButton) {
            qWarning() << "[selftest] testButton not found in QML scene";
            app.exit(2);
            return;
        }

        const QPointF center = testButton->mapToScene(
            QPointF(testButton->width() / 2.0, testButton->height() / 2.0));
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center.toPoint());

        // 给 qDebug / 事件循环留出输出时间后正常退出
        QTimer::singleShot(800, &app, [&app]() { app.exit(0); });
    });
}

} // namespace
#endif

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("Schedule"));
    app.setApplicationName(QStringLiteral("Schedule"));
    app.setApplicationVersion(QStringLiteral("1.0.0"));

    // 注册 AppBridge 到 QML（模块 MyApp 1.0）
    qmlRegisterType<myapp::AppBridge>("MyApp", 1, 0, "AppBridge");

    QQmlApplicationEngine engine;

    // 平台分流：移动端加载手机布局，其余平台加载桌面布局
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
    const QUrl mainQml(QStringLiteral("qrc:/qt/qml/MyApp/qml/MainMobile.qml"));
#else
    const QUrl mainQml(QStringLiteral("qrc:/qt/qml/MyApp/qml/MainDesktop.qml"));
#endif

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        [mainQml](const QUrl &objectUrl) {
            qWarning() << "Failed to load QML:" << objectUrl << "(" << mainQml << ")";
        },
        Qt::QueuedConnection);

    engine.load(mainQml);

    if (engine.rootObjects().isEmpty()) {
        return -1;
    }

#if MYAPP_SELFTEST
    if (app.arguments().contains(QStringLiteral("--selftest"))) {
        scheduleUiSelfTest(engine, app);
    }
#endif

    return app.exec();
}