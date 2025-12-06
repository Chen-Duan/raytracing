#!/bin/bash
#SBATCH --job-name=cuda_trace
#SBATCH --nodes=1
#SBATCH --gres=gpu:1
#SBATCH --time=00:10:00
#SBATCH --output=result.out
#SBATCH --error=result.err

# --- 1. Load Modules (CRITICAL) ---
# The compute node needs these to run cmake and nvcc
module purge
module load cmake/3.31.6
module load gcc/11.3.0
module load CUDA/12.4.0
module load nvhpc/24.11-nompi

# --- 2. Build the Project ---
# We build inside the job to ensure the binary matches the compute node's GPU
cd build
cmake ..
make

# --- 3. Run & Profile ---
echo "Starting execution..."

# Option A: Just run it (Uncomment if you just want the image)
./pathtracer > image.ppm

# --- REQUIREMENT 2: Performance Timeline (nsys) ---
# This generates 'timeline_report.nsys-rep'
# We send stdout to /dev/null because we don't need the image data for profiling
echo "Profiling Timeline..."
nsys profile --trace=cuda,osrt --output=timeline_report --force-overwrite true ./pathtracer > /dev/null

# --- REQUIREMENT 3: Kernel Analysis (ncu) ---
# This generates 'kernel_report.ncu-rep'
echo "Profiling Kernels..."
ncu --set full --output=kernel_report --force-overwrite ./pathtracer > /dev/null
