cmake_minimum_required(VERSION 3.10)
project(PathTracerCUDA LANGUAGES CXX CUDA)

set(CMAKE_CUDA_STANDARD 14)
# Added 50 and 52 for older Maxwell GPUs often found in clusters
set(CMAKE_CUDA_ARCHITECTURES 50 52 60 70 75 80) 

include_directories(${CMAKE_SOURCE_DIR})

# --- THE FIX: Explicitly list your file instead of using GLOB ---
# file(GLOB sources *.cu) <--- DELETE THIS LINE
add_executable(pathtracer main.cu)

target_compile_options(pathtracer PRIVATE $<$<COMPILE_LANGUAGE:CUDA>:-O3 -use_fast_math>)
