# src/data/tests - 持久化层单元测试（SQLite 内存库 + QTemporaryDir，不触碰真实用户数据）
find_package(Qt6 6.9.3 REQUIRED COMPONENTS Test)

include(${CMAKE_SOURCE_DIR}/cmake/ScheduleQtDeploy.cmake)

set(SCHEDULE_DATA_TESTS
	tst_import_export
	tst_sample_files
	tst_schedule_json
	tst_school_adapter
	tst_sqlite_repository
)

foreach(test_name IN LISTS SCHEDULE_DATA_TESTS)
	add_executable(${test_name} ${test_name}.cpp)

	target_link_libraries(${test_name}
		PRIVATE
			ScheduleData
			Qt6::Test
	)

	# 样本文件测试需要知道仓库中 samples/ 的绝对路径
	target_compile_definitions(${test_name}
		PRIVATE SCHEDULE_SAMPLES_DIR="${CMAKE_SOURCE_DIR}/samples"
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
