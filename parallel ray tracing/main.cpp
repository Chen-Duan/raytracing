#include "rtweekend.h"

#include "camera.h"
#include "hittable_list.h"
#include "material.h"
#include "sphere.h"

#include <iostream>
#include <thread>
#include <vector>
#include <atomic>

double hit_sphere(const point3& center, double radius, const ray& r) {
    vec3 oc = center - r.origin();
    auto a = r.direction().length_squared();
    auto h = dot(r.direction(), oc);
    auto c = oc.length_squared() - radius*radius;
    auto discriminant = h*h - a*c;

    if (discriminant < 0) {
        return -1.0;
    } else {
        return (h - std::sqrt(discriminant)) / a;
    }
}

// color ray_color(const ray& r, const hittable& world) {
//     hit_record rec;
//     if (world.hit(r, interval(0, infinity), rec)) {
//             return 0.5 * (rec.normal + color(1,1,1));
//         }

//     vec3 unit_direction = unit_vector(r.direction());
//     auto a = 0.5*(unit_direction.y() + 1.0);
//     return (1.0-a)*color(1.0, 1.0, 1.0) + a*color(0.5, 0.7, 1.0);
// }

color ray_color(const ray& r, int depth, const hittable& world) {
    hit_record rec;

    // If we've exceeded the ray bounce limit, no more light is gathered.
    if (depth <= 0)
        return color(0,0,0);

    if (world.hit(r, interval(0.001, infinity), rec)) {
        ray scattered;
        color attenuation;
        
        // Get the emission color from the hit material
        color emitted_color = rec.mat->emitted();

        if (rec.mat->scatter(r, rec, attenuation, scattered)) {
            // A scattered ray contributes its color multiplied by the material's attenuation
            return emitted_color + attenuation * ray_color(scattered, depth - 1, world);
        }
        
        // If the ray doesn't scatter, we only get the emitted light
        return emitted_color;
    }

    // If the ray hits nothing, return the background color (black)
    return color(0,0,0);
}


int main() {

    // Image

    // auto aspect_ratio = 16.0 / 9.0;
    // int image_width = 400;

    // // Calculate the image height, and ensure that it's at least 1.
    // int image_height = int(image_width / aspect_ratio);
    // image_height = (image_height < 1) ? 1 : image_height;

    // World
    hittable_list world;

    auto material_ground = make_shared<lambertian>(color(0.8, 0.8, 0.0));
    auto material_center = make_shared<lambertian>(color(0.1, 0.2, 0.5));
    auto material_light  = make_shared<diffuse_light>(color(4,4,4));

    world.add(make_shared<sphere>(point3(0.0, -100.5, -1.0), 100.0, material_ground));
    world.add(make_shared<sphere>(point3(0.0,    0.0, -1.0),   0.5, material_center));
    world.add(make_shared<sphere>(point3(0.0,    1.5, -1.0),   0.5, material_light));

    // Camera
    camera cam;

    cam.aspect_ratio      = 16.0 / 9.0;
    cam.image_width       = 400;
    cam.samples_per_pixel = 100;
    cam.max_depth         = 50;  // Set a global maximum number of bounces

    cam.vfov     = 20;
    cam.lookfrom = point3(0,2,10);
    cam.lookat   = point3(0,0,0);
    cam.vup      = vec3(0,1,0);

    cam.render(world);
    // // Calculate the vectors across the horizontal and down the vertical viewport edges.
    // auto viewport_u = vec3(viewport_width, 0, 0);
    // auto viewport_v = vec3(0, -viewport_height, 0);

    // // Calculate the horizontal and vertical delta vectors from pixel to pixel.
    // auto pixel_delta_u = viewport_u / image_width;
    // auto pixel_delta_v = viewport_v / image_height;

    // // Calculate the location of the upper left pixel.
    // auto viewport_upper_left = camera_center
    //                          - vec3(0, 0, focal_length) - viewport_u/2 - viewport_v/2;
    // auto pixel00_loc = viewport_upper_left + 0.5 * (pixel_delta_u + pixel_delta_v);

    // // Render

    // std::cout << "P3\n" << image_width << ' ' << image_height << "\n255\n";

    // for (int j = 0; j < image_height; j++) {
    //     std::clog << "\rScanlines remaining: " << (image_height - j) << ' ' << std::flush;
    //     for (int i = 0; i < image_width; i++) {
    //         auto pixel_center = pixel00_loc + (i * pixel_delta_u) + (j * pixel_delta_v);
    //         auto ray_direction = pixel_center - camera_center;
    //         ray r(camera_center, ray_direction);

    //         color pixel_color = ray_color(r, world);
    //         write_color(std::cout, pixel_color);
    //     }
    // }

    // === Multi-threaded Render ===

    // Create a buffer in memory to hold the pixel data
    std::vector<color> image_buffer(cam.image_width * cam.image_height);
    std::atomic<int> completed_scanlines = 0;

    // Determine thread count and launch threads
    const int num_threads = std::thread::hardware_concurrency();
    std::vector<std::thread> threads;
    std::clog << "Starting render with " << num_threads << " threads.\n";

    int scanlines_per_thread = cam.image_height / num_threads;
    for (int t = 0; t < num_threads; ++t) {
        int start_scanline = t * scanlines_per_thread;
        int end_scanline = (t + 1 == num_threads) ? cam.image_height : start_scanline + scanlines_per_thread;
        
        threads.emplace_back(render_scanlines, image_buffer.data(), cam.image_width, cam.image_height,
                             start_scanline, end_scanline, std::cref(world), std::cref(cam),
                             std::ref(completed_scanlines));
    }

    // Progress reporting loop
    while (completed_scanlines < cam.image_height) {
        std::clog << "\rScanlines remaining: " << (cam.image_height - completed_scanlines) << ' ' << std::flush;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    // Wait for all rendering threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    std::clog << "\rRender complete. Writing to file...\n";

    // Write the final image from the buffer to the output file
    std::cout << "P3\n" << cam.image_width << " " << cam.image_height << "\n255\n";
    for (int j = 0; j < cam.image_height; ++j) {
        for (int i = 0; i < cam.image_width; ++i) {
            write_color(std::cout, image_buffer[j * cam.image_width + i], cam.samples_per_pixel);
        }
    }
    std::clog << "\rDone.                 \n";
}