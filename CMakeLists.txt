cmake_minimum_required(VERSION 3.16)

project(frp_control_client VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Network)

if(WIN32 AND NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(FRP_CLIENT_APP_TYPE WIN32)
endif()

if(WIN32)
    set(FRP_CLIENT_WIN_RESOURCES app/assets/app_icon.rc)
endif()

add_executable(frp-control-client ${FRP_CLIENT_APP_TYPE}
    app/src/main.cpp
    app/src/models.h
    app/src/profile_service.h
    app/src/profile_service.cpp
    app/src/control_api_client.h
    app/src/control_api_client.cpp
    app/src/tunnel_runtime_service.h
    app/src/tunnel_runtime_service.cpp
    app/src/app_controller.h
    app/src/app_controller.cpp
    app/assets/app_assets.qrc
    ${FRP_CLIENT_WIN_RESOURCES}
)

target_link_libraries(frp-control-client PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Network
)

qt_add_qml_module(frp-control-client
    URI FrpControlClient
    VERSION 1.0
    QML_FILES
        qml/Main.qml
)

target_compile_definitions(frp-control-client PRIVATE
    APP_VERSION="${PROJECT_VERSION}"
)

if(MSVC)
    target_compile_options(frp-control-client PRIVATE /utf-8)
endif()

if(MINGW)
    target_link_options(frp-control-client PRIVATE -static-libgcc -static-libstdc++)
endif()
