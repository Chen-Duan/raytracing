cmake_minimum_required(VERSION 3.10)
project(PathTracerCUDA LANGUAGES CXX CUDA)

set(CMAKE_CUDA_STANDARD 14)
set(CMAKE_CUDA_ARCHITECTURES 60 70 75 80) # Targets typical University Cluster GPUs (Pascal, Volta, Turing, Ampere)

include_directories(${CMAKE_SOURCE_DIR})

# Find all CUDA files
file(GLOB sources *.cu)
add_executable(pathtracer main.cu)

# Optimization flags are critical for the "Speedup" report
target_compile_options(pathtracer PRIVATE $<$<COMPILE_LANGUAGE:CUDA>:-O3 -use_fast_math>)
