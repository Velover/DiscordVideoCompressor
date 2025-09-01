cmake_minimum_required(VERSION 3.16)
project(VideoCompressor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Configure for dynamic builds
set(CMAKE_PREFIX_PATH "C:/Qt/6.9.2/mingw_64")

# Find required Qt components
find_package(Qt6 REQUIRED COMPONENTS 
    Core 
    Widgets 
    Quick
    Qml
    QuickControls2
    Gui
)

qt_standard_project_setup()

# Debug: Print the actual CMAKE_BUILD_TYPE value
message(STATUS "CMAKE_BUILD_TYPE is: '${CMAKE_BUILD_TYPE}'")
message(STATUS "CMAKE_BUILD_TYPE length: ${CMAKE_BUILD_TYPE}")

# Set default build type if not specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
    message(STATUS "Setting build type to default: ${CMAKE_BUILD_TYPE}")
endif()

# Normalize to uppercase for reliable comparison
string(TOUPPER "${CMAKE_BUILD_TYPE}" BUILD_TYPE_UPPER)
message(STATUS "Build type (uppercase): ${BUILD_TYPE_UPPER}")

# Create Windows resource file for icon
set(RESOURCE_FILES "")
if(WIN32)
    set(RESOURCE_FILE "${CMAKE_CURRENT_BINARY_DIR}/app_icon.rc")
    file(WRITE ${RESOURCE_FILE} "IDI_ICON1 ICON \"${CMAKE_CURRENT_SOURCE_DIR}/app.ico\"\n")
    set(RESOURCE_FILES ${RESOURCE_FILE})
endif()

# Use normalized comparison
if(BUILD_TYPE_UPPER STREQUAL "DEBUG")
    message(STATUS "Creating DEBUG executable")
    qt_add_executable(video_compressor 
        src/main.cpp
        src/videocompressor.cpp
        src/videocompressor.h
        src/clipboardmanager.cpp
        src/clipboardmanager.h
        ${RESOURCE_FILES}
    )
else()
    message(STATUS "Creating RELEASE executable (WIN32)")
    qt_add_executable(video_compressor WIN32
        src/main.cpp
        src/videocompressor.cpp
        src/videocompressor.h
        src/clipboardmanager.cpp
        src/clipboardmanager.h
        ${RESOURCE_FILES}
    )
endif()

# Set up include directories
target_include_directories(video_compressor PRIVATE src)

# Add QML resources
qt_add_resources(video_compressor "qml_resources"
    PREFIX "/"
    FILES
        qml/VideoCompressorWindow.qml
        qml/VideoListItem.qml
        qml/DebugConsole.qml
)

# Link Qt libraries (added Gui for QPainter)
target_link_libraries(video_compressor PRIVATE 
    Qt6::Core
    Qt6::Widgets
    Qt6::Quick
    Qt6::Qml
    Qt6::QuickControls2
    Qt6::Gui
)