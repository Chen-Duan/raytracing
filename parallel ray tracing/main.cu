#include <iostream>
#include <vector>
#include <cuda_runtime.h>
#include <curand_kernel.h>
#include <float.h>

#include "vec3.h" 
#include "ray.h"  

// ==========================================
// 1. Data Structures (Flattened for GPU)
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
// 2. CUDA Kernel Helpers
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
// 3. The Rendering Kernel (With Shared Memory!)
// ==========================================
__global__ void render_kernel(vec3 *fb, int max_x, int max_y, int ns, int max_depth, Sphere *d_spheres, int num_spheres, Material *d_materials) {
    
    // We load the scene (spheres) into fast on-chip memory (L1 Cache/Shared)
    // This reduces global memory reads during the intense ray intersection loop.
    extern __shared__ Sphere shared_spheres[]; 

    int tid = threadIdx.x + threadIdx.y * blockDim.x;
    if (tid < num_spheres) {
        shared_spheres[tid] = d_spheres[tid];
    }
    __syncthreads(); // Wait for all threads to load the scene
    // -------------------------------------------

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
        
        // Simple Camera (Hardcoded for "One Weekend" setup)
        vec3 lookfrom(-3, 2, 3);
        vec3 lookat(0, 0.5, -1.0);
        float dist_to_focus = (lookfrom - lookat).length();
        float aperture = 0.0f; // No blur for simplicity
        
        vec3 lower_left_corner(-2.0, -1.0, -1.0);
        vec3 horizontal(4.0, 0.0, 0.0);
        vec3 vertical(0.0, 2.0, 0.0);
        vec3 origin(0.0, 0.0, 0.0);
        ray r(origin + vec3(u*4 - 2, v*2 - 1, -1) /* simplified projection */, vec3(0,0,-1)); 

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
                // Hit something
                Material mat = d_materials[shared_spheres[hit_idx].material_index];
                
                // Add emission
                cur_col += cur_weight * mat.emit;

                if (mat.type == LIGHT) {
                    break; // Lights don't scatter
                }

                // Scatter (Lambertian)
                vec3 target = cur_ray.at(closest_so_far) + hit_normal + random_in_unit_sphere(&local_rand_state);
                cur_ray = ray(cur_ray.at(closest_so_far), target - cur_ray.at(closest_so_far));
                cur_weight = cur_weight * mat.albedo;
            } else {
                // Background (Sky)
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
// 4. Main Host Code
// ==========================================
int main() {
    int nx = 600;
    int ny = 337; // 16:9
    int ns = 100; // Samples per pixel
    int max_depth = 50;

    // Allocate Framebuffer
    int num_pixels = nx * ny;
    size_t fb_size = num_pixels * sizeof(vec3);
    vec3 *fb;
    cudaMallocManaged((void **)&fb, fb_size);

    // Setup Scene (Spheres + Materials)
    std::vector<Material> cpu_mats;
    cpu_mats.push_back({LAMBERTIAN, vec3(0.5, 0.5, 0.5), vec3(0,0,0)}); // Ground
    cpu_mats.push_back({LAMBERTIAN, vec3(0.7, 0.1, 0.1), vec3(0,0,0)}); // Center
    cpu_mats.push_back({LIGHT,      vec3(0,0,0),         vec3(15,15,15)}); // Light

    std::vector<Sphere> cpu_spheres;
    cpu_spheres.push_back({vec3(0, -100.5, -1), 100, 0});
    cpu_spheres.push_back({vec3(0, 0.5, -1.5), 0.5, 1});
    cpu_spheres.push_back({vec3(0, 2.0, -1.0), 0.3, 2});

    // Copy to GPU
    Material *d_materials;
    Sphere *d_spheres;
    cudaMalloc(&d_materials, cpu_mats.size() * sizeof(Material));
    cudaMalloc(&d_spheres, cpu_spheres.size() * sizeof(Sphere));
    cudaMemcpy(d_materials, cpu_mats.data(), cpu_mats.size() * sizeof(Material), cudaMemcpyHostToDevice);
    cudaMemcpy(d_spheres, cpu_spheres.data(), cpu_spheres.size() * sizeof(Sphere), cudaMemcpyHostToDevice);

    // Launch Config
    dim3 blocks(8, 8);
    dim3 grids((nx + blocks.x - 1) / blocks.x, (ny + blocks.y - 1) / blocks.y);

    std::cerr << "Rendering... ";
    
    // Dynamic Shared Memory Size = num_spheres * sizeof(Sphere)
    render_kernel<<<grids, blocks, cpu_spheres.size() * sizeof(Sphere)>>>(fb, nx, ny, ns, max_depth, d_spheres, cpu_spheres.size(), d_materials);
    
    cudaDeviceSynchronize();
    std::cerr << "Done.\n";

    // Output PPM
    std::cout << "P3\n" << nx << " " << ny << "\n255\n";
    for (int j = ny-1; j >= 0; j--) {
        for (int i = 0; i < nx; i++) {
            size_t pixel_index = j * nx + i;
            int ir = int(255.99 * sqrt(fb[pixel_index].x()));
            int ig = int(255.99 * sqrt(fb[pixel_index].y()));
            int ib = int(255.99 * sqrt(fb[pixel_index].z()));
            std::cout << ir << " " << ig << " " << ib << "\n";
        }
    }

    cudaFree(fb);
    cudaFree(d_materials);
    cudaFree(d_spheres);
}
