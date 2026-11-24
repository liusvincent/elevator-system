cmake_minimum_required(VERSION 3.20)
project(elevator-system)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(external/SDL)

add_library(elevator_core
    src/elevator.cpp
    src/dispatcher.cpp
)

target_include_directories(elevator_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(elevator-system src/main.cpp)

target_link_libraries(elevator-system PRIVATE elevator_core SDL3::SDL3)

