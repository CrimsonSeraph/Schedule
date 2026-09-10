# 供各模块复用的 Qt 运行时部署辅助函数。
#
# 背景：Windows 桌面构建产物若不在 PATH 中能找到 Qt DLL，双击运行或 ctest 都会以
# 0xc0000135（STATUS_DLL_NOT_FOUND）失败。本模块把“把 Qt 运行库部署到目标文件同级目录”
# 这件事集中成一个函数，避免每个 target 各写一遍。
include_guard(GLOBAL)

# schedule_deploy_qt_runtime(<target> [额外 windeployqt 参数...])
#
# 非 Windows 或交叉编译（Android / iOS）时为空操作。
# 额外参数会原样透传给 windeployqt，例如 `--qmldir <dir>`、`--compiler-runtime`。
function(schedule_deploy_qt_runtime target)
	if(NOT WIN32 OR CMAKE_CROSSCOMPILING)
		return()
	endif()

	# Qt6_DIR 形如 <Qt 安装根>/lib/cmake/Qt6，向上三级即套件根目录
	get_filename_component(qt_install_prefix "${Qt6_DIR}/../../.." ABSOLUTE)
	find_program(SCHEDULE_WINDEPLOYQT_EXECUTABLE
		NAMES
			windeployqt
		HINTS
			"${qt_install_prefix}/bin"
		NO_DEFAULT_PATH
	)
	if(NOT SCHEDULE_WINDEPLOYQT_EXECUTABLE)
		message(WARNING
			"windeployqt not found under ${qt_install_prefix}/bin — Qt runtime will not be deployed for target '${target}'"
		)
		return()
	endif()

	set(schedule_extra_arguments "")
	if(ARGC GREATER 1)
		list(APPEND schedule_extra_arguments ${ARGN})
	endif()

	add_custom_command(
		TARGET ${target}
		POST_BUILD
		COMMAND
			"${SCHEDULE_WINDEPLOYQT_EXECUTABLE}" --no-translations
			--no-system-d3d-compiler --no-opengl-sw --no-ffmpeg
			"$<$<CONFIG:Debug>:--debug>$<$<NOT:$<CONFIG:Debug>>:--release>"
			${schedule_extra_arguments}
			"$<TARGET_FILE:${target}>"
		COMMENT "windeployqt: deploying Qt runtime for ${target}"
		VERBATIM
	)
endfunction()
