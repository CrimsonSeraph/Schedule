#include "UiConnector.h"

#include "Logging.h"
#include "core/service/WeekCalculator.h"
#include "engine/CourseListModel.h"
#include "engine/NotificationService.h"

#include <QAbstractItemModel>
#include <QDateTime>
#include <QDebug>
#if defined(Q_OS_ANDROID)
#include <QJniObject>
#endif
#include <QFont>
#include <QImage>
#include <QPainter>
#include <QDesktopServices>
#include <QDir>
#include <QMetaMethod>
#include <QMetaObject>
#include <QQuickItem>
#include <QQuickItemGrabResult>
#include <QQuickWindow>
#include <QSharedPointer>
#include <QTimer>
#include <QUrl>
#include <QVariantList>

#include <functional>

namespace Schedule {

    namespace {

        /**
         * @brief Android 上请求屏幕方向。
         * @param orientation 0=横屏，1=竖屏，-1=跟随系统传感器。
         *
         * 内嵌教务页面是横屏布局，进入浏览器时切横屏、退出时恢复系统方向；
         * 桌面平台为空操作。
         */
        /**
         * @brief Android 上通过系统分享面板发送图片。
         * @param path 图片在本机的绝对路径（位于应用 external files 目录下）。
         * @return 成功拉起分享面板返回 true。
         *
         * 复用 Qt 模板自带的 FileProvider（authority = <package>.qtprovider），
         * 其 qtprovider_paths.xml 已覆盖 external-files 目录，无需再新增 provider。
         */
#if defined(Q_OS_ANDROID)
        bool share_image_android(const QString& path) {
            QJniObject activity = QJniObject::callStaticObjectMethod(
                "org/qtproject/qt/android/QtNative", "activity", "()Landroid/app/Activity;");
            if (!activity.isValid()) {
                qWarning() << "[share] 无法取得 Android Activity";
                return false;
            }

            const QString package = activity.callObjectMethod("getPackageName", "()Ljava/lang/String;").toString();
            const QString authority = package + QStringLiteral(".qtprovider");
            QJniObject file("java/io/File", "(Ljava/lang/String;)V", QJniObject::fromString(path).object());
            QJniObject uri = QJniObject::callStaticObjectMethod(
                "androidx/core/content/FileProvider", "getUriForFile",
                "(Landroid/content/Context;Ljava/lang/String;Ljava/io/File;)Landroid/net/Uri;",
                activity.object(), QJniObject::fromString(authority).object(), file.object());
            if (!uri.isValid()) {
                qWarning() << "[share] FileProvider 返回空 URI" << authority << path;
                return false;
            }

            QJniObject intent("android/content/Intent", "()V");
            intent.callObjectMethod("setAction", "(Ljava/lang/String;)Landroid/content/Intent;",
                QJniObject::fromString(QStringLiteral("android.intent.action.SEND")).object());
            intent.callObjectMethod("setType", "(Ljava/lang/String;)Landroid/content/Intent;",
                QJniObject::fromString(QStringLiteral("image/png")).object());
            intent.callObjectMethod("putExtra", "(Ljava/lang/String;Landroid/os/Parcelable;)Landroid/content/Intent;",
                QJniObject::fromString(QStringLiteral("android.intent.extra.STREAM")).object(), uri.object());
            intent.callObjectMethod("addFlags", "(I)Landroid/content/Intent;", 1); // FLAG_GRANT_READ_URI_PERMISSION

            QJniObject chooser = QJniObject::callStaticObjectMethod(
                "android/content/Intent", "createChooser",
                "(Landroid/content/Intent;Ljava/lang/CharSequence;)Landroid/content/Intent;",
                intent.object(), QJniObject::fromString(QStringLiteral("分享课表图片")).object());
            activity.callMethod<void>("startActivity", "(Landroid/content/Intent;)V", chooser.object());
            qInfo() << "[share] 已打开系统分享面板" << path;
            return true;
        }
#endif

        void request_screen_orientation(int orientation) {
#if defined(Q_OS_ANDROID)
            QJniObject activity = QJniObject::callStaticObjectMethod(
                "org/qtproject/qt/android/QtNative", "activity", "()Landroid/app/Activity;");
            if (!activity.isValid()) {
                qWarning() << "[browser] 无法取得 Android Activity，忽略屏幕方向请求";
                return;
            }
            activity.callMethod<void>("setRequestedOrientation", "(I)V", orientation);
#else
            Q_UNUSED(orientation)
#endif
        }

        /** @return 由本地目录字符串构造 QUrl。 */
        QUrl local_url(const QString& path) {
            return QUrl::fromLocalFile(path.trimmed());
        }

        /** @return 指定控件上某个 QUrl 属性转换出的本地路径；控件不存在返回空串。 */
        QString local_path_property(QObject* object, const char* name) {
            return object ? object->property(name).toUrl().toLocalFile() : QString();
        }

        /** @return QML `ComboBox` 当前选中项的 `valueRole` 值。 */
        QVariant combo_value(QObject* combo) {
            return combo ? combo->property("currentValue") : QVariant();
        }

        /** @return QML `ComboBox` 的当前下标；控件不存在返回 0。 */
        int combo_index(QObject* combo) {
            return combo ? combo->property("currentIndex").toInt() : 0;
        }

        /**
         * @return 时间段草稿行的展示文本，形如“周一 第 1-2 节 · 1-16 · 教一 101”。
         *
         * 表单新建的行与从课程数据回填的行共用它，保证草稿列表文本一致。
         */
        QString session_summary_text(const QVariantMap& row) {
            QString summary = QStringLiteral("%1 第 %2-%3 节 · %4")
                                  .arg(row.value(QStringLiteral("dayName")).toString())
                                  .arg(row.value(QStringLiteral("startSlot")).toInt())
                                  .arg(row.value(QStringLiteral("endSlot")).toInt())
                                  .arg(row.value(QStringLiteral("weeks")).toString());
            const QString location = row.value(QStringLiteral("location")).toString();
            if (!location.isEmpty()) {
                summary += QStringLiteral(" · ") + location;
            }
            return summary;
        }

        /**
         * @brief 递归收集可视子树中 `objectName` 匹配的节点。
         *
         * `Repeater` / `ListView` 生成的委托经 `setParentItem()` 挂进可视树，`QObject`
         * 父对象为空，`findChildren()` 扫不到，只能沿 `QQuickItem::childItems()` 查找。
         */
        void collect_visual_children(QQuickItem* item, const QString& name, QList<QQuickItem*>* out) {
            if (!item) {
                return;
            }
            if (item->objectName() == name) {
                out->append(item);
            }
            for (QQuickItem* child : item->childItems()) {
                collect_visual_children(child, name, out);
            }
        }

    } // namespace

    UiConnector::UiConnector(QObject* root,
        ScheduleBridge* bridge,
        AppBridge* app_bridge,
        NotificationService* notifications,
        QObject* parent)
        : QObject(parent)
        , m_root(root)
        , m_bridge(bridge)
        , m_app_bridge(app_bridge)
        , m_notifications(notifications) {
    }

    UiConnector::~UiConnector() = default;

    // ------------------------------------------------------------查找与读写

    QObject* UiConnector::find(const char* name) const {
        if (!m_root) {
            return nullptr;
        }
        return m_root->findChild<QObject*>(QString::fromLatin1(name));
    }

    QString UiConnector::text_of(const char* name) const {
        QObject* object = find(name);
        return object ? object->property("text").toString() : QString();
    }

    void UiConnector::set_text(const char* name, const QString& text) {
        if (QObject* object = find(name)) {
            object->setProperty("text", text);
        }
    }

    int UiConnector::int_of(const char* name, int fallback) const {
        QObject* object = find(name);
        if (!object) {
            return fallback;
        }
        const QVariant value = object->property("value");
        return value.isValid() ? value.toInt() : fallback;
    }

    void UiConnector::set_int(const char* name, int value) {
        if (QObject* object = find(name)) {
            object->setProperty("value", value);
        }
    }

    void UiConnector::set_property(QObject* object, const char* name, const QVariant& value) {
        if (object) {
            object->setProperty(name, value);
        }
    }

    bool UiConnector::invoke(QObject* object, const char* method) {
        return object && QMetaObject::invokeMethod(object, method);
    }

    bool UiConnector::on_signal(const char* name, const char* signal, std::function<void()> handler) {
        QObject* object = find(name);
        if (!object) {
            // 桌面端与移动端控件集不完全相同，缺失不视为错误
            return false;
        }
        return on_signal(object, signal, std::move(handler));
    }

    bool UiConnector::on_signal(QObject* object, const char* signal, std::function<void()> handler) {
        if (!object) {
            return false;
        }

        const QMetaObject* meta = object->metaObject();
        const QByteArray provided(signal);
        int index = meta->indexOfSignal(provided.constData());

        if (index < 0) {
            const int paren = provided.indexOf('(');
            const QByteArray bare = paren >= 0 ? provided.left(paren) : provided;
            for (int i = 0; i < meta->methodCount(); ++i) {
                const QMetaMethod m = meta->method(i);
                if (m.methodType() == QMetaMethod::Signal && QByteArray(m.name()) == bare) {
                    index = i;
                    break;
                }
            }
        }

        if (index < 0) {
            qWarning() << "[app] 找不到信号：" << object->objectName() << signal;
            return false;
        }

        m_handlers.insert(qMakePair(object, index), std::move(handler));

        const QMetaMethod method = meta->method(index);
        const QByteArray actual = QByteArray("2") + method.methodSignature();

        const bool connected = QObject::connect(object, actual.constData(), this, SLOT(dispatch()));
        if (connected) {
            ++m_connection_count;
        }
        else {
            qWarning() << "[app] 无法连接信号：" << object->objectName() << actual;
        }
        return connected;
    }

    bool UiConnector::on_click(const char* name, std::function<void()> handler) {
        return on_signal(name, "clicked()", std::move(handler));
    }

    void UiConnector::dispatch() {
        QObject* object = sender();
        if (!object) {
            return;
        }
        const auto iterator = m_handlers.constFind(qMakePair(object, senderSignalIndex()));
        if (iterator != m_handlers.constEnd() && *iterator) {
            (*iterator)();
        }
    }

    // ----------------------------------------------------------------入口

    int UiConnector::connect_all() {
        if (!m_root) {
            qWarning() << "[app] UiConnector: QML 根对象为空，未建立任何连接";
            return 0;
        }
        if (!m_bridge) {
            qWarning() << "[app] UiConnector: ScheduleBridge 为空，未建立任何连接";
            return 0;
        }

        m_page_stack = find("pageStack");
        m_course_editor = find("courseEditor");
        m_course_detail = find("courseDetailDialog");
        m_import_wizard = find("importWizard");
        m_export_dialog = find("exportDialog");
        m_browser_dialog = find("browserImportDialog");

        int count = 0;
        Q_UNUSED(count);

        connect_navigation();
        connect_week_navigation();
        connect_course_actions();
        connect_semester_page();
        connect_settings_page();
        connect_course_editor();
        connect_course_detail();
        connect_import_wizard();
        connect_browser_import();
        connect_export_dialog();
        connect_reminders();
        connect_school_adapters();

        prime_widgets();

        // 模型重置后 Repeater / ListView 会重建委托，需要重新连接热区；
        // 委托增删本身由承载容器的 childrenChanged 触发（见 connect_session_cards()）
        if (QAbstractItemModel* course_model = m_bridge->course_model()) {
            connect(course_model, &QAbstractItemModel::modelReset, this, [this]() { schedule_card_reconnect(); });
        }
        if (QAbstractItemModel* week_model = m_bridge->week_model()) {
            connect(week_model, &QAbstractItemModel::modelReset, this, [this]() { schedule_card_reconnect(); });
        }
        if (QAbstractItemModel* session_model = m_bridge->session_model()) {
            connect(session_model, &QAbstractItemModel::modelReset, this, [this]() { schedule_card_reconnect(); });
        }

        schedule_card_reconnect();
        qInfo() << "[app] UiConnector: 已建立" << m_connection_count << "个 QML ↔ C++ 连接";
        return m_connection_count;
    }

    // ----------------------------------------------------------------导航

    void UiConnector::connect_navigation() {
        const auto go_to_page = [this](int index) {
            set_property(m_page_stack, "currentIndex", index);
        };

        on_click("navWeekButton", [go_to_page]() { go_to_page(0); });
        on_click("navDayButton", [go_to_page]() { go_to_page(1); });
        on_click("navSemesterButton", [go_to_page]() { go_to_page(2); });
        on_click("navSettingsButton", [go_to_page]() { go_to_page(3); });
    }

    void UiConnector::connect_week_navigation() {
        on_click("prevWeekButton", [this]() { m_bridge->previous_week(); });
        on_click("nextWeekButton", [this]() { m_bridge->next_week(); });
        on_click("currentWeekButton", [this]() { m_bridge->go_to_current_week(); });

        on_signal("weekSelector", "currentIndexChanged()", [this]() {
            if (m_syncing_week_selector) {
                return;
            }
            QObject* combo = find("weekSelector");
            if (!combo) {
                return;
            }
            const int index = combo->property("currentIndex").toInt();
            if (index >= 0) {
                m_bridge->select_week(index + 1);
            }
        });

        on_signal("weekSlider", "moved()", [this]() {
            QObject* slider = find("weekSlider");
            if (slider) {
                m_bridge->select_week(slider->property("value").toInt());
            }
        });

        connect(m_bridge, &ScheduleBridge::selectedWeekChanged, this, [this]() {
            const int week = m_bridge->property("selectedWeek").toInt();
            if (QObject* slider = find("weekSlider")) {
                if (slider->property("value").toInt() != week) {
                    slider->setProperty("value", week);
                }
            }
            QObject* combo = find("weekSelector");
            if (!combo) {
                return;
            }
            const int index = qMax(0, week - 1);
            if (combo->property("currentIndex").toInt() == index) {
                return;
            }
            m_syncing_week_selector = true;
            combo->setProperty("currentIndex", index);
            m_syncing_week_selector = false;
        });

        if (QObject* combo = find("weekSelector")) {
            const int week = m_bridge->property("selectedWeek").toInt();
            m_syncing_week_selector = true;
            combo->setProperty("currentIndex", qMax(0, week - 1));
            m_syncing_week_selector = false;
            if (QObject* slider = find("weekSlider")) {
                slider->setProperty("value", week);
            }
        }

        on_signal("daySelector", "currentIndexChanged()", [this]() {
            if (m_syncing_day_selector) {
                return;
            }
            QObject* combo = find("daySelector");
            if (!combo) {
                return;
            }
            // 读 currentIndex 而不是 currentValue：后者在 currentIndexChanged 触发时尚未更新，
            // 会读回旧值 0 并把「整周」写回 selectedDay（dayOptions 的下标与 value 一致）
            const int day_of_week = combo->property("currentIndex").toInt();
            if (day_of_week >= 0) {
                m_bridge->select_day(day_of_week);
            }
        });

        // selectedDay 也可能由业务侧改变（初始化等），统一由 C++ 回写下拉框，避免两边不同步
        connect(m_bridge, &ScheduleBridge::selectedDayChanged, this, [this]() {
            QObject* combo = find("daySelector");
            if (!combo) {
                return;
            }
            const int index = qMax(0, m_bridge->property("selectedDay").toInt());
            if (combo->property("currentIndex").toInt() == index) {
                return;
            }
            m_syncing_day_selector = true;
            combo->setProperty("currentIndex", index);
            m_syncing_day_selector = false;
        });

        if (QObject* combo = find("daySelector")) {
            m_syncing_day_selector = true;
            combo->setProperty("currentIndex", qMax(0, m_bridge->property("selectedDay").toInt()));
            m_syncing_day_selector = false;
        }
    }

    // ------------------------------------------------------------课程操作

    void UiConnector::connect_course_actions() {
        on_click("addCourseButton", [this]() { open_course_editor(QString()); });
        on_click("pageNewCourseButton", [this]() { open_course_editor(QString()); });

        on_click("editCourseButton", [this]() {
            QObject* list = find("courseList");
            auto* model = qobject_cast<CourseListModel*>(m_bridge->course_model());
            if (!list || !model) {
                return;
            }
            const int row = list->property("currentIndex").toInt();
            const QVariantMap course = model->get(row);
            open_course_editor(course.value(QStringLiteral("courseId")).toString());
        });

        on_click("deleteCourseButton", [this]() {
            QObject* list = find("courseList");
            auto* model = qobject_cast<CourseListModel*>(m_bridge->course_model());
            if (!list || !model) {
                return;
            }
            const int row = list->property("currentIndex").toInt();
            const QVariantMap course = model->get(row);
            const QString course_id = course.value(QStringLiteral("courseId")).toString();
            if (course_id.isEmpty()) {
                return;
            }
            m_bridge->remove_course(course_id);
        });

        connect_overflow_menu();
    }

    void UiConnector::connect_semester_page() {
        on_click("saveSemesterButton", [this]() {
            const QString name = text_of("semesterNameField");
            const QString start = text_of("semesterStartField");
            const int weeks = int_of("semesterWeeksSpin", 20);

            const bool creating = m_creating_semester || !m_bridge->has_semester();
            if (creating) {
                m_bridge->create_semester(name, start, weeks);
            }
            else {
                m_bridge->update_semester(name, start, weeks);
            }
            m_creating_semester = false;
            refresh_semester_form();
        });

        on_signal("semesterSelector", "currentIndexChanged()", [this]() {
            QObject* combo = find("semesterSelector");
            if (!combo) {
                return;
            }
            const int index = combo->property("currentIndex").toInt();
            const QVariantList options = m_bridge->semester_options();
            if (index < 0 || index >= options.size()) {
                return;
            }
            m_bridge->switch_semester(options.at(index).toMap().value(QStringLiteral("id")).toString());
            refresh_semester_form();
        });

        on_click("deleteSemesterButton", [this]() {
            if (m_creating_semester) {
                return;
            }
            if (QObject* dialog = find("semesterDeleteDialog")) {
                invoke(dialog, "open");
            }
        });

        on_click("semesterDeleteCancelButton", [this]() {
            if (QObject* dialog = find("semesterDeleteDialog")) {
                invoke(dialog, "close");
            }
        });

        on_click("semesterDeleteConfirmButton", [this]() {
            const QVariantList options = m_bridge->semester_options();
            const int index = m_bridge->current_semester_index();
            if (index >= 0 && index < options.size()) {
                m_bridge->remove_semester(options.at(index).toMap().value(QStringLiteral("id")).toString());
            }
            if (QObject* dialog = find("semesterDeleteDialog")) {
                invoke(dialog, "close");
            }
            refresh_semester_form();
        });

        on_click("newSemesterButton", [this]() {
            m_creating_semester = true;
            set_text("semesterNameField", QString());
            set_text("semesterStartField", WeekCalculator::monday_of(QDate::currentDate()).toString(Qt::ISODate));
            set_int("semesterWeeksSpin", 20);
            set_int("semesterSelector", -1);
            set_text("saveSemesterButton", QStringLiteral("创建学期"));
        });

        connect(m_bridge, &ScheduleBridge::semesterChanged, this, [this]() { refresh_semester_form(); });
    }

    void UiConnector::refresh_semester_form() {
        if (m_creating_semester) {
            set_int("semesterSelector", -1);
            set_text("saveSemesterButton", QStringLiteral("创建学期"));
            return;
        }
        if (m_bridge->has_semester()) {
            set_text("semesterNameField", m_bridge->semester_name());
            set_text("semesterStartField", m_bridge->semester_start_date());
            set_int("semesterWeeksSpin", m_bridge->total_weeks());
            set_int("semesterSelector", m_bridge->current_semester_index());
            set_text("saveSemesterButton", QStringLiteral("保存学期信息"));
        }
        else {
            set_text("semesterNameField", QStringLiteral("我的课表"));
            set_text("semesterStartField", WeekCalculator::monday_of(QDate::currentDate()).toString(Qt::ISODate));
            set_int("semesterWeeksSpin", 20);
            set_int("semesterSelector", -1);
            set_text("saveSemesterButton", QStringLiteral("创建学期"));
        }
    }

    // --------------------------------------------------------------设置页

    void UiConnector::connect_settings_page() {
        // --- 显示设置
        on_signal("showWeekendCheck", "toggled(bool)", [this]() {
            QObject* check = find("showWeekendCheck");
            m_bridge->set_show_weekend(check && check->property("checked").toBool());
        });
        on_signal("showOtherWeeksCheck", "toggled(bool)", [this]() {
            QObject* check = find("showOtherWeeksCheck");
            m_bridge->set_show_other_weeks(check && check->property("checked").toBool());
        });
        on_signal("slotDensitySelector", "currentIndexChanged()", [this]() {
            m_bridge->set_slot_density(combo_index(find("slotDensitySelector")));
        });
        on_signal("cardRadiusSelector", "currentIndexChanged()", [this]() {
            m_bridge->set_card_radius(combo_index(find("cardRadiusSelector")));
        });
        on_signal("themeSelector", "currentIndexChanged()", [this]() {
            const int index = combo_index(find("themeSelector"));
            m_bridge->set_theme(index == 1 ? QStringLiteral("light")
                                          : index == 2 ? QStringLiteral("dark")
                                                       : QStringLiteral("system"));
        });

        // --- 默认导入目录
        on_click("chooseImportDirButton", [this]() {
            if (QObject* dialog = find("importDirDialog")) {
                dialog->setProperty("currentFolder", local_url(text_of("importDirField")));
                invoke(dialog, "open");
            }
        });
        on_signal("importDirDialog", "accepted()", [this]() {
            set_text("importDirField", local_path_property(find("importDirDialog"), "selectedFolder"));
        });

        // --- 默认导出目录
        on_click("chooseExportDirButton", [this]() {
            if (QObject* dialog = find("exportDirDialog")) {
                dialog->setProperty("currentFolder", local_url(text_of("exportDirField")));
                invoke(dialog, "open");
            }
        });
        on_signal("exportDirDialog", "accepted()", [this]() {
            set_text("exportDirField", local_path_property(find("exportDirDialog"), "selectedFolder"));
        });

        on_click("saveDirsButton", [this]() {
            auto* io = import_export();
            io->set_default_import_dir(local_url(text_of("importDirField")));
            io->set_default_export_dir(local_url(text_of("exportDirField")));
        });

        on_click("resetDirsButton", [this]() {
            auto* io = import_export();
            io->reset_default_directories();
            set_text("importDirField", io->default_import_dir());
            set_text("exportDirField", io->default_export_dir());
        });

        // --- 作息表
        on_signal("slotSelector", "currentIndexChanged()", [this]() {
            const QVariantList periods = m_bridge->time_slots();
            const int index = combo_index(find("slotSelector"));
            if (index < 0 || index >= periods.size()) {
                return;
            }
            const QVariantMap period = periods.at(index).toMap();
            set_text("slotLabelField", period.value(QStringLiteral("label")).toString());
            set_text("slotStartField", period.value(QStringLiteral("start")).toString());
            set_text("slotEndField", period.value(QStringLiteral("end")).toString());
        });

        on_click("saveSlotButton", [this]() {
            QObject* slot_selector = find("slotSelector");
            const int index = combo_value(slot_selector).toInt();
            m_bridge->save_time_slot(index, text_of("slotLabelField"), text_of("slotStartField"), text_of("slotEndField"));
        });

        on_click("resetSlotsButton", [this]() {
            m_bridge->reset_time_slots_to_default();
            // 恢复默认后把第一个节次回填到表单
            const QVariantList periods = m_bridge->time_slots();
            if (!periods.isEmpty()) {
                const QVariantMap period = periods.first().toMap();
                set_text("slotLabelField", period.value(QStringLiteral("label")).toString());
                set_text("slotStartField", period.value(QStringLiteral("start")).toString());
                set_text("slotEndField", period.value(QStringLiteral("end")).toString());
            }
        });

        // --- 数据维护
        on_click("reloadButton", [this]() { m_bridge->reload_from_repository(); });
        on_click("saveNowButton", [this]() { m_bridge->save_to_repository(); });
        on_click("exportLogsButton", [this]() {
            auto* io = import_export();
            const QString target = QDir(io->default_export_dir()).filePath(QStringLiteral("logs"));
            QString error;
            const QString path = Logging::export_logs(target, &error);
            if (path.isEmpty()) {
                m_bridge->post_error(QStringLiteral("导出日志失败：%1").arg(error));
            }
            else {
                m_bridge->post_info(QStringLiteral("日志已导出：%1").arg(path));
            }
        });
    }

    // ----------------------------------------------------------课程编辑器

    void UiConnector::connect_course_editor() {
        on_click("courseCancelButton", [this]() { invoke(m_course_editor, "close"); });
        on_click("courseSaveButton", [this]() { save_course_from_editor(); });

        on_click("sessionAddButton", [this]() { append_session_row(session_row_from_form()); });

        on_click("sessionUpdateButton", [this]() {
            QObject* list = find("sessionList");
            const int row = list ? list->property("currentIndex").toInt() : -1;
            if (row >= 0) {
                update_session_row(row, session_row_from_form());
            }
        });

        on_click("sessionRemoveButton", [this]() {
            QObject* list = find("sessionList");
            QObject* draft = find("sessionDraftModel");
            if (!list || !draft) {
                return;
            }
            const int row = list->property("currentIndex").toInt();
            if (row >= 0) {
                // QQmlListModel::remove 只接受 QQmlV4FunctionPtr，改走 QML 侧的 removeDraft(index) 封装
                QMetaObject::invokeMethod(draft, "removeDraft", Q_ARG(QVariant, row));
            }
        });

        on_signal("sessionList", "currentIndexChanged()", [this]() {
            QObject* list = find("sessionList");
            load_session_row_into_form(list ? list->property("currentIndex").toInt() : -1);
        });
    }

    void UiConnector::connect_course_detail() {
        // 详情窗只是入口：先关闭详情窗，再复用既有的课程编辑器
        on_click("courseDetailEditButton", [this]() {
            const QString course_id = m_detail_course_id;
            invoke(m_course_detail, "close");
            if (!course_id.isEmpty()) {
                open_course_editor(course_id);
            }
        });

        on_signal("courseDetailReminderCheck", "toggled(bool)", [this]() {
            QObject* check = find("courseDetailReminderCheck");
            if (m_notifications && !m_detail_course_id.isEmpty()) {
                m_notifications->set_course_reminder_enabled(m_detail_course_id, check && check->property("checked").toBool());
            }
        });

        on_click("courseDetailCopyButton", [this]() {
            const QString course_id = m_detail_course_id;
            invoke(m_course_detail, "close");
            if (!course_id.isEmpty()) {
                open_course_editor(course_id, true);
            }
        });

        on_click("courseDetailCloseButton", [this]() { invoke(m_course_detail, "close"); });
    }

    void UiConnector::connect_overflow_menu() {
        on_click("moreMenuButton", [this]() {
            if (QObject* menu = find("moreMenu")) {
                invoke(menu, "open");
            }
        });
        on_signal("addCourseMenuItem", "triggered()", [this]() { open_course_editor(QString()); });
        on_signal("importMenuItem", "triggered()", [this]() {
            auto* io = import_export();
            set_property(m_import_wizard, "initialDirectory", io->last_import_dir());
            invoke(m_import_wizard, "open");
        });
        on_signal("exportMenuItem", "triggered()", [this]() {
            auto* io = import_export();
            set_text("exportDirField", io->default_export_dir());
            invoke(m_export_dialog, "open");
        });
        on_signal("shareImageMenuItem", "triggered()", [this]() { save_week_image(true); });
    }

    void UiConnector::connect_import_wizard() {
        on_click("importButton", [this]() {
            auto* io = import_export();
            set_property(m_import_wizard, "initialDirectory", io->last_import_dir());
            invoke(m_import_wizard, "open");
        });

        on_click("importChooseFileButton", [this]() {
            auto* io = import_export();
            if (QObject* dialog = find("importFileDialog")) {
                dialog->setProperty("currentFolder", local_url(io->last_import_dir()));
                invoke(dialog, "open");
            }
        });

        // 文件选择对话框属于 UI 层，选完后把路径交给数据层解析
        on_signal("importFileDialog", "accepted()", [this]() {
            auto* io = import_export();
            QObject* dialog = find("importFileDialog");
            const QUrl url = dialog ? dialog->property("selectedFile").toUrl() : QUrl();
            set_text("importFileField", url.toLocalFile());
            io->preview_import(url);
        });

        on_click("importApplyButton", [this]() {
            auto* io = import_export();
            io->apply_import(combo_index(find("importStrategySelector")));
            invoke(m_import_wizard, "close");
        });

        on_signal("importFromWebButton", "clicked()", [this]() { open_browser_import(); });

        on_click("importCancelButton", [this]() {
            auto* io = import_export();
            io->cancel_import();
            invoke(m_import_wizard, "close");
        });
    }

    void UiConnector::open_browser_import() {
        // 从导入向导进入时默认落在第一个已适配学校的直达入口；
        // 内置浏览器（index 0）没有预设地址，直接打开只会得到 about:blank。
        int entry_index = 0;
        const QVariantList entries = m_bridge->import_export()->browser_entries();
        for (int i = 1; i < entries.size(); ++i) {
            if (!entries.at(i).toMap().value(QStringLiteral("url")).toString().trimmed().isEmpty()) {
                entry_index = i;
                break;
            }
        }
        open_browser_import_at(entry_index);
    }

    void UiConnector::open_browser_import_at(int entry_index) {
        if (!m_browser_dialog) {
            return;
        }
        // 入口下标有效时先同步到对话框的选择器（会触发 currentIndexChanged 回填地址栏），
        // 这样从设置页进入时能直接落在用户选中的那个教务入口上。
        if (entry_index >= 0) {
            set_property(find("browserEntrySelector"), "currentIndex", entry_index);
        }
        sync_browser_url_field();
        invoke(m_browser_dialog, "open");
        // 打开即导航到当前入口，省去用户再点一次「打开此入口」
        navigate_browser_to_entry();
    }

    void UiConnector::resize_browser_dialog() {
        if (!m_browser_dialog || !m_browser_dialog->property("visible").toBool()) {
            return;
        }
        auto* window = qobject_cast<QQuickWindow*>(find("mainWindow"));
        if (!window) {
            return;
        }
        if (!m_browser_dialog->property("fullScreenMode").toBool()) {
            return;
        }
        m_browser_dialog->setProperty("width", window->width());
        m_browser_dialog->setProperty("height", window->height());
        m_browser_dialog->setProperty("x", 0);
        m_browser_dialog->setProperty("y", 0);
        qInfo() << "[browser] 对话框跟随窗口尺寸" << window->width() << "x" << window->height();
    }

    void UiConnector::connect_browser_import() {
        // 切换入口 → 回填地址栏（不自动导航，避免选中即请求）
        on_signal("browserEntrySelector", "currentIndexChanged()", [this]() { sync_browser_url_field(); });

        on_click("browserOpenEntryButton", [this]() { navigate_browser_to_entry(); });
        on_click("browserGoButton", [this]() { navigate_browser_to_entry(); });

        on_click("browserReloadButton", [this]() {
            if (QObject* browser = find("scheduleBrowser")) {
                QMetaObject::invokeMethod(browser, "reload");
            }
        });
        on_click("browserCompactReloadButton", [this]() {
            if (QObject* browser = find("scheduleBrowser")) {
                QMetaObject::invokeMethod(browser, "reload");
            }
        });

        // 系统浏览器兜底：不参与抓取，也不需要任何凭证
        on_click("browserSystemOpenButton", [this]() {
            const QString url = text_of("browserUrlField").trimmed();
            if (url.isEmpty()) {
                return;
            }
            QDesktopServices::openUrl(QUrl(url));
        });

        on_click("browserImportButton", [this]() {
            if (QObject* browser = find("scheduleBrowser")) {
                QMetaObject::invokeMethod(browser, "grabTimetable");
            }
        });

        on_click("browserCloseButton", [this]() { invoke(m_browser_dialog, "close"); });

        // 窗口尺寸变化（横竖屏切换）后，Popup 自己不会重新按父级尺寸布局，
        // 必须显式把全屏对话框的宽高同步到新窗口尺寸，否则会保留竖屏高度、底部被截掉。
        if (auto* window = qobject_cast<QQuickWindow*>(find("mainWindow"))) {
            connect(window, &QWindow::widthChanged, this, [this]() { resize_browser_dialog(); });
            connect(window, &QWindow::heightChanged, this, [this]() { resize_browser_dialog(); });
        }

        on_click("browserAddressButton", [this]() {
            if (!m_browser_dialog) {
                return;
            }
            const bool expanded = m_browser_dialog->property("addressVisible").toBool();
            m_browser_dialog->setProperty("addressVisible", !expanded);
            set_text("browserAddressButton", expanded ? QStringLiteral("地址") : QStringLiteral("收起"));
        });

        // 教务页面按横屏设计：打开对话框时自动横屏，关闭时恢复系统方向；
        // 顶栏的“横屏/竖屏”按钮允许用户手动切回。
        on_signal("browserImportDialog", "opened()", [this]() {
            m_browser_landscape = true;
            request_screen_orientation(0);
            set_text("browserRotateButton", QStringLiteral("竖屏"));
            if (m_browser_dialog) {
                m_browser_dialog->setProperty("addressVisible", false);
            }
            set_text("browserAddressButton", QStringLiteral("地址"));
            resize_browser_dialog();
            qInfo() << "[browser] 打开内嵌浏览器，请求横屏";
        });
        on_signal("browserImportDialog", "closed()", [this]() {
            m_browser_landscape = false;
            request_screen_orientation(-1);
            set_text("browserRotateButton", QStringLiteral("横屏"));
            qInfo() << "[browser] 关闭内嵌浏览器，恢复系统方向";
        });
        on_click("browserRotateButton", [this]() {
            m_browser_landscape = !m_browser_landscape;
            request_screen_orientation(m_browser_landscape ? 0 : 1);
            set_text("browserRotateButton", m_browser_landscape ? QStringLiteral("竖屏") : QStringLiteral("横屏"));
            // 窗口尺寸变化后 Popup 不会自动重排，等系统完成旋转再同步一次
            QTimer::singleShot(400, this, [this]() { resize_browser_dialog(); });
            qInfo() << "[browser] 手动切换方向" << (m_browser_landscape ? "横屏" : "竖屏");
        });

        // 设置页的简化入口：与导入向导共用同一个对话框，只是先同步选中的教务入口
        on_click("settingsAdapterImportButton", [this]() {
            open_browser_import_at(combo_index(find("settingsAdapterSelector")));
        });

        on_click("settingsAdapterSystemOpenButton", [this]() {
            const QVariantList entries = m_bridge->import_export()->browser_entries();
            const int index = combo_index(find("settingsAdapterSelector"));
            if (index < 0 || index >= entries.size()) {
                return;
            }
            const QString url = entries.at(index).toMap().value(QStringLiteral("url")).toString();
            if (!url.isEmpty()) {
                QDesktopServices::openUrl(QUrl(url));
            }
        });

        // 页面加载完成：自动注入「抓取课表」悬浮按钮
        on_signal("scheduleBrowser", "pageLoadingChanged()", [this]() {
            QObject* browser = find("scheduleBrowser");
            if (!browser) {
                return;
            }
            // 只在"从加载中变为加载完成"时注入
            if (browser->property("pageLoading").toBool()) {
                return;
            }
            if (!import_export()->has_embedded_browser()) {
                return;
            }
            QMetaObject::invokeMethod(browser, "injectCaptureButton");
        });

        // 抓取完成：页面原文经属性回传（dispatch() 不携带信号参数）
        on_signal("scheduleBrowser", "captureFinished(bool,QString)", [this]() {
            auto* io = import_export();
            QObject* browser = find("scheduleBrowser");
            if (!io || !browser) {
                return;
            }
            const QString html = browser->property("capturedPayload").toString();
            const QString source = browser->property("currentUrl").toString();
            io->submit_web_capture(html, source);
        });

        // 网页缩放：直接读写 scheduleBrowser.zoomFactor
        const auto adjust_zoom = [this](qreal delta) {
            QObject* browser = find("scheduleBrowser");
            if (!browser) {
                return;
            }
            const qreal current = browser->property("zoomFactor").toReal();
            const qreal next = qBound(0.5, current + delta, 3.0);
            browser->setProperty("zoomFactor", next);
        };

        on_click("browserZoomOutButton", [adjust_zoom]() { adjust_zoom(-0.1); });
        on_click("browserZoomInButton", [adjust_zoom]() { adjust_zoom(+0.1); });

        on_click("browserZoomResetButton", [this]() {
            if (QObject* browser = find("scheduleBrowser")) {
                browser->setProperty("zoomFactor", 1.0);
            }
        });
    }

    void UiConnector::sync_browser_url_field() {
        const QVariantList entries = m_bridge->import_export()->browser_entries();
        const int index = combo_index(find("browserEntrySelector"));
        if (index < 0 || index >= entries.size()) {
            return;
        }
        set_text("browserUrlField", entries.at(index).toMap().value(QStringLiteral("url")).toString());
    }

    void UiConnector::navigate_browser_to_entry() {
        QObject* browser = find("scheduleBrowser");
        if (!browser) {
            return;
        }
        const QString url = text_of("browserUrlField").trimmed();
        if (url.isEmpty()) {
            return;
        }
        QMetaObject::invokeMethod(browser, "loadUrl", Q_ARG(QVariant, QVariant(url)));
    }

    void UiConnector::connect_export_dialog() {
        on_click("exportButton", [this]() {
            auto* io = import_export();
            set_text("exportDirField", io->default_export_dir());
            invoke(m_export_dialog, "open");
        });

        on_click("exportChooseDirButton", [this]() {
            if (QObject* dialog = find("exportDirDialog")) {
                dialog->setProperty("currentFolder", local_url(text_of("exportDirField")));
                invoke(dialog, "open");
            }
        });

        on_signal("exportDirDialog", "accepted()", [this]() {
            set_text("exportDirField", local_path_property(find("exportDirDialog"), "selectedFolder"));
        });

        on_click("exportResetDirButton", [this]() {
            auto* io = import_export();
            set_text("exportDirField", io->default_export_dir());
        });

        on_click("exportConfirmButton", [this]() {
            auto* io = import_export();
            io->export_schedule(combo_index(find("exportFormatSelector")), local_url(text_of("exportDirField")));
        });

        on_click("exportCancelButton", [this]() { invoke(m_export_dialog, "close"); });
    }

    // ------------------------------------------------------------初值填充

    void UiConnector::prime_widgets() {
        // 学期表单
        if (m_bridge->has_semester()) {
            set_text("semesterNameField", m_bridge->semester_name());
            set_text("semesterStartField", m_bridge->semester_start_date());
            set_int("semesterWeeksSpin", m_bridge->total_weeks());
        }
        else {
            set_text("semesterNameField", QStringLiteral("我的课表"));
            set_text("semesterStartField", WeekCalculator::monday_of(QDate::currentDate()).toString(Qt::ISODate));
            set_int("semesterWeeksSpin", 20);
        }
        refresh_semester_form();

        // 设置页目录与首个节次
        auto* io = import_export();
        set_text("importDirField", io->default_import_dir());
        set_text("exportDirField", io->default_export_dir());
        set_text("logDirField", Logging::log_directory());

        const QVariantList periods = m_bridge->time_slots();
        if (!periods.isEmpty()) {
            const QVariantMap period = periods.first().toMap();
            set_text("slotLabelField", period.value(QStringLiteral("label")).toString());
            set_text("slotStartField", period.value(QStringLiteral("start")).toString());
            set_text("slotEndField", period.value(QStringLiteral("end")).toString());
        }
    }

    ImportExportBridge* UiConnector::import_export() const {
        return m_bridge ? m_bridge->import_export() : nullptr;
    }

    // --------------------------------------------------------编辑器的实现

    void UiConnector::show_course_detail(const QString& course_id) {
        if (course_id.isEmpty() || !m_course_detail) {
            return;
        }
        auto* model = qobject_cast<CourseListModel*>(m_bridge->course_model());
        const QVariantMap course = model ? model->find(course_id) : QVariantMap();
        if (course.isEmpty()) {
            return;
        }

        m_detail_course_id = course_id;
        set_property(m_course_detail, "course", course);
        set_property(m_course_detail, "sessions", course.value(QStringLiteral("sessions")));
        set_property(m_course_detail, "weekNote", m_bridge->course_note(course_id, m_bridge->selected_week()));
        set_property(m_course_detail, "reminderEnabled", m_notifications ? m_notifications->course_reminder_enabled(course_id) : true);
        invoke(m_course_detail, "open");
    }

    void UiConnector::open_course_editor(const QString& course_id, bool duplicate) {
        // 先清空表单，避免上一次编辑的残留
        set_text("editorNameField", QString());
        set_text("editorCodeField", QString());
        set_text("editorTeacherField", QString());
        set_text("editorLocationField", QString());
        set_text("editorColorField", QString());
        set_text("editorNotesField", QString());
        set_text("editorCreditsField", QStringLiteral("0"));
        clear_session_rows();

        QString resolved_id;
        if (!course_id.isEmpty()) {
            auto* model = qobject_cast<CourseListModel*>(m_bridge->course_model());
            const QVariantMap course = model ? model->find(course_id) : QVariantMap();
            if (!course.isEmpty()) {
                // 复制：保留全部字段与时间段，但清空 id、名称加“（副本）”，保存时创建新课程
                resolved_id = duplicate ? QString() : course_id;
                const QString course_name = course.value(QStringLiteral("name")).toString();
                set_text("editorNameField", duplicate ? QStringLiteral("%1（副本）").arg(course_name) : course_name);
                set_text("editorCodeField", course.value(QStringLiteral("code")).toString());
                set_text("editorTeacherField", course.value(QStringLiteral("teacher")).toString());
                set_text("editorLocationField", course.value(QStringLiteral("location")).toString());
                set_text("editorColorField", course.value(QStringLiteral("color")).toString());
                set_text("editorNotesField", course.value(QStringLiteral("notes")).toString());
                set_text("editorCreditsField", QString::number(course.value(QStringLiteral("credits")).toDouble(), 'g', 4));

                const QVariantList sessions = course.value(QStringLiteral("sessions")).toList();
                for (const QVariant& entry : sessions) {
                    append_session_row(entry.toMap());
                }
            }
        }

        set_property(m_course_editor, "editingCourseId", resolved_id);

        // 本周备注：存在时回填到备注框并勾选“仅保存到本周”；复制/新建不继承
        bool week_only = false;
        if (!course_id.isEmpty() && !duplicate) {
            const QString week_note = m_bridge->course_note(course_id, m_bridge->selected_week());
            if (!week_note.isEmpty()) {
                week_only = true;
                set_text("editorNotesField", week_note);
            }
        }
        set_property(find("editorNotesThisWeekCheck"), "checked", week_only);

        set_property(m_course_editor, "conflictHint", QString());

        // 表单默认值：周次填满整个学期，便于快速录入
        set_text("sessionWeeksField", QStringLiteral("1-%1").arg(qMax(1, m_bridge->total_weeks())));
        set_text("sessionLocationField", QString());
        set_text("sessionTeacherField", QString());
        set_int("sessionStartSpin", 1);
        set_int("sessionCountSpin", 2);

        invoke(m_course_editor, "open");
    }

    QVariantMap UiConnector::session_row_from_form() const {
        QObject* day_selector = find("sessionDaySelector");
        const int day_of_week = combo_value(day_selector).toInt();
        const int start_slot = int_of("sessionStartSpin", 1);
        const int slot_count = int_of("sessionCountSpin", 1);
        const QString weeks = text_of("sessionWeeksField");
        const QString location = text_of("sessionLocationField");
        const QString teacher = text_of("sessionTeacherField");

        const QString day_name = WeekCalculator::day_name(day_of_week);

        QVariantMap row;
        row.insert(QStringLiteral("sessionId"), QString());
        row.insert(QStringLiteral("dayOfWeek"), day_of_week);
        row.insert(QStringLiteral("dayName"), day_name);
        row.insert(QStringLiteral("startSlot"), start_slot);
        row.insert(QStringLiteral("slotCount"), qMax(1, slot_count));
        row.insert(QStringLiteral("endSlot"), start_slot + qMax(1, slot_count) - 1);
        row.insert(QStringLiteral("weeks"), weeks);
        row.insert(QStringLiteral("weeksDisplay"), weeks);
        row.insert(QStringLiteral("location"), location);
        row.insert(QStringLiteral("teacher"), teacher);
        row.insert(QStringLiteral("summary"), session_summary_text(row));
        return row;
    }

    void UiConnector::load_session_row_into_form(int row) {
        QObject* draft = find("sessionDraftModel");
        if (!draft || row < 0) {
            return;
        }

        QVariantMap data;
        QVariant result;
        // QQmlListModel::get 返回 QJSValue，C++ 接不了，改走 QML 侧的 rowDraft(index) 封装
        if (QMetaObject::invokeMethod(draft, "rowDraft", Q_RETURN_ARG(QVariant, result), Q_ARG(QVariant, row))) {
            data = result.toMap();
        }
        if (data.isEmpty()) {
            return;
        }

        set_int("sessionStartSpin", data.value(QStringLiteral("startSlot")).toInt());
        set_int("sessionCountSpin", data.value(QStringLiteral("slotCount")).toInt());
        set_text("sessionWeeksField", data.value(QStringLiteral("weeks")).toString());
        set_text("sessionLocationField", data.value(QStringLiteral("location")).toString());
        set_text("sessionTeacherField", data.value(QStringLiteral("teacher")).toString());

        if (QObject* day_selector = find("sessionDaySelector")) {
            const int day_of_week = data.value(QStringLiteral("dayOfWeek")).toInt();
            // sessionDaySelector 的模型是去掉“整周”后的 dayOptions，因此下标 = 星期 - 1
            day_selector->setProperty("currentIndex", qBound(0, day_of_week - 1, 6));
        }
    }

    void UiConnector::append_session_row(const QVariantMap& row) {
        QObject* draft = find("sessionDraftModel");
        if (!draft) {
            qWarning() << "[editor] 缺少 sessionDraftModel，无法添加时间段";
            return;
        }

        // 同一 星期 + 起始节次 + 连续节数 + 周次 视为同一时间段：
        // 重复点击“添加时间段”时选中已有行并提示，避免累积重复行后被冲突检测报告成多处问题。
        const int existing_rows = session_row_count();
        for (int index = 0; index < existing_rows; ++index) {
            const QVariantMap existing = session_row_at(index);
            if (existing.isEmpty()) {
                continue;
            }
            const bool same = existing.value(QStringLiteral("dayOfWeek")).toInt() == row.value(QStringLiteral("dayOfWeek")).toInt()
                && existing.value(QStringLiteral("startSlot")).toInt() == row.value(QStringLiteral("startSlot")).toInt()
                && existing.value(QStringLiteral("slotCount")).toInt() == row.value(QStringLiteral("slotCount")).toInt()
                && existing.value(QStringLiteral("weeks")).toString().trimmed() == row.value(QStringLiteral("weeks")).toString().trimmed();
            if (!same) {
                continue;
            }
            set_int("sessionList", index);
            if (QObject* list = find("sessionList")) {
                QMetaObject::invokeMethod(list, "positionViewAtEnd");
            }
            set_property(m_course_editor, "conflictHint",
                QStringLiteral("该时间段已在列表中（第 %1 行），已为你选中；修改后点“更新选中时间段”").arg(index + 1));
            if (m_course_editor) {
                QMetaObject::invokeMethod(m_course_editor, "scrollToBottom");
            }
            qInfo().noquote() << "[editor] 重复时间段，选中已有行" << index << existing.value(QStringLiteral("summary")).toString();
            return;
        }

        QVariantMap entry = row;
        if (entry.value(QStringLiteral("summary")).toString().isEmpty()) {
            entry.insert(QStringLiteral("summary"), session_summary_text(entry));
        }
        if (!QMetaObject::invokeMethod(draft, "appendDraft", Q_ARG(QVariant, QVariant(entry)))) {
            set_property(m_course_editor, "conflictHint", QStringLiteral("添加时间段失败，请查看日志"));
            qWarning() << "[editor] 无法向列表模型追加时间段草稿";
            return;
        }

        const int new_index = session_row_count() - 1;
        set_int("sessionList", new_index);
        if (QObject* list = find("sessionList")) {
            QMetaObject::invokeMethod(list, "positionViewAtEnd");
        }
        set_property(m_course_editor, "conflictHint", QString());
        if (m_course_editor) {
            QMetaObject::invokeMethod(m_course_editor, "scrollToBottom");
        }
        qInfo().noquote() << "[editor] 已添加时间段" << new_index << entry.value(QStringLiteral("summary")).toString();
    }

    QVariantMap UiConnector::session_row_at(int row) const {
        QObject* draft = find("sessionDraftModel");
        if (!draft || row < 0) {
            return QVariantMap();
        }
        QVariant result;
        if (QMetaObject::invokeMethod(draft, "rowDraft", Q_RETURN_ARG(QVariant, result), Q_ARG(QVariant, row))) {
            return result.toMap();
        }
        return QVariantMap();
    }


    void UiConnector::update_session_row(int row, const QVariantMap& row_data) {
        QObject* draft = find("sessionDraftModel");
        if (!draft) {
            return;
        }
        // 同上：QQmlListModel::set 收 QJSValue，改走 QML 侧的 updateDraft(index, row) 封装
        if (!QMetaObject::invokeMethod(draft, "updateDraft", Q_ARG(QVariant, row), Q_ARG(QVariant, QVariant(row_data)))) {
            qWarning() << "[app] 无法更新时间段草稿";
        }
    }

    void UiConnector::clear_session_rows() {
        if (QObject* draft = find("sessionDraftModel")) {
            invoke(draft, "clear");
        }
    }

    int UiConnector::session_row_count() const {
        QObject* draft = find("sessionDraftModel");
        return draft ? draft->property("count").toInt() : 0;
    }

    void UiConnector::save_course_from_editor() {
        QVariantMap data;
        data.insert(QStringLiteral("courseId"), m_course_editor ? m_course_editor->property("editingCourseId").toString() : QString());
        data.insert(QStringLiteral("name"), text_of("editorNameField"));
        data.insert(QStringLiteral("code"), text_of("editorCodeField"));
        data.insert(QStringLiteral("teacher"), text_of("editorTeacherField"));
        data.insert(QStringLiteral("location"), text_of("editorLocationField"));
        data.insert(QStringLiteral("color"), text_of("editorColorField"));
        data.insert(QStringLiteral("credits"), text_of("editorCreditsField").toDouble());

        // “仅保存到本周”：保留课程本身的备注不动，备注写入 course_note 设置项
        const QString editing_id = m_course_editor ? m_course_editor->property("editingCourseId").toString() : QString();
        QObject* week_check = find("editorNotesThisWeekCheck");
        const bool week_only = week_check && week_check->property("checked").toBool() && !editing_id.isEmpty();
        const QString note_text = text_of("editorNotesField");
        if (week_only) {
            auto* model = qobject_cast<CourseListModel*>(m_bridge->course_model());
            const QVariantMap existing = model ? model->find(editing_id) : QVariantMap();
            data.insert(QStringLiteral("notes"), existing.value(QStringLiteral("notes")).toString());
        } else {
            data.insert(QStringLiteral("notes"), note_text);
        }

        // 时间段来自 QML 的草稿列表模型
        QVariantList sessions;
        const int rows = session_row_count();
        qInfo().noquote() << "[editor] 保存课程" << data.value(QStringLiteral("name")).toString() << "，时间段数" << rows;
        for (int row = 0; row < rows; ++row) {
            const QVariantMap row_data = session_row_at(row);
            if (row_data.isEmpty()) {
                qWarning() << "[editor] 时间段行读取失败" << row;
                continue;
            }
            sessions.append(row_data);
            qInfo().noquote() << "[editor]   时间段" << row
                              << row_data.value(QStringLiteral("summary")).toString()
                              << "周次" << row_data.value(QStringLiteral("weeks")).toString();
        }
        data.insert(QStringLiteral("sessions"), sessions);

        // 只有在业务层确实写入成功时才关闭对话框，以免校验失败时用户丢失已填内容
        const bool saved = m_bridge->save_course(data);
        qInfo().noquote() << "[editor] 保存结果" << saved;
        if (saved) {
            if (week_only) {
                m_bridge->set_course_note(editing_id, m_bridge->selected_week(), note_text);
            }
            invoke(m_course_editor, "close");
        }
    }

    // ----------------------------------------------------------教务适配器

    void UiConnector::connect_school_adapters() {
        // 切换适配器时把该适配器当前的地址回填到表单
        on_signal("adapterSelector", "currentIndexChanged()", [this]() {
            QObject* selector = find("adapterSelector");
            const int index = combo_index(selector);
            const QVariantList options = m_bridge->import_export()->adapter_options();
            if (index < 0 || index >= options.size()) {
                return;
            }
            const QVariantMap option = options.at(index).toMap();
            set_text("adapterScheduleUrlField", option.value(QStringLiteral("scheduleUrl")).toString());
            set_text("adapterLoginUrlField", option.value(QStringLiteral("loginUrl")).toString());
        });

        on_click("adapterSaveUrlButton", [this]() {
            auto* io = import_export();
            io->save_adapter_endpoints(combo_index(find("adapterSelector")),
                text_of("adapterScheduleUrlField"),
                text_of("adapterLoginUrlField"));
        });

        // 仅在此处把 Cookie 交给桥接对象：桥接只保留在内存中，不写库、不写日志
        on_click("adapterImportButton", [this]() {
            auto* io = import_export();
            io->import_from_adapter(combo_index(find("adapterSelector")), text_of("adapterCookieField"));
        });

        on_click("adapterClearSessionButton", [this]() {
            auto* io = import_export();
            io->clear_adapter_session();
            set_text("adapterCookieField", QString());
        });
    }

    // --------------------------------------------------------------提醒

    void UiConnector::connect_reminders() {
        if (!m_notifications) {
            return;
        }
        NotificationService* service = m_notifications;

        // 启用 / 停用：CheckBox 的 checked 由 QML 绑定到 reminders.enabled，
        // 用户点击后由 C++ 读回并写设置。
        on_click("reminderEnabledCheck", [this, service]() {
            QObject* check = find("reminderEnabledCheck");
            if (check) {
                service->set_enabled(check->property("checked").toBool());
            }
        });

        on_signal("reminderMinutesSelector", "currentIndexChanged()", [this, service]() {
            service->set_minutes_before(combo_value(find("reminderMinutesSelector")).toInt());
        });

        on_click("testNotificationButton", [service]() { service->show_test_notification(); });
        on_click("requestPermissionButton", [service]() { service->request_permission(); });

        // 系统通知不可用时，应用内横幅保证提醒不会丢失
        connect(service, &NotificationService::notificationRequested, this, [this](const QString& title, const QString& message) {
            // 横幅带「稍后提醒 / 知道了」，停留久一点
            show_banner(title, message, 15);
        });

        on_click("bannerSnoozeButton", [this, service]() {
            service->snooze(5);
            hide_banner();
        });
        on_click("bannerDismissButton", [this]() { hide_banner(); });
    }

    void UiConnector::show_banner(const QString& title, const QString& message, int seconds) {
        QObject* banner = find("notificationBanner");
        if (!banner) {
            return;
        }
        banner->setProperty("bannerTitle", title);
        banner->setProperty("bannerMessage", message);

        const int generation = ++m_banner_generation;
        QTimer::singleShot(seconds * 1000, this, [this, generation]() {
            if (generation != m_banner_generation) {
                return; // 已被更新的横幅取代
            }
            if (QObject* current = find("notificationBanner")) {
                current->setProperty("bannerTitle", QString());
                current->setProperty("bannerMessage", QString());
            }
        });
    }

    void UiConnector::hide_banner() {
        ++m_banner_generation;
        if (QObject* banner = find("notificationBanner")) {
            banner->setProperty("bannerTitle", QString());
            banner->setProperty("bannerMessage", QString());
        }
    }

    void UiConnector::save_week_image(bool share_after) {
        if (QObject* stack = find("pageStack")) {
            stack->setProperty("currentIndex", 0);
        }
        // 等 QML 完成页面切换与布局后再截图
        QTimer::singleShot(200, this, [this, share_after]() {
            auto* week = qobject_cast<QQuickItem*>(find("weekView"));
            if (!week) {
                m_bridge->post_error(QStringLiteral("找不到课表视图"));
                return;
            }
            auto result = week->grabToImage();
            if (!result) {
                m_bridge->post_error(QStringLiteral("课表截图失败"));
                return;
            }
            connect(result.data(), &QQuickItemGrabResult::ready, this, [this, result, share_after]() {
                const QString base = import_export() ? import_export()->default_export_dir() : QDir::currentPath();
                const QString dir = QDir(base).filePath(QStringLiteral("share"));
                QDir().mkpath(dir);
                const QString path = QDir(dir).filePath(QStringLiteral("timetable-week%1-%2.png")
                    .arg(m_bridge->selected_week())
                    .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"))));
                // 在网格上方合成一条标题带：学期名 + 周次，方便直接分享
                QImage source = result->image();
                // 抓图结果带 devicePixelRatio，直接 drawImage 会按 DPR 缩小；
                // 这里按原始像素绘制，保证分享图里的网格铺满宽度。
                source.setDevicePixelRatio(1.0);
                const bool dark = m_bridge->property("darkMode").toBool();
                const QColor surface = dark ? QColor(QStringLiteral("#151A23")) : QColor(Qt::white);
                const int band = qMax(120, source.height() / 12);
                QImage output(source.width(), source.height() + band, QImage::Format_ARGB32_Premultiplied);
                output.fill(surface);

                QPainter painter(&output);
                painter.fillRect(0, 0, output.width(), band, QColor(QStringLiteral("#4C8DFF")));
                painter.drawImage(0, band, source);

                QFont titleFont = painter.font();
                titleFont.setPixelSize(qMax(28, band / 3));
                titleFont.setBold(true);
                painter.setFont(titleFont);
                painter.setPen(Qt::white);
                painter.drawText(QRect(40, 8, output.width() - 80, band / 2), Qt::AlignLeft | Qt::AlignVCenter,
                    m_bridge->semester_name());

                QFont subFont = titleFont;
                subFont.setPixelSize(qMax(20, band / 5));
                subFont.setBold(false);
                painter.setFont(subFont);
                painter.setPen(QColor(QStringLiteral("#E8EDF5")));
                painter.drawText(QRect(40, band / 2, output.width() - 80, band / 2 - 8), Qt::AlignLeft | Qt::AlignVCenter,
                    QStringLiteral("第 %1 周 · %2").arg(m_bridge->selected_week()).arg(m_bridge->selected_week_range()));
                painter.end();

                if (output.save(path)) {
                    if (!share_after) {
                        m_bridge->post_info(QStringLiteral("课表图片已保存：%1").arg(path));
                    }
#if defined(Q_OS_ANDROID)
                    else if (share_image_android(path)) {
                        m_bridge->post_info(QStringLiteral("课表图片已保存，已打开分享面板：%1").arg(path));
                    }
                    else {
                        m_bridge->post_error(QStringLiteral("图片已保存，但打开分享面板失败：%1").arg(path));
                    }
#else
                    else {
                        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
                        m_bridge->post_info(QStringLiteral("课表图片已保存：%1").arg(path));
                    }
#endif
                }
                else {
                    m_bridge->post_error(QStringLiteral("保存课表图片失败"));
                }
            });
        });
    }

    // ------------------------------------------------------------动态卡片

    void UiConnector::prune_card_connections() {
        for (auto iterator = m_connected_cards.begin(); iterator != m_connected_cards.end();) {
            if (!*iterator) {
                iterator = m_connected_cards.erase(iterator);
            }
            else {
                ++iterator;
            }
        }
    }

    void UiConnector::connect_session_cards() {
        if (!m_root) {
            return;
        }
        prune_card_connections();

        // `Repeater` 生成的课卡经 `setParentItem()` 挂到可视树上，`QObject` 父对象为空，
        // 因此 `findChildren("sessionCardClick")` 恒为空，只能沿 `childItems()` 递归查找。
        const QList<QQuickItem*> hotspots = visual_hotspots(QStringLiteral("sessionCardClick"));
        for (QQuickItem* hotspot : hotspots) {
            // 课卡增删（模型重置 / 周次 / 星期切换）时重扫；同一容器只连一次
            if (QQuickItem* host = hotspot->parentItem()) {
                QObject::connect(host, &QQuickItem::childrenChanged,
                    this, &UiConnector::schedule_card_reconnect, Qt::UniqueConnection);
            }

            if (m_connected_cards.contains(hotspot)) {
                continue;
            }
            m_connected_cards.insert(hotspot);

            // 课卡的 courseId 由 QML 从当前 model 赋值，点击时读取即可
            on_signal(hotspot, "clicked()", [this, hotspot]() {
                show_course_detail(hotspot->property("courseId").toString());
            });
            // 模型重置会销毁旧卡片，及时清理映射，避免悬空键
            QObject::connect(hotspot, &QObject::destroyed, this, [this, hotspot]() {
                for (auto it = m_handlers.begin(); it != m_handlers.end();) {
                    if (it.key().first == hotspot) {
                        it = m_handlers.erase(it);
                    }
                    else {
                        ++it;
                    }
                }
                m_connected_cards.remove(hotspot);
            });
        }
    }

    QList<QQuickItem*> UiConnector::visual_hotspots(const QString& name) const {
        QList<QQuickItem*> result;
        collect_visual_children(root_item(), name, &result);
        return result;
    }

    QQuickItem* UiConnector::root_item() const {
        if (!m_root) {
            return nullptr;
        }
        // `ApplicationWindow` 是 `QQuickWindow`（不是 `QQuickItem`），可视树的根是它的 contentItem
        if (auto* item = qobject_cast<QQuickItem*>(m_root.data())) {
            return item;
        }
        return qobject_cast<QQuickItem*>(m_root->property("contentItem").value<QObject*>());
    }

    void UiConnector::connect_list_selection(
        const char* list_name, const char* hotspot_name, const char* index_property) {
        QObject* list = find(list_name);
        if (!list) {
            return;
        }
        auto* content = qobject_cast<QQuickItem*>(list->property("contentItem").value<QObject*>());
        if (!content) {
            return;
        }

        // ListView 的委托由 QQmlDelegateModel 创建，只挂在 contentItem 的**可视**子树下
        // （`QQuickItem::childItems()`），并不进入 `QObject::children()`；因此
        // `findChildren<QObject*>(hotspot_name)` 恒为空，必须沿可视子项遍历。
        // 委托会随滚动 / 模型重置动态增删，故在可视子项变化时重扫一次。
        QObject::connect(content, &QQuickItem::childrenChanged,
            this, &UiConnector::schedule_card_reconnect, Qt::UniqueConnection);

        const QString hotspot_object_name = QString::fromLatin1(hotspot_name);
        const QByteArray index_object_name(index_property);
        const QPointer<QObject> target_list(list);

        for (QQuickItem* delegate : content->childItems()) {
            QObject* hotspot = delegate->findChild<QObject*>(hotspot_object_name);
            if (!hotspot || m_connected_cards.contains(hotspot)) {
                continue;
            }
            m_connected_cards.insert(hotspot);

            // 行号由 QML 从委托的 index 赋值，点击时读取即可
            on_signal(hotspot, "clicked()", [target_list, hotspot, index_object_name]() {
                if (!target_list) {
                    return;
                }
                target_list->setProperty("currentIndex", hotspot->property(index_object_name.constData()).toInt());
            });
            // 委托被回收 / 重建时及时清理映射，避免悬空键
            QObject::connect(hotspot, &QObject::destroyed, this, [this, hotspot]() {
                for (auto it = m_handlers.begin(); it != m_handlers.end();) {
                    if (it.key().first == hotspot) {
                        it = m_handlers.erase(it);
                    }
                    else {
                        ++it;
                    }
                }
                m_connected_cards.remove(hotspot);
            });
        }
    }

    void UiConnector::schedule_card_reconnect() {
        if (m_reconnect_pending) {
            return;
        }
        m_reconnect_pending = true;
        // 模型 reset 与委托重建不在同一拍，延后一个事件循环再扫描
        QTimer::singleShot(0, this, [this]() {
            m_reconnect_pending = false;
            connect_session_cards();
            connect_list_selection("courseList", "courseListItemClick", "itemIndex");
            connect_list_selection("sessionList", "sessionRowClick", "rowIndex");
        });
    }

} // namespace Schedule
