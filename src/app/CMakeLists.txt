# src/app - 应用入口：可执行程序 Schedule
# 使用 qt_add_executable 而非 add_executable：
#   * 桌面平台（Windows/Linux/macOS）等价于 add_executable，行为不变；
#   * Android 下会构建为可被 Java 壳加载的 MODULE 库，并创建 apk / aab 打包目标
#     （androiddeployqt 由该目标驱动）。若用 add_executable，交叉编译只能得到 .so，
#     cmake --build 结束时不生成 APK，也不生成 *deployment-settings.json。
qt_add_executable(Schedule
	main.cpp
	UiConnector.cpp
	UiConnector.h
)

# 桌面系统托盘通知后端依赖 Qt Widgets（QSystemTrayIcon），因此只参与桌面构建。
# 移动端使用 engine 层的 AndroidNotificationBackend 或应用内横幅。
if(NOT ANDROID AND NOT IOS)
	find_package(Qt6 6.9.3 REQUIRED COMPONENTS Widgets)
	target_sources(Schedule
		PRIVATE
			TrayNotificationBackend.cpp
			TrayNotificationBackend.h
	)
	target_compile_definitions(Schedule PRIVATE Schedule_HAS_TRAY_NOTIFICATIONS=1)
	target_link_libraries(Schedule PRIVATE Qt6::Widgets)
endif()

target_link_libraries(Schedule
	PRIVATE
		ScheduleUI
		ScheduleEngine
		ScheduleData
		ScheduleCore
)

set_target_properties(Schedule
	PROPERTIES
		AUTOMOC ON
		CXX_STANDARD 20
		CXX_STANDARD_REQUIRED ON
		CXX_EXTENSIONS OFF
)

# 自动化端到端自检（默认关闭；仅供本地开发验证，勿用于移动端发布包）
option(BUILD_SELFTEST
	"Enable automated end-to-end self test (desktop development only)"
	OFF
)
if(BUILD_SELFTEST)
	find_package(Qt6 6.9.3 REQUIRED COMPONENTS Test)
	target_sources(Schedule
		PRIVATE
			SelfTest.cpp
			SelfTest.h
	)
	target_link_libraries(Schedule PRIVATE Qt6::Test)
	target_compile_definitions(Schedule PRIVATE Schedule_SELFTEST=1)
endif()

# ---------------------------------------------------------------- 国际化
# 源语言是简体中文（qsTr/tr 直接写中文），因此只需要其它语言的 .ts。
# qt_add_translations 会把 .qm 嵌入资源（前缀 /i18n），并创建 update_translations 目标。
# 缺少 Qt Linguist 工具时降级为“不翻译”，不影响构建。
find_package(Qt6 6.9.3 QUIET COMPONENTS LinguistTools)
if(Qt6LinguistTools_FOUND)
	qt_add_translations(Schedule
		TS_FILES
			"${CMAKE_SOURCE_DIR}/i18n/schedule_en.ts"
		RESOURCE_PREFIX
			"/i18n"
	)
else()
	message(WARNING "未找到 Qt Linguist 工具（lupdate/lrelease），本次构建不包含翻译文件")
endif()

# 课表样本文件：复制到可执行文件同级目录，供 --selftest 与手工导入使用。
# 数据层只接收路径，因此自检读取的是真实文件而不是内嵌资源。
add_custom_command(
	TARGET Schedule
	POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory
		"${CMAKE_SOURCE_DIR}/samples"
		"$<TARGET_FILE_DIR:Schedule>/samples"
	COMMENT "复制课表样本文件到构建输出目录"
	VERBATIM
)

# Windows 桌面：构建后把 Qt 运行库部署到 Schedule.exe 同级目录，可直接双击运行。
# 具体实现收敛在 cmake/ScheduleQtDeploy.cmake，测试可执行文件复用同一个函数。
include(${CMAKE_SOURCE_DIR}/cmake/ScheduleQtDeploy.cmake)
schedule_deploy_qt_runtime(Schedule
	--compiler-runtime
	--qmldir "${CMAKE_SOURCE_DIR}/src/ui/qml"
)
