# src/engine - 引擎/桥接层：向 QML 暴露 C++ 对象
add_library(ScheduleEngine STATIC
	src/AndroidNotificationBackend.cpp
	src/AppBridge.cpp
	src/CourseListModel.cpp
	src/INotificationBackend.cpp
	src/ImportExportBridge.cpp
	src/NotificationService.cpp
	src/ScheduleBridge.cpp
	src/SessionListModel.cpp
	include/engine/AndroidNotificationBackend.h
	include/engine/AppBridge.h
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
