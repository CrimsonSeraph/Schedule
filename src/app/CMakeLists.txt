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
# ---- Windows 桌面：构建后把 Qt 运行库部署到 Schedule.exe 同级目录，可直接双击运行 ----
# add_custom_command(TARGET ...) 必须与目标同目录，故写在这里而非根 CMakeLists.txt。
# Qt6_DIR（find_package 已由根 CMakeLists 执行）形如 <Qt安装根>/lib/cmake/Qt6，向上三级即套件根目录。
if(WIN32 AND NOT CMAKE_CROSSCOMPILING)
	get_filename_component(QT_INSTALL_PREFIX "${Qt6_DIR}/../../.." ABSOLUTE)
	find_program(WINDEPLOYQT_EXECUTABLE
		NAMES
		windeployqt
		HINTS
		"${QT_INSTALL_PREFIX}/bin"
		NO_DEFAULT_PATH
	)
	if(WINDEPLOYQT_EXECUTABLE)
		add_custom_command(
			TARGET Schedule
			POST_BUILD
			COMMAND "${WINDEPLOYQT_EXECUTABLE}" --no-translations
				--no-system-d3d-compiler --no-opengl-sw --no-ffmpeg --compiler-runtime
				--qmldir "${CMAKE_SOURCE_DIR}/src/ui/qml"
				"$<$<CONFIG:Debug>:--debug>$<$<NOT:$<CONFIG:Debug>>:--release>"
				"$<TARGET_FILE:Schedule>"
			COMMENT "windeployqt: copying Qt runtime next to Schedule.exe"
			VERBATIM
		)
	else()
		message(WARNING
			"windeployqt not found under ${QT_INSTALL_PREFIX}/bin — Qt DLLs will not be deployed next to Schedule.exe"
		)
	endif()
endif()
