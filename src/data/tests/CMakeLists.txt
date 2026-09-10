# src/data/tests - 持久化层单元测试（SQLite 内存库 + QTemporaryDir，不触碰真实用户数据）
find_package(Qt6 6.9.3 REQUIRED COMPONENTS Test)

include(${CMAKE_SOURCE_DIR}/cmake/ScheduleQtDeploy.cmake)

set(SCHEDULE_DATA_TESTS
	tst_import_export
	tst_schedule_json
	tst_sqlite_repository
)

foreach(test_name IN LISTS SCHEDULE_DATA_TESTS)
	add_executable(${test_name} ${test_name}.cpp)

	target_link_libraries(${test_name}
		PRIVATE
			ScheduleData
			Qt6::Test
	)

	set_target_properties(${test_name}
		PROPERTIES
			AUTOMOC ON
			CXX_STANDARD 20
			CXX_STANDARD_REQUIRED ON
			CXX_EXTENSIONS OFF
	)

	# 测试可执行文件同样需要 Qt6Sql.dll 等运行库才能启动
	schedule_deploy_qt_runtime(${test_name})

	add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
