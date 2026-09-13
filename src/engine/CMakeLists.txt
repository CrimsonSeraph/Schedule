# src/engine - 引擎/桥接层：向 QML 暴露 C++ 对象
add_library(ScheduleEngine STATIC
	src/AndroidNotificationBackend.cpp
	src/AppBridge.cpp
	src/BrowserCaptureScript.cpp
	src/CourseListModel.cpp
	src/INotificationBackend.cpp
	src/ImportExportBridge.cpp
	src/NotificationService.cpp
	src/ScheduleBridge.cpp
	src/SessionListModel.cpp
	include/engine/AndroidNotificationBackend.h
	include/engine/AppBridge.h
	include/engine/BrowserCaptureScript.h
	include/engine/CourseListModel.h
	include/engine/INotificationBackend.h
	include/engine/ImportExportBridge.h
	include/engine/NotificationService.h
	include/engine/ScheduleBridge.h
	include/engine/SessionListModel.h
)

target_include_directories(ScheduleEngine
	PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ScheduleEngine
	PUBLIC
		ScheduleCore
		ScheduleData
		Qt6::Core
		Qt6::Qml
)

set_target_properties(ScheduleEngine
	PROPERTIES
		AUTOMOC ON
		CXX_STANDARD 20
		CXX_STANDARD_REQUIRED ON
		CXX_EXTENSIONS OFF
)

# 内嵌浏览器后端（由根 CMakeLists 在配置期判定）。
# 只暴露给 C++ 侧用于回答 QML 的 webBrowserBackend；具体的 QML 实现文件由 ui 层按同一变量选择。
if(SCHEDULE_BROWSER_BACKEND STREQUAL "webview")
	target_compile_definitions(ScheduleEngine PRIVATE Schedule_HAS_WEBVIEW=1)
elseif(SCHEDULE_BROWSER_BACKEND STREQUAL "webengine")
	target_compile_definitions(ScheduleEngine PRIVATE Schedule_HAS_WEBENGINE=1)
endif()
