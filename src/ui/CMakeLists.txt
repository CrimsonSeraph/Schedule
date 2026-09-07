# src/ui - UI 层：QML 模块 ScheduleUI（URI: Schedule）
find_package(Qt6 6.9.3 REQUIRED COMPONENTS Quick Qml)

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
	RESOURCES
	resources/assets.qrc
)

target_link_libraries(ScheduleUI
	PUBLIC
		ScheduleEngine
		Qt6::Quick
		Qt6::Qml
)
