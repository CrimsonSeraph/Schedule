cmake_minimum_required(VERSION 3.20)

project(MyQtApp
    VERSION 1.0.0
    DESCRIPTION "跨平台课表应用 Schedule (MyQtApp)"
    LANGUAGES CXX
)

# ---- 全局编译选项 ----
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

# ---- Qt 依赖（后续模块按需补充组件）----
find_package(Qt6 6.9.3 REQUIRED COMPONENTS Core Qml)

# ---- 子模块（按阶段逐步启用）----
# 阶段 2: 核心层，纯 C++，仅依赖 Qt6::Core
# add_subdirectory(src/core)
# 阶段 3: 引擎层，QML 绑定对象，依赖 core
# add_subdirectory(src/engine)
# 阶段 4: UI 层，QML 模块，依赖 engine
# add_subdirectory(src/ui)
# 阶段 5: 应用入口，可执行程序
# add_subdirectory(src/app)
