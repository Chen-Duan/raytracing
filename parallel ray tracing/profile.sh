#!/bin/bash
#SBATCH --job-name=cuda_trace
#SBATCH --nodes=1
#SBATCH --gres=gpu:1
#SBATCH --time=01:00:00
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
mkdir -p build && cd build
cmake ..
make
# --- 3. Run & Profile ---
echo "Starting CPU profiling..."

# CHANGE 2: Updated Nsight Systems (nsys) command
# - Removed "cuda" from --trace (since there is no CUDA).
# - Added "--sample=cpu" (CRITICAL: this records call stacks to see which functions take time).
# - Changed output filename to "timeline_cpu".
echo "Profiling Timeline..."
nsys profile \
    --trace=osrt \
    --sample=cpu \
    --output=timeline_cpu \
    --force-overwrite true \
    ./pathtracer > /dev/null
