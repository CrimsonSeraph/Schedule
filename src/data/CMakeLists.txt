# src/data - 基础设施层：SQLite 持久化、JSON 序列化、导入导出实现
find_package(Qt6 6.9.3 REQUIRED COMPONENTS Network Sql)

add_library(ScheduleData STATIC
	src/AppSettings.cpp
	src/ImportSource.cpp
	src/ScheduleJson.cpp
	src/SettingsKeys.cpp
	src/SqliteScheduleRepository.cpp
	src/import_export/CsvScheduleIo.cpp
	src/import_export/ExportManager.cpp
	src/import_export/IcsScheduleIo.cpp
	src/import_export/ImportExportTypes.cpp
	src/import_export/ImportManager.cpp
	src/import_export/JsonScheduleIo.cpp
	src/adapter/GenericSchoolAdapter.cpp
	src/adapter/NetworkScheduleFetcher.cpp
	include/data/AppSettings.h
	include/data/ImportSource.h
	include/data/IScheduleRepository.h
	include/data/ScheduleJson.h
	include/data/SettingsKeys.h
	include/data/SqliteScheduleRepository.h
	include/data/import_export/CsvScheduleIo.h
	include/data/import_export/ExportManager.h
	include/data/import_export/IScheduleExporter.h
	include/data/import_export/IScheduleImporter.h
	include/data/import_export/IcsScheduleIo.h
	include/data/import_export/ImportExportTypes.h
	include/data/import_export/ImportManager.h
	include/data/import_export/JsonScheduleIo.h
	include/data/adapter/GenericSchoolAdapter.h
	include/data/adapter/NetworkScheduleFetcher.h
)

target_include_directories(ScheduleData
	PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ScheduleData
	PUBLIC
		ScheduleCore
		Qt6::Core
		Qt6::Network
		Qt6::Sql
)

set_target_properties(ScheduleData
	PROPERTIES
		CXX_STANDARD 20
		CXX_STANDARD_REQUIRED ON
		CXX_EXTENSIONS OFF
)

# 持久化层单元测试：使用内存库 / 临时目录，不触碰真实用户数据
if(BUILD_TESTS)
	add_subdirectory(tests)
endif()
