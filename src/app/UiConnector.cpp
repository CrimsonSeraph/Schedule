#include "UiConnector.h"

#include "core/service/WeekCalculator.h"
#include "engine/CourseListModel.h"
#include "engine/NotificationService.h"

#include <QAbstractItemModel>
#include <QDebug>
#include <QMetaObject>
#include <QTimer>
#include <QUrl>
#include <QVariantList>

#include <functional>

namespace Schedule {

    namespace {

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

    // ---------------------------------------------------------------- 查找与读写

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

        m_handlers.insert(object, std::move(handler));

        // QML 控件的信号在公开 C++ 头文件中不可见，只能按元对象签名（"2xxx()"）连接；
        // 连接仍然发生在 C++ 侧，QML 中不出现任何信号处理器。
        const QByteArray signature = QByteArray("2") + signal;
        const bool connected = QObject::connect(object, signature.constData(), this, SLOT(dispatch()));
        if (connected) {
            ++m_connection_count;
        }
        else {
            qWarning() << "[app] 无法连接信号：" << name << signature;
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
        const auto iterator = m_handlers.constFind(object);
        if (iterator != m_handlers.constEnd() && *iterator) {
            (*iterator)();
        }
    }

    // -------------------------------------------------------------------- 入口

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
        m_import_wizard = find("importWizard");
        m_export_dialog = find("exportDialog");

        int count = 0;
        Q_UNUSED(count);

        connect_navigation();
        connect_week_navigation();
        connect_course_actions();
        connect_semester_page();
        connect_settings_page();
        connect_course_editor();
        connect_import_wizard();
        connect_export_dialog();
        connect_reminders();

        prime_widgets();

        // 模型重置后 Repeater 会重建卡片，需要重新连接热区
        if (QAbstractItemModel* course_model = m_bridge->course_model()) {
            connect(course_model, &QAbstractItemModel::modelReset, this, [this]() { schedule_card_reconnect(); });
        }
        if (QAbstractItemModel* week_model = m_bridge->week_model()) {
            connect(week_model, &QAbstractItemModel::modelReset, this, [this]() { schedule_card_reconnect(); });
        }
        if (QAbstractItemModel* session_model = m_bridge->session_model()) {
            connect(session_model, &QAbstractItemModel::modelReset, this, [this]() { schedule_card_reconnect(); });
        }
        connect(m_bridge, &ScheduleBridge::selectedWeekChanged, this, [this]() { schedule_card_reconnect(); });
        connect(m_bridge, &ScheduleBridge::selectedDayChanged, this, [this]() { schedule_card_reconnect(); });

        schedule_card_reconnect();
        qInfo() << "[app] UiConnector: 已建立" << m_connection_count << "个 QML ↔ C++ 连接";
        return m_connection_count;
    }

    // -------------------------------------------------------------------- 导航

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
            const int week = combo_value(find("weekSelector")).toInt();
            if (week > 0) {
                m_bridge->select_week(week);
            }
        });

        on_signal("daySelector", "currentIndexChanged()", [this]() {
            m_bridge->select_day(combo_value(find("daySelector")).toInt());
        });
    }

    // ---------------------------------------------------------------- 课程操作

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
    }

    void UiConnector::connect_semester_page() {
        on_click("saveSemesterButton", [this]() {
            const QString name = text_of("semesterNameField");
            const QString start = text_of("semesterStartField");
            const int weeks = int_of("semesterWeeksSpin", 20);

            if (m_bridge->has_semester()) {
                m_bridge->update_semester(name, start, weeks);
            }
            else {
                m_bridge->create_semester(name, start, weeks);
            }
        });
    }

    // ------------------------------------------------------------------ 设置页

    void UiConnector::connect_settings_page() {
        ImportExportBridge* io = m_bridge->import_export();

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

        on_click("saveDirsButton", [this, io]() {
            io->set_default_import_dir(local_url(text_of("importDirField")));
            io->set_default_export_dir(local_url(text_of("exportDirField")));
        });

        on_click("resetDirsButton", [this, io]() {
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
    }

    // -------------------------------------------------------------- 课程编辑器

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
                QMetaObject::invokeMethod(draft, "remove", Q_ARG(int, row), Q_ARG(int, 1));
            }
        });

        on_signal("sessionList", "currentIndexChanged()", [this]() {
            QObject* list = find("sessionList");
            load_session_row_into_form(list ? list->property("currentIndex").toInt() : -1);
        });
    }

    void UiConnector::connect_import_wizard() {
        ImportExportBridge* io = m_bridge->import_export();

        on_click("importButton", [this, io]() {
            set_property(m_import_wizard, "initialDirectory", io->last_import_dir());
            invoke(m_import_wizard, "open");
        });

        on_click("importChooseFileButton", [this, io]() {
            if (QObject* dialog = find("importFileDialog")) {
                dialog->setProperty("currentFolder", local_url(io->last_import_dir()));
                invoke(dialog, "open");
            }
        });

        // 文件选择对话框属于 UI 层，选完后把路径交给数据层解析
        on_signal("importFileDialog", "accepted()", [this, io]() {
            QObject* dialog = find("importFileDialog");
            const QUrl url = dialog ? dialog->property("selectedFile").toUrl() : QUrl();
            set_text("importFileField", url.toLocalFile());
            io->preview_import(url);
        });

        on_click("importApplyButton", [this, io]() {
            io->apply_import(combo_index(find("importStrategySelector")));
            invoke(m_import_wizard, "close");
        });

        on_click("importCancelButton", [this, io]() {
            io->cancel_import();
            invoke(m_import_wizard, "close");
        });
    }

    void UiConnector::connect_export_dialog() {
        ImportExportBridge* io = m_bridge->import_export();

        on_click("exportButton", [this, io]() {
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

        on_click("exportResetDirButton", [this, io]() {
            set_text("exportDirField", io->default_export_dir());
        });

        on_click("exportConfirmButton", [this, io]() {
            io->export_schedule(combo_index(find("exportFormatSelector")), local_url(text_of("exportDirField")));
        });

        on_click("exportCancelButton", [this]() { invoke(m_export_dialog, "close"); });
    }

    // ---------------------------------------------------------------- 初值填充

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

        // 设置页目录与首个节次
        ImportExportBridge* io = m_bridge->import_export();
        set_text("importDirField", io->default_import_dir());
        set_text("exportDirField", io->default_export_dir());

        const QVariantList periods = m_bridge->time_slots();
        if (!periods.isEmpty()) {
            const QVariantMap period = periods.first().toMap();
            set_text("slotLabelField", period.value(QStringLiteral("label")).toString());
            set_text("slotStartField", period.value(QStringLiteral("start")).toString());
            set_text("slotEndField", period.value(QStringLiteral("end")).toString());
        }
    }

    // ------------------------------------------------------------ 编辑器的实现

    void UiConnector::open_course_editor(const QString& course_id) {
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
                resolved_id = course_id;
                set_text("editorNameField", course.value(QStringLiteral("name")).toString());
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
        QString summary = QStringLiteral("%1 第 %2-%3 节 · %4")
                              .arg(day_name)
                              .arg(start_slot)
                              .arg(start_slot + qMax(1, slot_count) - 1)
                              .arg(weeks);
        if (!location.isEmpty()) {
            summary += QStringLiteral(" · ") + location;
        }

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
        row.insert(QStringLiteral("summary"), summary);
        return row;
    }

    void UiConnector::load_session_row_into_form(int row) {
        QObject* draft = find("sessionDraftModel");
        if (!draft || row < 0) {
            return;
        }

        QVariantMap data;
        QVariant result;
        if (QMetaObject::invokeMethod(draft, "get", Q_RETURN_ARG(QVariant, result), Q_ARG(int, row))) {
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
            return;
        }
        // QML ListModel 的 append 在不同 Qt 版本中参数类型可能是 QVariantMap 或 QVariant，
        // 这里两种签名都尝试一次，避免版本差异导致静默失败。
        if (!QMetaObject::invokeMethod(draft, "append", Q_ARG(QVariantMap, row))) {
            if (!QMetaObject::invokeMethod(draft, "append", Q_ARG(QVariant, QVariant(row)))) {
                qWarning() << "[app] 无法向列表模型追加时间段草稿";
            }
        }
    }

    void UiConnector::update_session_row(int row, const QVariantMap& row_data) {
        QObject* draft = find("sessionDraftModel");
        if (!draft) {
            return;
        }
        if (!QMetaObject::invokeMethod(draft, "set", Q_ARG(int, row), Q_ARG(QVariantMap, row_data))) {
            if (!QMetaObject::invokeMethod(draft, "set", Q_ARG(int, row), Q_ARG(QVariant, QVariant(row_data)))) {
                qWarning() << "[app] 无法更新时间段草稿";
            }
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
        data.insert(QStringLiteral("notes"), text_of("editorNotesField"));
        data.insert(QStringLiteral("credits"), text_of("editorCreditsField").toDouble());

        // 时间段来自 QML 的草稿列表模型
        QVariantList sessions;
        QObject* draft = find("sessionDraftModel");
        const int rows = session_row_count();
        for (int row = 0; row < rows; ++row) {
            if (!draft) {
                break;
            }
            QVariant result;
            if (QMetaObject::invokeMethod(draft, "get", Q_RETURN_ARG(QVariant, result), Q_ARG(int, row))) {
                sessions.append(result.toMap());
            }
        }
        data.insert(QStringLiteral("sessions"), sessions);

        // 只有在业务层确实写入成功时才关闭对话框，以免校验失败时用户丢失已填内容
        if (m_bridge->save_course(data)) {
            invoke(m_course_editor, "close");
        }
    }

    // ------------------------------------------------------------------ 提醒

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
            show_banner(title, message);
        });
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

    // ---------------------------------------------------------------- 动态卡片

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

        const QList<QObject*> cards = m_root->findChildren<QObject*>(QStringLiteral("sessionCardClick"));
        for (QObject* card : cards) {
            if (!card || m_connected_cards.contains(card)) {
                continue;
            }
            m_connected_cards.insert(card);

            // 课卡的 courseId 由 QML 从当前 model 赋值，点击时读取即可
            m_handlers.insert(card, [this, card]() {
                const QString course_id = card->property("courseId").toString();
                if (!course_id.isEmpty()) {
                    open_course_editor(course_id);
                }
            });
            QObject::connect(card, SIGNAL(clicked()), this, SLOT(dispatch()));
            // 模型重置会销毁旧卡片，及时清理映射，避免悬空键
            QObject::connect(card, &QObject::destroyed, this, [this, card]() {
                m_handlers.remove(card);
                m_connected_cards.remove(card);
            });
        }
    }

    void UiConnector::schedule_card_reconnect() {
        if (m_reconnect_pending) {
            return;
        }
        m_reconnect_pending = true;
        // 模型 reset 与 Repeater 重建不在同一拍，延后一个事件循环再扫描
        QTimer::singleShot(0, this, [this]() {
            m_reconnect_pending = false;
            connect_session_cards();
        });
    }

} // namespace Schedule
