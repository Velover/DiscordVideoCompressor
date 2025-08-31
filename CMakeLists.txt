cmake_minimum_required(VERSION 3.16)
project(qt_hello LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_PREFIX_PATH "C:/Qt/6.9.2/mingw_64")

find_package(Qt6 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()

add_executable(qt_hello src/main.cpp)
target_link_libraries(qt_hello PRIVATE Qt6::Widgets)
