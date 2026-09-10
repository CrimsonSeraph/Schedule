#include "engine/AppBridge.h"
#include "engine/NotificationService.h"
#include "engine/ScheduleBridge.h"

#include "data/AppSettings.h"
#include "data/SqliteScheduleRepository.h"

#include "UiConnector.h"

#if Schedule_HAS_TRAY_NOTIFICATIONS
#include "TrayNotificationBackend.h"

// 系统托盘图标（QSystemTrayIcon）依赖 Qt Widgets，必须使用 QApplication；
// 移动端没有托盘，继续使用更轻量的 QGuiApplication。
#include <QApplication>
#else
#include <QGuiApplication>
#endif

#include <QDebug>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QUrl>

#include <memory>

#if Schedule_SELFTEST
#include "SelfTest.h"
#endif

int main(int argc, char* argv[]) {
#if Schedule_HAS_TRAY_NOTIFICATIONS
    QApplication app(argc, argv);
#else
    QGuiApplication app(argc, argv);
#endif
    QGuiApplication& gui_app = app;
    gui_app.setOrganizationName(QStringLiteral("Schedule"));
    gui_app.setApplicationName(QStringLiteral("Schedule"));
    gui_app.setApplicationVersion(QStringLiteral("1.0.0"));

    // ---------------------------------------------------------------- 数据层组装
    // 数据库位于 AppDataLocation/schedule.db；若无法打开（只读介质、驱动缺失等），
    // 自动降级为内存库，保证界面仍可启动，只是本次会话的数据不会留存。
    auto repository = std::make_unique<Schedule::SqliteScheduleRepository>(Schedule::SqliteScheduleRepository::default_database_path());
    QString repository_error;
    if (!repository->open(&repository_error)) {
        qWarning() << "[app] 打开数据库失败，降级为内存模式：" << repository_error;
        repository = std::make_unique<Schedule::SqliteScheduleRepository>(QStringLiteral(":memory:"));
        if (!repository->open(&repository_error)) {
            qCritical() << "[app] 内存数据库同样不可用：" << repository_error;
        }
    }

    Schedule::AppSettings app_settings(repository.get());
    Schedule::ScheduleService schedule_service;

    // ---------------------------------------------------------------- 桥接对象
    // 桥接对象由 C++ 侧创建并持有，以上下文属性注入 QML（QML 不再自行实例化）
    Schedule::AppBridge app_bridge;
    Schedule::ScheduleBridge schedule_bridge(&schedule_service, repository.get(), &app_settings);
    schedule_bridge.initialize();

    // ---------------------------------------------------------------- 本地提醒
    // 提醒服务只负责“何时提醒”，系统通知由后端投递：
    //  - 桌面：TrayNotificationBackend（系统托盘气泡，需 Qt Widgets，故放在 app 层组装）
    //  - Android：AndroidNotificationBackend（engine 层，QJniObject 调用 NotificationManager）
    //  - 其它平台：自动回退为应用内横幅
    Schedule::NotificationService notification_service(&schedule_service, &app_settings);
#if Schedule_HAS_TRAY_NOTIFICATIONS
    Schedule::TrayNotificationBackend tray_notification_backend;
    notification_service.set_backend(&tray_notification_backend);
    notification_service.request_permission();
#endif
    notification_service.start();

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("bridge"), &app_bridge);
    engine.rootContext()->setContextProperty(QStringLiteral("schedule"), &schedule_bridge);
    engine.rootContext()->setContextProperty(QStringLiteral("reminders"), &notification_service);

    // 平台分流：移动端加载手机布局，其余平台加载桌面布局
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
    const QUrl main_qml(QStringLiteral("qrc:/qt/qml/Schedule/qml/MainMobile.qml"));
#else
    const QUrl main_qml(QStringLiteral("qrc:/qt/qml/Schedule/qml/MainDesktop.qml"));
#endif

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        [main_qml](const QUrl& object_url) {
            qWarning() << "Failed to load QML:" << object_url << "(" << main_qml << ")";
        },
        Qt::QueuedConnection);

    engine.load(main_qml);

    if (engine.rootObjects().isEmpty()) {
        return -1;
    }

    // 显式信号连接集中在 C++ 侧完成，QML 中不再出现 onClicked / Connections 等
    // 按名称隐式连接的写法：
    // 1) QML “测试”按钮 clicked -> AppBridge::test_button_clicked()
    //    QML 控件信号在公开 C++ 头文件中不可见，故按元对象签名显式连接。
    if (QObject* root = engine.rootObjects().value(0)) {
        QObject* test_button = root->findChild<QObject*>(QStringLiteral("testButton"));
        if (!test_button) {
            qWarning() << "[app] testButton not found; click-to-bridge connection was not established";
        }
        else if (!QObject::connect(
                     test_button,
                     SIGNAL(clicked()),
                     &app_bridge,
                     SLOT(test_button_clicked()))) {
            qWarning() << "[app] failed to connect testButton.clicked to AppBridge::test_button_clicked";
        }
    }

    // 2) 其余全部界面交互（导航、周次、课程编辑、导入导出向导、设置）统一由
    //    UiConnector 在 C++ 侧按 objectName 显式连接；QML 中不含任何信号处理器。
    Schedule::UiConnector ui_connector(
        engine.rootObjects().value(0), &schedule_bridge, &app_bridge, &notification_service, &app);
    ui_connector.connect_all();

    // 2) AppBridge::test_signal(message) -> 应用日志（原 QML Connections 消费逻辑迁移到 C++）
    QObject::connect(
        &app_bridge,
        &Schedule::AppBridge::test_signal,
        &gui_app,
        [](const QString& message) {
            qDebug() << "[app] test_signal received:" << message;
        });

    // 3) ScheduleBridge 的错误 / 提示 -> 应用日志
    //    界面提示由 QML 通过属性绑定 lastError / lastInfo 呈现，这里只做日志留痕。
    QObject::connect(
        &schedule_bridge,
        &Schedule::ScheduleBridge::errorOccurred,
        &gui_app,
        [](const QString& message) {
            qWarning() << "[app] schedule error:" << message;
        });
    QObject::connect(
        &schedule_bridge,
        &Schedule::ScheduleBridge::infoMessage,
        &gui_app,
        [](const QString& message) {
            qDebug() << "[app] schedule info:" << message;
        });

    // 4) 导入导出结果 -> 应用日志（含实际导出路径，便于排查“文件到底写到哪里了”）
    QObject::connect(
        schedule_bridge.import_export(),
        &Schedule::ImportExportBridge::exportFinished,
        &gui_app,
        [](bool success, const QString& summary, const QString& file_path) {
            if (success) {
                qInfo() << "[app] export finished:" << summary << "->" << file_path;
            }
            else {
                qWarning() << "[app] export failed:" << summary;
            }
        });
    QObject::connect(
        schedule_bridge.import_export(),
        &Schedule::ImportExportBridge::importFinished,
        &gui_app,
        [](bool success, const QString& summary) {
            if (success) {
                qInfo() << "[app] import finished:" << summary;
            }
            else {
                qWarning() << "[app] import failed:" << summary;
            }
        });

#if Schedule_SELFTEST
    // --selftest：在真实 QML 场景下端到端验证导入导出与 C++ 侧连接（详见 SelfTest 注释）
    std::unique_ptr<Schedule::SelfTest> self_test;
    if (app.arguments().contains(QStringLiteral("--selftest"))) {
        self_test = std::make_unique<Schedule::SelfTest>(
            engine.rootObjects().value(0), &schedule_bridge, &app_bridge, &gui_app);
        self_test->start();
    }
#endif

    return app.exec();
}
