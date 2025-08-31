cmake_minimum_required(VERSION 3.16)
project(VideoPlayer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Configure for dynamic builds
set(CMAKE_PREFIX_PATH "C:/Qt/6.9.2/mingw_64")

# Find required Qt components
find_package(Qt6 REQUIRED COMPONENTS 
    Core 
    Widgets 
    Multimedia 
    MultimediaWidgets
    Quick
    Qml
    QuickControls2
)

qt_standard_project_setup()

# Create the executable
qt_add_executable(qt_hello
    src/main.cpp
    src/videoplayer.cpp
    src/videoplayer.h
    src/clipboardmanager.cpp
    src/clipboardmanager.h
)

# Set up include directories
target_include_directories(qt_hello PRIVATE src)

# Add QML resources
qt_add_resources(qt_hello "qml_resources"
    PREFIX "/"
    FILES
        qml/VideoPlayerWindow.qml
        qml/VideoControls.qml
)

# Link Qt libraries
target_link_libraries(qt_hello PRIVATE 
    Qt6::Core
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    Qt6::Quick
    Qt6::Qml
    Qt6::QuickControls2
)
