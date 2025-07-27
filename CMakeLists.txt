cmake_minimum_required(VERSION 3.16)
project(GlueDispensePC LANGUAGES CXX)

# --- Project-wide Settings ---
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# --- Qt Setup ---
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# --- Build Options ---
option(BUILD_FULL "Build the full application" ON)
option(BUILD_SIMPLE "Build the simple demo" OFF)
option(BUILD_DEBUG "Build the minimal debug version" OFF)

# ===================================================================
# === Target 1: Full Application (GlueDispensePC)
# ===================================================================
if(BUILD_FULL)
    find_package(Qt6 REQUIRED COMPONENTS
        Core Widgets SerialPort Network Sql Charts Concurrent
        SerialBus Bluetooth Multimedia PrintSupport Xml
    )

    # Automatically discover source files to avoid manual listing.
    # CONFIGURE_DEPENDS ensures that CMake reruns if files are added/removed.
    file(GLOB_RECURSE FULL_APP_SOURCES CONFIGURE_DEPENDS
        "src/communication/*.cpp"
        "src/config/*.cpp"
        "src/core/*.cpp"
        "src/data/*.cpp"
        "src/error/*.cpp"
        "src/logger/*.cpp"
        "src/ui/*.cpp"
        "src/utils/*.cpp"
        "src/mainwindow.cpp"
        "src/stub_implementations.cpp"
    )
    
    # Add continuous optimization sources
    list(APPEND FULL_APP_SOURCES
        "src/core/continuousoptimizer.cpp"
        "src/core/intelligentanalyzer.cpp"
        "src/core/adaptiveconfigmanager.cpp"
        "src/core/loadbalancer.cpp"
        "src/core/mlperformancepredictor.cpp"
    )

    add_executable(GlueDispensePC
        src/main.cpp
        ${FULL_APP_SOURCES}
        resources/app.qrc
    )

    target_include_directories(GlueDispensePC PRIVATE src src/communication src/ui src/data src/logger src/config src/utils)

    target_link_libraries(GlueDispensePC PRIVATE
        Qt6::Core Qt6::Widgets Qt6::SerialPort Qt6::Network Qt6::Sql Qt6::Charts Qt6::Concurrent
        Qt6::SerialBus Qt6::Bluetooth Qt6::Multimedia Qt6::PrintSupport Qt6::Xml
    )

    set_target_properties(GlueDispensePC PROPERTIES
        OUTPUT_NAME "GlueDispensePC"
        WIN32_EXECUTABLE TRUE
        MACOSX_BUNDLE TRUE
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
endif()

# ===================================================================
# === Target 2: Simple Demo
# ===================================================================
if(BUILD_SIMPLE)
    find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

    add_executable(SimpleDemo src/main_simple.cpp)

    target_link_libraries(SimpleDemo PRIVATE Qt6::Core Qt6::Widgets)

    set_target_properties(SimpleDemo PROPERTIES
        OUTPUT_NAME "SimpleDemo"
        WIN32_EXECUTABLE TRUE
        MACOSX_BUNDLE TRUE
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    if(APPLE)
        set_target_properties(SimpleDemo PROPERTIES
            MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/resources/Info.plist
        )
    endif()
endif()

# ===================================================================
# === Target 3: Minimal Debug
# ===================================================================
if(BUILD_DEBUG)
    find_package(Qt6 REQUIRED COMPONENTS 
        Core Widgets Network SerialPort SerialBus Bluetooth
    )

    # Include logger, config, utils, and essential communication sources for debug build
    file(GLOB DEBUG_SOURCES CONFIGURE_DEPENDS
        "src/logger/*.cpp"
        "src/config/*.cpp"
        "src/utils/*.cpp"
        "src/communication/serialcommunication.cpp"
        "src/communication/tcpcommunication.cpp"
        "src/communication/icommunication.cpp"
        "src/communication/protocolparser.cpp"
    )

    add_executable(DebugGlueDispensePC 
        src/main_debug.cpp
        ${DEBUG_SOURCES}
    )

    target_include_directories(DebugGlueDispensePC PRIVATE 
        src src/logger src/config src/utils src/communication
    )
    
    target_link_libraries(DebugGlueDispensePC PRIVATE
        Qt6::Core Qt6::Widgets Qt6::Network Qt6::SerialPort Qt6::SerialBus Qt6::Bluetooth
    )
    
    target_compile_definitions(DebugGlueDispensePC PRIVATE DEBUG_MODE)

    set_target_properties(DebugGlueDispensePC PROPERTIES
        OUTPUT_NAME "DebugGlueDispensePC"
        WIN32_EXECUTABLE TRUE
        MACOSX_BUNDLE TRUE
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
    
    # 添加简化版MainWindow测试目标
    add_executable(SimpleMainWindowTest
        src/main_simple_test.cpp
        src/mainwindow_simple.cpp
        ${DEBUG_SOURCES}
    )
    
    target_include_directories(SimpleMainWindowTest PRIVATE 
        src src/logger src/config src/utils src/communication
    )
    
    target_link_libraries(SimpleMainWindowTest PRIVATE
        Qt6::Core Qt6::Widgets Qt6::Network Qt6::SerialPort Qt6::SerialBus Qt6::Bluetooth
    )
    
    target_compile_definitions(SimpleMainWindowTest PRIVATE DEBUG_MODE)

    set_target_properties(SimpleMainWindowTest PROPERTIES
        OUTPUT_NAME "SimpleMainWindowTest"
        WIN32_EXECUTABLE TRUE
        MACOSX_BUNDLE TRUE
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
endif()

# --- CPack for packaging (optional but good practice) ---
include(CPack)