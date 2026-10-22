cmake_minimum_required(VERSION 3.21)

project(clipboard-linux
    VERSION 0.1.0
    DESCRIPTION "Clipboard history manager for Linux, inspired by Windows Win+V"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets)

add_executable(clipboard-linux
    src/main.cpp
)

target_include_directories(clipboard-linux PRIVATE include)

target_compile_definitions(clipboard-linux PRIVATE
    APP_VERSION="${PROJECT_VERSION}")

target_compile_options(clipboard-linux PRIVATE -Wall -Wextra -Wpedantic)

target_link_libraries(clipboard-linux PRIVATE Qt6::Widgets)
