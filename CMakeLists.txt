cmake_minimum_required(VERSION 3.5)


## Project

project(emu-minimal)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules)


## MiniFB

set(MINIFB_BUILD_EXAMPLES OFF CACHE INTERNAL "")
add_subdirectory(lib/minifb)


## Project targets

add_executable(emu-minimal main.cpp)
target_link_libraries(emu-minimal minifb)
set_property(TARGET emu-minimal PROPERTY CXX_STANDARD 17)
