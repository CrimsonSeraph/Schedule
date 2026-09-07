# src/app - 应用入口：可执行程序 MyApp
add_executable(MyApp
    main.cpp
)

target_link_libraries(MyApp PRIVATE
    MyUI
    MyEngine
)

set_target_properties(MyApp PROPERTIES
    AUTOMOC ON
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# 自动化 UI 自检（默认关闭；仅供本地开发验证，勿用于移动端发布包）
option(BUILD_SELFTEST "Enable automated UI self test (desktop development only)" OFF)
if(BUILD_SELFTEST)
    find_package(Qt6 6.9.3 REQUIRED COMPONENTS Test)
    target_link_libraries(MyApp PRIVATE Qt6::Test)
    target_compile_definitions(MyApp PRIVATE MYAPP_SELFTEST=1)
endif()
