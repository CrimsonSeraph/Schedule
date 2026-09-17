#include "engine/AppBridge.h"
#include "engine/ImportExportBridge.h"
#include "engine/NotificationService.h"
#include "engine/ScheduleBridge.h"

#include "core/adapter/SchoolAdapter.h"
#include "data/AppSettings.h"
#include "data/SqliteScheduleRepository.h"
#include "data/adapter/GenericSchoolAdapter.h"
#include "data/adapter/NetworkScheduleFetcher.h"

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
#include <QDir>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QTranslator>
#include <QUrl>

#if defined(Schedule_HAS_WEBVIEW)
#include <QtWebView/QtWebView>
#endif

#include <memory>

#if Schedule_SELFTEST
#include "SelfTest.h"
#endif

int main(int argc, char* argv[]) {
#if defined(Schedule_HAS_WEBVIEW)
    // 在创建 Q*Application 实例之前调用
    QtWebView::initialize();
#endif

#if Schedule_HAS_TRAY_NOTIFICATIONS
    QApplication app(argc, argv);
#else
    QGuiApplication app(argc, argv);
#endif
    QGuiApplication& gui_app = app;
    gui_app.setOrganizationName(QStringLiteral("Schedule"));
    gui_app.setApplicationName(QStringLiteral("Schedule"));
    gui_app.setApplicationVersion(QStringLiteral("1.0.0"));

    // ------------------------------------------------------------------ 国际化
    // 源语言是简体中文；若存在与系统语言匹配的 .qm（由 qt_add_translations 嵌入 :/i18n），
    // 则安装翻译器，否则自动回退到中文源字符串。
    QTranslator translator;
    if (translator.load(QLocale(), QStringLiteral("schedule"), QStringLiteral("_"), QStringLiteral(":/i18n"))) {
        gui_app.installTranslator(&translator);
    }

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

    // ------------------------------------------------------------ 教务适配器
    // 分层约定：**接口在 core，实现在 data，注册在 app**。
    // 隐私约束：适配器只接收“WebView 登录后得到的 Cookie”，绝不接收或保存密码；
    //          凭证仅驻留内存（AdapterSession），可随时在设置页擦除。
    std::vector<std::shared_ptr<Schedule::IScheduleFetcher>> schedule_fetchers = {
        std::make_shared<Schedule::NetworkScheduleFetcher>(),
        std::make_shared<Schedule::LocalFileScheduleFetcher>(),
    };
    Schedule::SchoolAdapterRegistry adapter_registry;

    Schedule::AdapterInfo sample_adapter;
    sample_adapter.id = QStringLiteral("local-sample");
    sample_adapter.name = QStringLiteral("本地样本适配器");
    sample_adapter.description = QStringLiteral("离线演示：从 samples/schedule_sample.json 读取课表，用于验证适配器全链路");
    sample_adapter.schedule_url = QDir(QCoreApplication::applicationDirPath())
                                      .filePath(QStringLiteral("samples/schedule_sample.json"));
    sample_adapter.requires_session = false;
    sample_adapter.is_experimental = false;
    adapter_registry.register_adapter(
        std::make_unique<Schedule::GenericSchoolAdapter>(sample_adapter, schedule_fetchers));

    Schedule::AdapterInfo generic_adapter;
    generic_adapter.id = QStringLiteral("generic-jwgl");
    generic_adapter.name = QStringLiteral("通用教务适配器（实验性）");
    generic_adapter.description = QStringLiteral(
        "在浏览器 / WebView 中登录教务系统后，把课表接口地址与 Cookie 填入设置页，再点击导入；"
        "不保存密码，Cookie 仅驻留内存。返回内容需为 JSON / CSV / ICS。");
    generic_adapter.requires_session = true;
    generic_adapter.is_experimental = true;
    adapter_registry.register_adapter(
        std::make_unique<Schedule::GenericSchoolAdapter>(generic_adapter, schedule_fetchers));

    // 安徽工程大学（正方教务 V9）。**只配登录页**：适配器不直接抓取数据，而是作为
    // 「从教务导入」列表里的直达入口——用户在内嵌浏览器里登录后自己走到课表页，
    // 再由网页抓取导入。课表页实测为 /ahpu/courseTableForStd!courseTable.action，
    // 但其中带有与会话绑定的 ids 参数，交给用户在页面里自然地走一遍更稳妥。
    //
    // 隐私：应用不接收也不保存密码；登录会话只存在于内嵌 Web 组件里，
    // 抓取时只带走**当前页面的 HTML**，不做后台同步。
    Schedule::AdapterInfo ahpu_adapter;
    ahpu_adapter.id = QStringLiteral("ahpu-jwxt");
    ahpu_adapter.name = QStringLiteral("安徽工程大学教务系统（正方 V9）");
    ahpu_adapter.description = QStringLiteral(
        "点击后在内嵌浏览器中打开安徽工程大学教务系统；登录并进入“个人课表”页面后，"
        "点“导入课表”即可抓取当前页面。不保存密码，也不读取 Cookie。");
    ahpu_adapter.login_url = QStringLiteral("http://xjwxt.ahpu.edu.cn/ahpu/localLogin.action");
    ahpu_adapter.schedule_url.clear();
    // 抓取走的是内嵌浏览器的**当前页面**，不需要应用侧另行携带会话凭证
    ahpu_adapter.requires_session = false;
    ahpu_adapter.is_experimental = true;
    adapter_registry.register_adapter(
        std::make_unique<Schedule::GenericSchoolAdapter>(ahpu_adapter, schedule_fetchers));

    // 华东交通大学（教务综合管理系统）。与安徽工程大学一样是**浏览器直达入口**：
    // 课表页要在登录会话内才有，交给用户在内嵌浏览器里自然地走一遍，比在应用里猜地址稳。
    //
    // 该校用的是自研/第三方的“教务综合管理系统”，**不是**正方系，页面结构与课表字段
    // 顺序都不同，因此解析由 EcjtuTimetableIo 承担（见 data/import_export/academic_affairs/）。
    // 两条路线共用同一个解析器：
    //   - 网页抓取：登录后停在课表页，点“导入课表”抓当前页面（页面的 outerHTML）；
    //   - 文件导入：在该系统里“导出”课表（得到 .doc / .docx 的 Word 表格）后走文件导入。
    // 前者依赖课表表格出现在页面 HTML 里；若该页面是纯脚本渲染，抓到的 HTML 里没有表格，
    // 解析会明确报错并提示改用后者——导出文件的解析是逐格核对过的。
    Schedule::AdapterInfo ecjtu_jwzhglxt;
    ecjtu_jwzhglxt.id = QStringLiteral("ecjtu-jwzhglxt");
    ecjtu_jwzhglxt.name = QStringLiteral("华东交通大学教务综合管理系统");
    ecjtu_jwzhglxt.description = QStringLiteral(
        "点击后在内嵌浏览器中打开华东交通大学教务综合管理系统；登录并进入课表页面后，"
        "点“导入课表”即可抓取当前页面。若抓不到表格，可改用该系统“导出”的课表文件导入。"
        "不保存密码，也不读取 Cookie。");
    ecjtu_jwzhglxt.login_url = QStringLiteral("https://jwxt.ecjtu.edu.cn");
    ecjtu_jwzhglxt.schedule_url.clear();
    // 抓取走的是内嵌浏览器的**当前页面**，不需要应用侧另行携带会话凭证
    ecjtu_jwzhglxt.requires_session = false;
    ecjtu_jwzhglxt.is_experimental = true;
    adapter_registry.register_adapter(
        std::make_unique<Schedule::GenericSchoolAdapter>(ecjtu_jwzhglxt, schedule_fetchers));

    // ---------------------------------------------------------------- 桥接对象
    // 桥接对象由 C++ 侧创建并持有，以上下文属性注入 QML（QML 不再自行实例化）
    Schedule::AppBridge app_bridge;
    Schedule::ScheduleBridge schedule_bridge(&schedule_service, repository.get(), &app_settings);
    schedule_bridge.initialize();
    // 适配器注册表必须在 ScheduleBridge 之前构造（见上方），此处仅注入引用
    schedule_bridge.import_export()->set_adapter_registry(&adapter_registry);

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
