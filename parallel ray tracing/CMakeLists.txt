cmake_minimum_required(VERSION 3.10)
project(PathTracerCPU LANGUAGES CXX) # <--- Make sure CUDA is gone from here

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_SOURCE_DIR})

add_executable(pathtracer main.cpp)

# Simple optimization flags for CPU
target_compile_options(pathtracer PRIVATE -O3)
