# src/core/tests - 核心层单元测试（仅依赖 Qt6::Test 与 Qt6::Core，无 GUI）
find_package(Qt6 6.9.3 REQUIRED COMPONENTS Test)

include(${CMAKE_SOURCE_DIR}/cmake/ScheduleQtDeploy.cmake)

# 每个测试用例一个可执行文件，便于 ctest 精确定位失败项
set(SCHEDULE_CORE_TESTS
	tst_week_mask
	tst_week_calculator
	tst_conflict_detector
	tst_schedule_service
	tst_reminder_scheduler
)

foreach(test_name IN LISTS SCHEDULE_CORE_TESTS)
	add_executable(${test_name} ${test_name}.cpp)

	target_link_libraries(${test_name}
		PRIVATE
			ScheduleCore
			Qt6::Test
	)

	set_target_properties(${test_name}
		PROPERTIES
			AUTOMOC ON
			CXX_STANDARD 20
			CXX_STANDARD_REQUIRED ON
			CXX_EXTENSIONS OFF
	)

	# 测试可执行文件同样需要 Qt6Test.dll 等运行库才能启动
	schedule_deploy_qt_runtime(${test_name})

	add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
