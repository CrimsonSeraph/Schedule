# src/ui - UI 层：QML 模块 MyUI（URI: MyApp）
find_package(Qt6 6.9.3 REQUIRED COMPONENTS Quick Qml)

add_library(MyUI STATIC)

qt6_add_qml_module(MyUI
    URI MyApp
    VERSION 1.0
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

set_target_properties(MyUI PROPERTIES
    AUTOMOC ON
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
