cmake_minimum_required(VERSION 3.21)

project(colibry-clock
    VERSION 0.2.0
    DESCRIPTION "Clock for the Colibry"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(Colibry_BUILD_TESTS "Build unit tests" ON)
option(Colibry_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

set(_qt_components Widgets)
if(Colibry_BUILD_TESTS)
    list(APPEND _qt_components Test)
endif()
find_package(Qt6 6.4 REQUIRED COMPONENTS ${_qt_components})

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(ColibryApps)

if(Colibry_BUILD_TESTS)
    enable_testing()
endif()

add_subdirectory(common)
add_library(gus_clock_core STATIC
    src/core/TimeSource.h
    src/core/Timers.h
    src/core/Timers.cpp
    src/core/Alarms.h
    src/core/Alarms.cpp
    src/core/ClockStore.h
    src/core/ClockStore.cpp
    src/core/WorldClock.h
    src/core/WorldClock.cpp
)
target_include_directories(gus_clock_core PUBLIC src)
target_link_libraries(gus_clock_core PUBLIC Qt6::Widgets)
colibry_target_defaults(gus_clock_core)

add_library(gus_clock_ui STATIC
    src/ClockWindow.h
    src/ClockWindow.cpp
)
target_include_directories(gus_clock_ui PUBLIC src)
target_link_libraries(gus_clock_ui PUBLIC gus_clock_core colibry_common Qt6::Widgets)
colibry_target_defaults(gus_clock_ui)

add_executable(colibry-clock src/main.cpp)
target_link_libraries(colibry-clock PRIVATE gus_clock_ui)
colibry_target_defaults(colibry-clock)

colibry_install_app(colibry-clock
    ${CMAKE_CURRENT_SOURCE_DIR}/packaging/colibry-clock.desktop
    ${CMAKE_CURRENT_SOURCE_DIR}/packaging/colibry-clock.svg)

if(Colibry_BUILD_TESTS)
    colibry_add_test(clock_core SOURCES tests/test_core.cpp LINK gus_clock_core)
    colibry_add_test(clock_ui SOURCES tests/test_ui.cpp LINK gus_clock_ui)
endif()
