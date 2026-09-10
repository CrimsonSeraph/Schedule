#include "SelfTest.h"

#include "engine/ImportExportBridge.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QQuickItem>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>

#include <QtTest/QTest>

namespace Schedule {

    namespace {

        /** 首步延时（毫秒）：等 QML 场景完成首次布局与模型填充。 */
        constexpr int INITIAL_DELAY_MS = 1500;

        /** 步骤间隔（毫秒）：给事件循环留出处理信号与重绘的时间。 */
        constexpr int STEP_DELAY_MS = 300;

    } // namespace

    SelfTest::SelfTest(QObject* root,
        ScheduleBridge* bridge,
        AppBridge* app_bridge,
        QGuiApplication* app,
        QObject* parent)
        : QObject(parent)
        , m_root(root)
        , m_bridge(bridge)
        , m_app_bridge(app_bridge)
        , m_app(app) {
        if (m_app_bridge) {
            connect(m_app_bridge, &AppBridge::test_signal, this, [this](const QString&) {
                m_test_signal_received = true;
            });
        }
    }

    SelfTest::~SelfTest() = default;

    void SelfTest::start() {
        const QString samples = samples_directory();
        const QString export_dir = export_directory();
        ImportExportBridge* io = import_export();

        m_steps = {
            // 1) 通过点击底部/顶部导航切到设置页（同时验证导航连接）
            [this]() {
                auto* window = qobject_cast<QQuickWindow*>(m_root.data());
                QQuickItem* nav = m_root ? m_root->findChild<QQuickItem*>(QStringLiteral("navSettingsButton")) : nullptr;
                if (!check(window != nullptr, QStringLiteral("QML 根对象是 QQuickWindow"))) {
                    return;
                }
                if (!check(nav != nullptr, QStringLiteral("找到导航按钮 navSettingsButton"))) {
                    return;
                }
                const QPointF center = nav->mapToScene(QPointF(nav->width() / 2.0, nav->height() / 2.0));
                QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center.toPoint());
            },
            [this]() {
                QObject* page_stack = m_root ? m_root->findChild<QObject*>(QStringLiteral("pageStack")) : nullptr;
                check(page_stack && page_stack->property("currentIndex").toInt() == 3,
                    QStringLiteral("点击导航后切换到设置页（C++ 侧导航连接生效）"));
            },

            // 2) 点击设置页的“自检”按钮，验证 Button::clicked -> AppBridge 的连接链路
            [this]() {
                auto* window = qobject_cast<QQuickWindow*>(m_root.data());
                QQuickItem* button = m_root ? m_root->findChild<QQuickItem*>(QStringLiteral("testButton")) : nullptr;
                if (!check(button != nullptr, QStringLiteral("找到自检按钮 testButton"))) {
                    return;
                }
                if (!check(button->isVisible(), QStringLiteral("自检按钮当前可见"))) {
                    return;
                }
                const QPointF center = button->mapToScene(QPointF(button->width() / 2.0, button->height() / 2.0));
                QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center.toPoint());
            },
            [this]() {
                check(m_test_signal_received, QStringLiteral("点击按钮触发 AppBridge::test_signal（C++ 侧连接生效）"));
            },

            // 2) 预览样本文件
            [this, samples, io]() {
                if (!check(!samples.isEmpty(), QStringLiteral("找到样本文件目录"))) {
                    return;
                }
                io->preview_import(QUrl::fromLocalFile(QDir(samples).filePath(QStringLiteral("schedule_sample.json"))));
            },
            [this, io]() {
                if (!check(io->has_pending_preview(), QStringLiteral("样本 JSON 生成导入预览"))) {
                    return;
                }
                check(io->preview_new_count() == 4, QStringLiteral("预览识别出 4 门待新增课程"));
                check(io->preview_duplicate_count() == 0, QStringLiteral("空课表下没有重复课程"));
            },

            // 3) 合并导入
            [io]() { io->apply_import(0); },
            [this]() {
                check(m_bridge->course_count() == 4, QStringLiteral("导入后课表包含 4 门课程"));
                check(m_bridge->has_semester(), QStringLiteral("导入后学期信息有效"));
            },

            // 4) 导出到指定目录（三种格式）
            [this, export_dir, io]() {
                const QUrl target = QUrl::fromLocalFile(export_dir);
                io->export_schedule(0, target);
                if (!check(!io->last_export_path().isEmpty(), QStringLiteral("导出 JSON 返回实际路径"))) {
                    return;
                }
                m_exported_files.append(io->last_export_path());
                io->export_schedule(1, target);
                m_exported_files.append(io->last_export_path());
                io->export_schedule(2, target);
                m_exported_files.append(io->last_export_path());
            },
            [this, export_dir]() {
                check(m_exported_files.size() == 3, QStringLiteral("依次导出了 JSON / CSV / ICS 三种格式"));
                for (const QString& path : m_exported_files) {
                    const QFileInfo info(path);
                    check(info.exists() && info.size() > 0, QStringLiteral("导出文件存在且非空：%1").arg(info.fileName()));
                    check(QDir(export_dir) == info.absoluteDir(), QStringLiteral("导出文件位于指定目录：%1").arg(info.fileName()));
                }
            },

            // 5) 往返：把刚导出的 JSON 再导入一次，应识别为全部重复
            [this, io]() {
                if (m_exported_files.isEmpty()) {
                    return;
                }
                io->cancel_import();
                io->preview_import(QUrl::fromLocalFile(m_exported_files.first()));
            },
            [this, io]() {
                check(io->has_pending_preview(), QStringLiteral("导出的 JSON 可以被重新导入"));
                check(io->preview_duplicate_count() == 4, QStringLiteral("重新导入时 4 门课程全部识别为重复"));
                io->cancel_import();
            },
        };

        qInfo() << "[selftest] 开始，样本目录：" << (samples.isEmpty() ? QStringLiteral("(未找到)") : samples);
        qInfo() << "[selftest] 导出目录：" << export_dir;
        schedule_next_step(INITIAL_DELAY_MS);
    }

    void SelfTest::schedule_next_step(int delay_ms) {
        QTimer::singleShot(delay_ms, this, [this]() { run_next_step(); });
    }

    void SelfTest::run_next_step() {
        if (m_finished) {
            return;
        }
        if (m_failed || m_step_index >= m_steps.size()) {
            finish();
            return;
        }

        const std::size_t index = m_step_index++;
        m_steps.at(index)();

        if (m_failed) {
            finish();
            return;
        }
        schedule_next_step(STEP_DELAY_MS);
    }

    void SelfTest::finish() {
        if (m_finished) {
            return;
        }
        m_finished = true;

        if (m_failed) {
            qWarning() << "[selftest] 失败：已通过" << m_passed_checks << "项检查，请查看上方日志";
            if (m_app) {
                m_app->exit(2);
            }
            return;
        }

        qInfo() << "[selftest] 全部通过，共" << m_passed_checks << "项检查";
        qInfo() << "[selftest] 导出文件：";
        for (const QString& path : m_exported_files) {
            qInfo() << "           " << path;
        }
        if (m_app) {
            m_app->exit(0);
        }
    }

    bool SelfTest::check(bool condition, const QString& description) {
        if (condition) {
            ++m_passed_checks;
            qInfo().noquote() << "[selftest] PASS" << description;
            return true;
        }
        m_failed = true;
        qWarning().noquote() << "[selftest] FAIL" << description;
        return false;
    }

    QString SelfTest::samples_directory() const {
        // 构建时 samples/ 会被复制到可执行文件同级目录；同时兼容直接从仓库运行的场景
        const QString app_dir = QCoreApplication::applicationDirPath();
        const QStringList candidates = {
            QDir(app_dir).filePath(QStringLiteral("samples")),
            QDir(app_dir).filePath(QStringLiteral("../../../samples")),
            QDir::current().filePath(QStringLiteral("samples")),
        };
        for (const QString& candidate : candidates) {
            const QDir directory(candidate);
            if (directory.exists() && directory.exists(QStringLiteral("schedule_sample.json"))) {
                return directory.absolutePath();
            }
        }
        return QString();
    }

    QString SelfTest::export_directory() const {
        const QString base = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        const QString directory = QDir(base.isEmpty() ? QDir::tempPath() : base).filePath(QStringLiteral("schedule-selftest"));
        QDir(directory).removeRecursively();
        return directory;
    }

    ImportExportBridge* SelfTest::import_export() const {
        return m_bridge ? m_bridge->import_export() : nullptr;
    }

} // namespace Schedule
