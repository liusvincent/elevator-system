cmake_minimum_required(VERSION 3.20)
project(elevator-system)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(external/SDL)

add_executable(elevator-system src/main.cpp)

target_link_libraries(elevator-system PRIVATE SDL3::SDL3)

