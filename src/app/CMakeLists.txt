# src/app - 应用入口：可执行程序 Schedule
add_executable(Schedule
	main.cpp
)

target_link_libraries(Schedule
	PRIVATE
		ScheduleUI
		ScheduleEngine
)

set_target_properties(Schedule
	PROPERTIES
		AUTOMOC ON
		CXX_STANDARD 20
		CXX_STANDARD_REQUIRED ON
		CXX_EXTENSIONS OFF
)

# 自动化 UI 自检（默认关闭；仅供本地开发验证，勿用于移动端发布包）
option(BUILD_SELFTEST
	"Enable automated UI self test (desktop development only)"
	OFF
)
if(BUILD_SELFTEST)
	find_package(Qt6 6.9.3 REQUIRED COMPONENTS Test)
	target_link_libraries(Schedule PRIVATE Qt6::Test)
	target_compile_definitions(Schedule PRIVATE Schedule_SELFTEST=1)
endif()
# Windows 桌面：构建后把 Qt 运行库部署到 Schedule.exe 同级目录，可直接双击运行。
# 具体实现收敛在 cmake/ScheduleQtDeploy.cmake，测试可执行文件复用同一个函数。
include(${CMAKE_SOURCE_DIR}/cmake/ScheduleQtDeploy.cmake)
schedule_deploy_qt_runtime(Schedule
	--compiler-runtime
	--qmldir "${CMAKE_SOURCE_DIR}/src/ui/qml"
)
