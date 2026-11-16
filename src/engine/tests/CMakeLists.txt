# src/engine/tests - 引擎层单元测试：验证 SessionListModel 的“非本周幽灵卡”逻辑
find_package(Qt6 6.9.3 REQUIRED COMPONENTS Test)

add_executable(tst_session_model tst_session_model.cpp)

target_link_libraries(tst_session_model
	PRIVATE
		ScheduleEngine
		Qt6::Test
)

set_target_properties(tst_session_model
	PROPERTIES
		AUTOMOC ON
		CXX_STANDARD 20
		CXX_STANDARD_REQUIRED ON
		CXX_EXTENSIONS OFF
)

schedule_deploy_qt_runtime(tst_session_model)

add_test(NAME tst_session_model COMMAND tst_session_model)
