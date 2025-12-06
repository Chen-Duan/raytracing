#include <iostream>
#include <vector>
#include <random>
#include <cmath>
#include <float.h>

#include "vec3cpu.h" 
#include "raycpu.h"  

// ==========================================
// CPU Random Number Generator Helpers
// ==========================================
inline double random_double() {
    static std::uniform_real_distribution<double> distribution(0.0, 1.0);
    static std::mt19937 generator;
    return distribution(generator);
}

inline vec3 random_vec3() {
    return vec3(random_double(), random_double(), random_double());
}

vec3 random_in_unit_sphere() {
    vec3 p;
    do {
        p = 2.0f * random_vec3() - vec3(1,1,1);
    } while (p.length_squared() >= 1.0f);
    return p;
}

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
// Intersection Logic
// ==========================================
bool hit_sphere(const Sphere& s, const ray& r, float t_min, float t_max, float& out_t, vec3& out_normal) {
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
// The CPU Rendering Function
// ==========================================
// Replaces the __global__ kernel
void render_scene(std::vector<vec3>& fb, int max_x, int max_y, int ns, int max_depth, 
                  const std::vector<Sphere>& spheres, const std::vector<Material>& materials) {

    // Loop over every pixel (formerly threadIdx/blockIdx)
    for (int j = 0; j < max_y; j++) {
        for (int i = 0; i < max_x; i++) {
            
            int pixel_index = j * max_x + i;
            vec3 col(0, 0, 0);

            // Samples per pixel
            for (int s = 0; s < ns; s++) {
                float u = float(i + random_double()) / float(max_x);
                float v = float(j + random_double()) / float(max_y);

                vec3 origin(0.0, 0.0, 0.0);
                ray r(origin + vec3(u*4 - 2, v*2 - 1, -1), vec3(0,0,-1)); 

                vec3 cur_weight(1, 1, 1);
                vec3 cur_col(0, 0, 0);
                ray cur_ray = r;

                // Bounce loop
                for (int d = 0; d < max_depth; d++) {
                    float closest_so_far = FLT_MAX;
                    int hit_idx = -1;
                    vec3 hit_normal;
                    float t;

                    // Iterate over ALL spheres
                    for (size_t k = 0; k < spheres.size(); k++) {
                        if (hit_sphere(spheres[k], cur_ray, 0.001f, closest_so_far, t, hit_normal)) {
                            closest_so_far = t;
                            hit_idx = k;
                        }
                    }

                    if (hit_idx != -1) {
                        Material mat = materials[spheres[hit_idx].material_index];
                        cur_col += cur_weight * mat.emit;

                        if (mat.type == LIGHT) break; 

                        // Scatter
                        vec3 target = cur_ray.at(closest_so_far) + hit_normal + random_in_unit_sphere();
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
    }
}

// ==========================================
// Main Function
// ==========================================
int main() {
    int nx = 600;
    int ny = 337;
    int ns = 50; 
    int max_depth = 50;

    int num_pixels = nx * ny;
    
    // 1. Allocate CPU Memory
    std::vector<vec3> fb(num_pixels);

    // 2. Setup Scene (Just standard vectors now)
    std::vector<Material> materials;
    materials.push_back({LAMBERTIAN, vec3(0.5, 0.5, 0.5), vec3(0,0,0)}); 
    materials.push_back({LAMBERTIAN, vec3(0.7, 0.1, 0.1), vec3(0,0,0)}); 
    materials.push_back({LIGHT,      vec3(0,0,0),         vec3(15,15,15)}); 

    std::vector<Sphere> spheres;
    spheres.push_back({vec3(0, -100.5, -1), 100, 0});
    spheres.push_back({vec3(0, 0.5, -1.5), 0.5, 1});
    spheres.push_back({vec3(0, 2.0, -1.0), 0.3, 2});

    std::cerr << "Rendering on CPU... ";

    // 3. Render
    render_scene(fb, nx, ny, ns, max_depth, spheres, materials);
    
    std::cerr << "Done.\n";

    // 4. Output PPM
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
}
