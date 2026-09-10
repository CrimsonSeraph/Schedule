cmake_minimum_required(VERSION 3.20)

project(ScheduleApp
	VERSION 1.0.0
	DESCRIPTION "跨平台课表应用 Schedule"
	LANGUAGES CXX
)

# 全局编译选项
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
	set_property(
		CACHE CMAKE_BUILD_TYPE
		PROPERTY
			STRINGS
			Debug
			Release
			RelWithDebInfo
			MinSizeRel
	)
endif()

# 通用选项
option(BUILD_TESTS "构建单元测试" OFF)

# 启用 CTest：必须在所有 add_test() 之前调用，故放在顶层
if(BUILD_TESTS)
	enable_testing()
endif()

# Qt 依赖（后续模块按需补充组件）
# Gui 必须在这里显式请求：Android 打包链路要求 Qt6::Gui 的插件包
# （如 Qt6::QAndroidIntegrationPlugin）在 qt_add_executable 定稿前完成注册；
# 只在子目录里通过 Quick 间接依赖 Gui，会导致 androiddeployqt 生成部署设置时报
# "No target Qt6::QAndroidIntegrationPlugin"。
find_package(Qt6 6.9.3 REQUIRED COMPONENTS Core Gui Qml Quick QuickDialogs2 Sql)

# Qt 推荐的项目级初始化（启用 AUTOMOC/AUTOUIC/AUTORCC，采纳 Qt 策略默认值）
qt_standard_project_setup()

# 子模块（按阶段逐步启用）
# 阶段 1: 核心层，纯 C++，仅依赖 Qt6::Core
add_subdirectory(src/core)
# 阶段 2: 基础设施层，SQLite 持久化与导入导出，依赖 core
add_subdirectory(src/data)
# 阶段 3: 引擎层，QML 绑定对象，依赖 core / data
add_subdirectory(src/engine)
# 阶段 4: UI 层，QML 模块，依赖 engine
add_subdirectory(src/ui)
# 阶段 5: 应用入口，可执行程序
add_subdirectory(src/app)
