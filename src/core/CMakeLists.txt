# src/core - 核心层：纯 C++ 业务逻辑，无任何 GUI 依赖
# 注意：含 Q_OBJECT 的头文件必须列在目标源中，AUTOMOC 才会为其生成元对象代码
add_library(ScheduleCore STATIC
	src/DataEngine.cpp
	src/model/Conflict.cpp
	src/model/Course.cpp
	src/model/CourseSession.cpp
	src/model/ScheduleSnapshot.cpp
	src/model/Semester.cpp
	src/model/TimeSlot.cpp
	src/model/WeekMask.cpp
	src/service/ConflictDetector.cpp
	src/service/ScheduleService.cpp
	src/service/WeekCalculator.cpp
	include/core/model/Conflict.h
	include/core/model/Course.h
	include/core/model/CourseSession.h
	include/core/model/ScheduleSnapshot.h
	include/core/model/Semester.h
	include/core/model/TimeSlot.h
	include/core/model/WeekMask.h
	include/core/service/ConflictDetector.h
	include/core/service/ScheduleService.h
	include/core/service/WeekCalculator.h
)

target_include_directories(ScheduleCore
	PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ScheduleCore
	PUBLIC Qt6::Core
)

set_target_properties(ScheduleCore
	PROPERTIES
		# ScheduleService 是 QObject，需要 AUTOMOC 生成元对象代码
		AUTOMOC ON
		CXX_STANDARD 20
		CXX_STANDARD_REQUIRED ON
		CXX_EXTENSIONS OFF
)

# 纯逻辑层单元测试：不依赖 GUI，可在无显示环境运行
if(BUILD_TESTS)
	add_subdirectory(tests)
endif()
