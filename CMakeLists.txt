cmake_minimum_required(VERSION 3.16)

project(AgentLauncher VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Qt6 discovery. Pass -DCMAKE_PREFIX_PATH=C:/Qt/6.7.3/msvc2019_64 if not set.
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Network)

qt_standard_project_setup(REQUIRES 6.5)

set(APP_SOURCES
    src/main.cpp
    src/AgentConfig.cpp
    src/AgentModel.cpp
    src/AgentLauncher.cpp
)

set(APP_HEADERS
    src/AgentConfig.h
    src/AgentModel.h
    src/AgentLauncher.h
)

qt_add_executable(AgentLauncher
    ${APP_SOURCES}
    ${APP_HEADERS}
)

qt_add_resources(AgentLauncher "app_resources"
    PREFIX "/"
    FILES
        config/default_agents.json
        icons/kimi-code.svg
        icons/opencode.svg
        icons/qwen-code.svg
        qml/main.qml
        qml/AgentCard.qml
        qml/ConfigPage.qml
)

target_link_libraries(AgentLauncher PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Network
)

# Win32 GUI subsystem (no console window) on Windows.
set_target_properties(AgentLauncher PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
)
