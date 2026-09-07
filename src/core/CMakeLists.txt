# src/core - 核心层：纯 C++ 业务逻辑，无任何 GUI 依赖
add_library(ScheduleCore STATIC
	src/DataEngine.cpp
)

target_include_directories(ScheduleCore
	PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ScheduleCore
	PUBLIC Qt6::Core
)

set_target_properties(ScheduleCore
	PROPERTIES
		CXX_STANDARD 20
		CXX_STANDARD_REQUIRED ON
		CXX_EXTENSIONS OFF
)
