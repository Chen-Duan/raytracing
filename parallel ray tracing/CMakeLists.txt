cmake_minimum_required(VERSION 3.10)

project(PathTracer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

add_executable(pathtracer main.cpp)

target_link_libraries(pathtracer PRIVATE Threads::Threads)

target_compile_options(pathtracer PRIVATE
    $<$<CONFIG:Release>:-O3>
)