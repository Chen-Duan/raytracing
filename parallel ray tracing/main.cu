#include <iostream>
#include <vector>
#include <cuda_runtime.h>
#include <curand_kernel.h>
#include <float.h>

// Keep your existing headers
#include "vec3.h" 
#include "ray.h"  

// ==========================================
// Data Structures
// ==========================================
enum MaterialType { LAMBERTIAN, LIGHT };

struct Material {
    MaterialType type;
    vec3 albedo;
    vec3 emit;
};

struct Sphere {
    vec3 center;
    float radius;
    int material_index;
};

// ==========================================
// Kernel Helpers
// ==========================================
#define RANDVEC3 vec3(curand_uniform(local_rand_state), curand_uniform(local_rand_state), curand_uniform(local_rand_state))

__device__ vec3 random_in_unit_sphere(curandState *local_rand_state) {
    vec3 p;
    do {
        p = 2.0f * RANDVEC3 - vec3(1,1,1);
    } while (p.length_squared() >= 1.0f);
    return p;
}

__device__ bool hit_sphere(const Sphere& s, const ray& r, float t_min, float t_max, float& out_t, vec3& out_normal) {
    vec3 oc = r.origin() - s.center;
    float a = dot(r.direction(), r.direction());
    float b = dot(oc, r.direction());
    float c = dot(oc, oc) - s.radius * s.radius;
    float discriminant = b * b - a * c;

    if (discriminant > 0) {
        float temp = (-b - sqrt(discriminant)) / a;
        if (temp < t_max && temp > t_min) {
            out_t = temp;
            out_normal = (r.at(out_t) - s.center) / s.radius;
            return true;
        }
    }
    return false;
}

// ==========================================
// The Rendering Kernel
// ==========================================
__global__ void render_kernel(vec3 *fb, int max_x, int max_y, int ns, int max_depth, Sphere *d_spheres, int num_spheres, Material *d_materials) {
    
    extern __shared__ Sphere shared_spheres[]; 

    int tid = threadIdx.x + threadIdx.y * blockDim.x;
    if (tid < num_spheres) {
        shared_spheres[tid] = d_spheres[tid];
    }
    __syncthreads(); 

    int i = threadIdx.x + blockIdx.x * blockDim.x;
    int j = threadIdx.y + blockIdx.y * blockDim.y;

    if ((i >= max_x) || (j >= max_y)) return;

    int pixel_index = j * max_x + i;
    curandState local_rand_state;
    curand_init(1984 + pixel_index, 0, 0, &local_rand_state);

    vec3 col(0, 0, 0);

    for (int s = 0; s < ns; s++) {
        float u = float(i + curand_uniform(&local_rand_state)) / float(max_x);
        float v = float(j + curand_uniform(&local_rand_state)) / float(max_y);
        
        // Simple Camera
        vec3 origin(0.0, 0.0, 0.0);
        ray r(origin + vec3(u*4 - 2, v*2 - 1, -1), vec3(0,0,-1)); 

        vec3 cur_weight(1, 1, 1);
        vec3 cur_col(0, 0, 0);
        ray cur_ray = r;

        for (int d = 0; d < max_depth; d++) {
            float closest_so_far = FLT_MAX;
            int hit_idx = -1;
            vec3 hit_normal;
            float t;

            // Iterate over SHARED memory spheres
            for (int k = 0; k < num_spheres; k++) {
                if (hit_sphere(shared_spheres[k], cur_ray, 0.001f, closest_so_far, t, hit_normal)) {
                    closest_so_far = t;
                    hit_idx = k;
                }
            }

            if (hit_idx != -1) {
                Material mat = d_materials[shared_spheres[hit_idx].material_index];
                cur_col += cur_weight * mat.emit;

                if (mat.type == LIGHT) break; 

                vec3 target = cur_ray.at(closest_so_far) + hit_normal + random_in_unit_sphere(&local_rand_state);
                cur_ray = ray(cur_ray.at(closest_so_far), target - cur_ray.at(closest_so_far));
                cur_weight = cur_weight * mat.albedo;
            } else {
                // Background
                vec3 unit_direction = unit_vector(cur_ray.direction());
                float t = 0.5f * (unit_direction.y() + 1.0f);
                vec3 sky = (1.0f - t) * vec3(1.0, 1.0, 1.0) + t * vec3(0.5, 0.7, 1.0);
                cur_col += cur_weight * sky;
                break;
            }
        }
        col += cur_col;
    }

    fb[pixel_index] = col / float(ns);
}

// ==========================================
// Main Function
// ==========================================
int main() {
    int nx = 600;
    int ny = 337;
    int ns = 50; // Samples per pixel
    int max_depth = 50;

    int num_pixels = nx * ny;
    size_t fb_size = num_pixels * sizeof(vec3);

    // --- 1. Allocate GPU Memory (Device) ---
    vec3 *fb_device;
    cudaError_t err = cudaMalloc((void **)&fb_device, fb_size);
    if(err != cudaSuccess) { std::cerr << "Error Malloc: " << cudaGetErrorString(err) << std::endl; return 1; }

    // --- 2. Allocate CPU Memory (Host) ---
    // This is the missing piece that solves the garbage output!
    vec3 *fb_host = (vec3 *)malloc(fb_size);

    // Setup Scene
    std::vector<Material> cpu_mats;
    cpu_mats.push_back({LAMBERTIAN, vec3(0.5, 0.5, 0.5), vec3(0,0,0)}); 
    cpu_mats.push_back({LAMBERTIAN, vec3(0.7, 0.1, 0.1), vec3(0,0,0)}); 
    cpu_mats.push_back({LIGHT,      vec3(0,0,0),         vec3(15,15,15)}); 

    std::vector<Sphere> cpu_spheres;
    cpu_spheres.push_back({vec3(0, -100.5, -1), 100, 0});
    cpu_spheres.push_back({vec3(0, 0.5, -1.5), 0.5, 1});
    cpu_spheres.push_back({vec3(0, 2.0, -1.0), 0.3, 2});

    Material *d_materials;
    Sphere *d_spheres;
    cudaMalloc(&d_materials, cpu_mats.size() * sizeof(Material));
    cudaMalloc(&d_spheres, cpu_spheres.size() * sizeof(Sphere));
    cudaMemcpy(d_materials, cpu_mats.data(), cpu_mats.size() * sizeof(Material), cudaMemcpyHostToDevice);
    cudaMemcpy(d_spheres, cpu_spheres.data(), cpu_spheres.size() * sizeof(Sphere), cudaMemcpyHostToDevice);

    dim3 blocks(8, 8);
    dim3 grids((nx + blocks.x - 1) / blocks.x, (ny + blocks.y - 1) / blocks.y);

    std::cerr << "Rendering... ";
    
    // Launch kernel using DEVICE pointer
    render_kernel<<<grids, blocks, cpu_spheres.size() * sizeof(Sphere)>>>(fb_device, nx, ny, ns, max_depth, d_spheres, cpu_spheres.size(), d_materials);
    
    cudaDeviceSynchronize();

    // --- 3. COPY GPU -> CPU ---
    // This ensures we have the actual data before printing
    cudaMemcpy(fb_host, fb_device, fb_size, cudaMemcpyDeviceToHost);
    
    std::cerr << "Done.\n";

    // Output PPM using HOST pointer
    std::cout << "P3\n" << nx << " " << ny << "\n255\n";
    for (int j = ny-1; j >= 0; j--) {
        for (int i = 0; i < nx; i++) {
            size_t pixel_index = j * nx + i;
            // Read from fb_host (CPU RAM)
            int ir = int(255.99 * sqrt(fb_host[pixel_index].x()));
            int ig = int(255.99 * sqrt(fb_host[pixel_index].y()));
            int ib = int(255.99 * sqrt(fb_host[pixel_index].z()));
            std::cout << ir << " " << ig << " " << ib << "\n";
        }
    }

    cudaFree(fb_device);
    cudaFree(d_materials);
    cudaFree(d_spheres);
    free(fb_host); // Don't forget to free CPU memory
}
