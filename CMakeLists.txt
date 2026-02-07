cmake_minimum_required(VERSION 3.20)
project(elvatorsystem)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(external/SDL)

add_executable(elevatorsystem src/main.cpp)

target_link_libraries(elevatorsystem PRIVATE SDL3::SDL3)

