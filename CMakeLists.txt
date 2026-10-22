cmake_minimum_required(VERSION 3.21)

project(clipboard-linux
    VERSION 0.1.0
    DESCRIPTION "Clipboard history manager for Linux, inspired by Windows Win+V"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_AUTOMOC ON)

option(BUILD_TESTING "Build the unit tests" ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Sql)

add_executable(clipboard-linux
    include/HistoryStore.h
    src/HistoryStore.cpp
    src/main.cpp
)

target_include_directories(clipboard-linux PRIVATE include)

target_compile_definitions(clipboard-linux PRIVATE
    APP_VERSION="${PROJECT_VERSION}")

target_compile_options(clipboard-linux PRIVATE -Wall -Wextra -Wpedantic)

target_link_libraries(clipboard-linux PRIVATE Qt6::Widgets Qt6::Sql)

if(BUILD_TESTING)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test)

    add_executable(test_history_store
        tests/test_history_store.cpp
        include/HistoryStore.h
        src/HistoryStore.cpp
    )
    target_include_directories(test_history_store PRIVATE include)
    target_link_libraries(test_history_store PRIVATE Qt6::Sql Qt6::Test)
    add_test(NAME test_history_store COMMAND test_history_store)
endif()
