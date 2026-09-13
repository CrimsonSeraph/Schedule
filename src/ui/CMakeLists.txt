# src/ui - UI 层：QML 模块 ScheduleUI（URI: Schedule）
# QuickDialogs2 提供 FileDialog / FolderDialog（文件选择属于 UI 层职责）
find_package(Qt6 6.9.3 REQUIRED COMPONENTS Quick Qml QuickDialogs2)

# 采用 /qt/qml 作为 QML 模块默认资源前缀（qrc:/qt/qml/Schedule/...）
qt_policy(SET QTP0001 NEW)
# 为子目录中的 QML 文件自动生成 qmldir（工具链支持）
qt_policy(SET QTP0004 NEW)

add_library(ScheduleUI STATIC)

# AUTOMOC 必须先于 qt6_add_qml_module 设置（元类型生成依赖）
set_target_properties(ScheduleUI
	PROPERTIES
		AUTOMOC ON
		CXX_STANDARD 20
		CXX_STANDARD_REQUIRED ON
		CXX_EXTENSIONS OFF
)

# style/ 下全部是 QML 单例（统一设计令牌），必须在 qt6_add_qml_module 之前声明。
# QT_QML_SINGLETON_TYPE 是「逐文件」属性：一次 set_source_files_properties 里写多组
# 同名属性只会让最后一组生效（曾导致 6 个单例全部指向 Typography.qml），
# 因此这里一个文件一条调用，且不额外设置 QT_RESOURCE_ALIAS（qmldir 会登记 style/ 子路径）。
set_source_files_properties(style/CourseCardStyle.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
set_source_files_properties(style/ListItemStyle.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
set_source_files_properties(style/Metrics.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
set_source_files_properties(style/Responsive.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
set_source_files_properties(style/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
set_source_files_properties(style/Typography.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt6_add_qml_module(
	ScheduleUI
	URI
	Schedule
	VERSION
	1.0
	NO_PLUGIN
	OUTPUT_DIRECTORY
	"${CMAKE_CURRENT_BINARY_DIR}/Schedule"
	QML_FILES
	qml/MainDesktop.qml
	qml/MainMobile.qml
	qml/WeekView.qml
	qml/DayView.qml
	qml/CourseCard.qml
	qml/CourseEditor.qml
	qml/CourseDetailDialog.qml
	qml/ImportWizard.qml
	qml/ExportDialog.qml
	qml/SettingsPage.qml
	qml/SemesterPage.qml
	style/CourseCardStyle.qml
	style/ListItemStyle.qml
	style/Metrics.qml
	style/Responsive.qml
	style/Theme.qml
	style/Typography.qml
	RESOURCES
	resources/assets.qrc
)

target_link_libraries(ScheduleUI
	PUBLIC
		ScheduleEngine
		Qt6::Quick
		Qt6::Qml
		Qt6::QuickDialogs2
)
