#pragma once

#include "engine/AppBridge.h"
#include "engine/NotificationService.h"
#include "engine/ScheduleBridge.h"

#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QVariantMap>

#include <functional>

class QQuickItem;

namespace Schedule {

    /**
     * @brief QML 交互连接的集中管理对象（**唯一**建立 QML ↔ C++ 连接的地方）。
     *
     * 项目规范要求 QML 中不出现 `onClicked` / `Connections` 等按名称隐式连接的写法，
     * 因此每个交互控件都通过 `objectName` 暴露给 C++，由本类在 `engine.load()`
     * 之后统一 `findChild` + `QObject::connect`。
     *
     * 覆盖的连接：
     *
     * | 区域 | 控件（objectName） | 行为 |
     * | ---- | ------------------ | ---- |
     * | 导航 | `navWeekButton` / `navDayButton` / `navSemesterButton` / `navSettingsButton` | 切换 `pageStack.currentIndex` |
     * | 周次 | `prevWeekButton` / `nextWeekButton` / `currentWeekButton` / `weekSelector` | 调用 `ScheduleBridge` 的周次槽 |
     * | 星期 | `daySelector` | 调用 `select_day()` |
     * | 课程 | `addCourseButton` / `pageNewCourseButton` / `editCourseButton` / `deleteCourseButton` | 打开编辑器 / 删除选中课程 |
     * | 学期 | `saveSemesterButton` | 创建或更新学期 |
     * | 设置 | `chooseImportDirButton` / `chooseExportDirButton` / `saveDirsButton` / `resetDirsButton` / `slotSelector` / `saveSlotButton` / `resetSlotsButton` / `reloadButton` / `saveNowButton` | 目录与作息表设置、数据维护 |
     * | 编辑器 | `courseSaveButton` / `courseCancelButton` / `sessionAddButton` / `sessionUpdateButton` / `sessionRemoveButton` / `sessionList` | 课程与时间段编辑 |
     * | 时间段草稿 | `sessionRowClick`（`sessionList` 委托内的热区） | 点击草稿行设置 `sessionList.currentIndex` |
     * | 导入 | `importButton` / `importChooseFileButton` / `importFileDialog` / `importApplyButton` / `importCancelButton` | 导入向导 |
     * | 导出 | `exportButton` / `exportChooseDirButton` / `exportResetDirButton` / `exportDirDialog` / `exportConfirmButton` / `exportCancelButton` | 导出对话框 |
     * | 动态卡片 | `sessionCardClick`（周 / 日视图内由 `Repeater` 生成） | 点击课卡打开课程详情弹层 |
     * | 课程详情 | `courseDetailEditButton` / `courseDetailCloseButton` | 进入课程编辑器 / 关闭详情弹层 |
     * | 课程列表 | `courseListItemClick`（学期页 `ListView` 的委托） | 点击列表项设置 `courseList.currentIndex` |
     *
     * `Repeater` 生成的课卡与 `ListView` 的委托都会被 `setParentItem()` 挂进可视树，
     * 其 `QObject` 父对象为空，`findChildren()` 扫不到，因此本类沿 `QQuickItem::childItems()`
     * 查找热区；委托增删由承载容器的 `childrenChanged` 或模型 reset 触发重扫
     * （见 `schedule_card_reconnect()`）。
     */
    class UiConnector : public QObject {
        Q_OBJECT

    public:
        /**
         * @param root          QML 根对象（`engine.rootObjects().value(0)`）
         * @param bridge        课表桥接（不持有所有权）
         * @param app_bridge    早期桥接对象，承载“测试”按钮（不持有所有权）
         * @param notifications 提醒服务（不持有所有权），可为空
         * @param parent        父对象
         */
        UiConnector(QObject* root,
            ScheduleBridge* bridge,
            AppBridge* app_bridge,
            NotificationService* notifications = nullptr,
            QObject* parent = nullptr);

        ~UiConnector() override;

        /**
         * @brief 建立全部连接并做一次界面初值填充。
         * @return 成功建立的关键连接数量（用于日志与自检）
         */
        int connect_all();

    private:
        // ------------------------------------------------------------ 查找与读写

        /** @return 按 objectName 查找子对象；未找到返回 nullptr。 */
        QObject* find(const char* name) const;

        /** @return 控件文本；控件不存在返回空串。 */
        QString text_of(const char* name) const;

        /** @brief 设置控件文本；控件不存在时静默跳过（移动端 / 桌面端控件集不同）。 */
        void set_text(const char* name, const QString& text);

        /** @return 控件整型属性值；控件不存在返回 fallback。 */
        int int_of(const char* name, int fallback = 0) const;

        /** @brief 设置控件整型属性。 */
        void set_int(const char* name, int value);

        /** @brief 设置任意属性。 */
        void set_property(QObject* object, const char* name, const QVariant& value);

        /** @brief 调用无参可调用方法（如 `open()` / `close()`）。 */
        static bool invoke(QObject* object, const char* method);

        // ---------------------------------------------------------------- 连接

        /** @brief 把一个具名控件的 `clicked()` 连接到处理函数。 */
        bool on_click(const char* name, std::function<void()> handler);

        /**
         * @brief 把一个具名控件的**无参信号**连接到统一分发槽。
         *
         * QML 控件的信号在公开 C++ 头文件中不可见，无法用编译期检查的函数指针形式连接；
         * 而 `QObject::connect(sender, SIGNAL(...), context, lambda)` 这种“字符串信号 +
         * lambda”的重载并不存在。因此采用“字符串信号 → 统一分发槽 → 按 sender() 查表”
         * 的写法：连接仍然是**在 C++ 侧显式建立**的，QML 中依旧没有任何信号处理器。
         *
         * @param name   控件 objectName
         * @param signal 信号签名（不含前缀），如 `clicked()`、`currentIndexChanged()`
         * @param handler 处理函数
         * @return 是否连接成功
         */
        bool on_signal(const char* name, const char* signal, std::function<void()> handler);

        /**
         * @brief 与上面同义，但直接作用于一个已知对象。
         *
         * 供 `Repeater` / `ListView` 生成的委托热区使用：它们 objectName 相同、
         * 数量不定，无法通过 `find(name)` 唯一定位，只能逐个对象连接。
         *
         * @param object  目标对象
         * @param signal  信号签名（不含前缀）
         * @param handler 处理函数
         * @return 是否连接成功
         */
        bool on_signal(QObject* object, const char* signal, std::function<void()> handler);

        /** @brief 导航按钮 → 切换页面。 */
        void connect_navigation();

        /** @brief 周次 / 星期切换。 */
        void connect_week_navigation();

        /** @brief 课程列表页与“新建课程”。 */
        void connect_course_actions();

        /** @brief 学期页表单。 */
        void connect_semester_page();

        /** @brief 设置页（目录、作息表、数据维护）。 */
        void connect_settings_page();

        /** @brief 课程编辑器。 */
        void connect_course_editor();

        /** @brief 课程详情弹层：进入编辑器 / 关闭。 */
        void connect_course_detail();

        /** @brief 折叠菜单 */
        void connect_overflow_menu();

        /** @brief 导入向导。 */
        void connect_import_wizard();

        /** @brief 导出对话框。 */
        void connect_export_dialog();

        /** @brief 提醒设置与通知横幅。 */
        void connect_reminders();

        /** @brief 教务适配器（可选）的地址、Cookie 与导入触发。 */
        void connect_school_adapters();

        /** @brief 在应用内横幅上展示一条提醒；`seconds` 秒后自动隐藏。 */
        void show_banner(const QString& title, const QString& message, int seconds = 8);

        /** @brief 首次填充界面初值（学期表单、设置页文本框）。 */
        void prime_widgets();

        /** @return 导入导出桥接；`m_bridge` 为空时返回 nullptr。 */
        ImportExportBridge* import_export() const;

        // ------------------------------------------------------------ 课程编辑器

        /**
         * @brief 打开课程详情弹层并填充 `course` / `sessions`。
         * @param course_id 课程 id；为空或查不到课程时不弹层
         */
        void show_course_detail(const QString& course_id);

        /** @brief 打开课程编辑器；`course_id` 为空表示新建。 */
        void open_course_editor(const QString& course_id);

        /** @brief 读取编辑器表单并保存课程；成功后关闭对话框。 */
        void save_course_from_editor();

        /** @return 由编辑器的时间段表单生成一行草稿。 */
        QVariantMap session_row_from_form() const;

        /** @brief 把时间段草稿行回填到表单。 */
        void load_session_row_into_form(int row);

        /** @brief 向 `sessionDraftModel` 追加一行。 */
        void append_session_row(const QVariantMap& row);

        /** @brief 替换 `sessionDraftModel` 的第 row 行。 */
        void update_session_row(int row, const QVariantMap& row_data);

        /** @brief 清空 `sessionDraftModel`。 */
        void clear_session_rows();

        /** @return `sessionDraftModel` 的行数。 */
        int session_row_count() const;

        // ---------------------------------------------------------------- 动态卡片

        /** @brief 立即扫描并连接全部 `sessionCardClick` 热区。 */
        void connect_session_cards();

        /** @return 可视子树中全部 objectName 匹配的节点（委托不在 `QObject::children()` 中）。 */
        QList<QQuickItem*> visual_hotspots(const QString& name) const;

        /** @return 可视树根；`ApplicationWindow` 的根是它的 `contentItem`。 */
        QQuickItem* root_item() const;

        /**
         * @brief 把 `ListView` 委托热区的点击转换为「把行号写回 `list.currentIndex`」。
         *
         * `ListView` 的委托只挂在 `contentItem` 的**可视**子树下，不进入 `QObject::children()`，
         * 因此这里遍历 `QQuickItem::childItems()`，并在可视子项变化时重扫。
         *
         * @param list_name      `ListView` 的 objectName
         * @param hotspot_name   委托内热区的 objectName
         * @param index_property 热区上保存行号的属性名
         */
        void connect_list_selection(const char* list_name, const char* hotspot_name, const char* index_property);

        /** @brief 清空已连接热区记录（对象被销毁后调用）。 */
        void prune_card_connections();

    private slots:
        /** @brief 统一分发槽：按 `sender()` 找到并执行对应控件的处理函数。 */
        void dispatch();

        /** @brief 延迟一拍重新扫描并连接课卡热区（`Repeater` 重建后调用）。 */
        void schedule_card_reconnect();

    private:
        /** QML 根对象；仅在本对象生命周期内有效。 */
        QPointer<QObject> m_root;

        /** 课表桥接；不持有所有权。 */
        ScheduleBridge* m_bridge = nullptr;

        /** 早期桥接对象；不持有所有权。 */
        AppBridge* m_app_bridge = nullptr;

        /** 提醒服务；不持有所有权，可为空。 */
        NotificationService* m_notifications = nullptr;

        /** 横幅自动隐藏的世代计数：新横幅到来时让上一次的定时回调失效。 */
        int m_banner_generation = 0;

        /** 页面栈（`StackLayout`）。 */
        QPointer<QObject> m_page_stack;

        /** 课程编辑器对话框。 */
        QPointer<QObject> m_course_editor;

        /** 课程详情弹层。 */
        QPointer<QObject> m_course_detail;

        /** 详情弹层当前展示的课程 id；「编辑」按钮据此进入课程编辑器。 */
        QString m_detail_course_id;

        /** 导入向导对话框。 */
        QPointer<QObject> m_import_wizard;

        /** 导出对话框。 */
        QPointer<QObject> m_export_dialog;

        /** 已连接课卡热区，避免重复连接。 */
        QSet<QObject*> m_connected_cards;

        /** 控件 → 处理函数映射；由 `dispatch()` 在信号触发时查表执行。 */
        QHash<QObject*, std::function<void()>> m_handlers;

        /** 重新扫描是否已经排队，避免同一帧内重复排队。 */
        bool m_reconnect_pending = false;

        /** 同步 weekSelector.currentIndex 时抑制信号回环。 */
        bool m_syncing_week_selector = false;

        /** 同步 daySelector.currentIndex 时抑制信号回环。 */
        bool m_syncing_day_selector = false;

        /** 已成功建立的 `clicked` 连接数量（用于启动日志与自检）。 */
        int m_connection_count = 0;
    };

} // namespace Schedule
