#!/bin/bash
#SBATCH --job-name=cuda_trace
#SBATCH --nodes=1
#SBATCH --gres=gpu:1
#SBATCH --time=00:10:00
#SBATCH --output=result.out

module load cuda
mkdir -p build && cd build
cmake ..
make

# 1. Profile Timeline (CPU + GPU interactions)
nsys profile --trace=cuda,osrt --output=timeline_report ./pathtracer > image.ppm

# 2. Profile Kernels (Memory throughput, Shared Memory usage)
ncu --set full --output=kernel_report ./pathtracer > image.ppm
