# src/ui - UI 层：QML 模块 MyUI（URI: MyApp）
find_package(Qt6 6.9.3 REQUIRED COMPONENTS Quick Qml)

# 采用 /qt/qml 作为 QML 模块默认资源前缀（qrc:/qt/qml/MyApp/...）
qt_policy(SET QTP0001 NEW)

add_library(MyUI STATIC)

# AUTOMOC 必须先于 qt6_add_qml_module 设置（元类型生成依赖）
set_target_properties(MyUI PROPERTIES
    AUTOMOC ON
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

qt6_add_qml_module(MyUI
    URI MyApp
    VERSION 1.0
    NO_PLUGIN
    QML_FILES
        qml/MainDesktop.qml
        qml/MainMobile.qml
    RESOURCES
        resources/assets.qrc
)

target_link_libraries(MyUI PUBLIC
    MyEngine
    Qt6::Quick
    Qt6::Qml
)