# src/engine - 引擎/桥接层：向 QML 暴露 C++ 对象
add_library(MyEngine STATIC
    src/AppBridge.cpp
    include/engine/AppBridge.h   # 显式列出头文件，确保 AUTOMOC 为其生成 moc
)

target_include_directories(MyEngine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(MyEngine PUBLIC
    MyCore
    Qt6::Core
    Qt6::Qml
)

set_target_properties(MyEngine PROPERTIES
    AUTOMOC ON
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
