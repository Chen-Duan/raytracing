#include "color.h"
#include "vec3.h"
#include "ray.h"
#include "hittable.h"
#include "material.h"
#include <iostream>
#include <vector>
#include <memory>
#include <thread>
#include <atomic>
#include <vector>
#include <algorithm>

// A simple sphere class
class sphere : public hittable {
  public:
    sphere(const point3& center, double radius, std::shared_ptr<material> mat)
      : center(center), radius(radius), mat(mat) {}

    bool hit(const ray& r, double ray_tmin, double ray_tmax, hit_record& rec) const override {
        vec3 oc = center - r.origin();
        auto a = r.direction().length_squared();
        auto h = dot(r.direction(), oc);
        auto c = oc.length_squared() - radius*radius;
        auto discriminant = h*h - a*c;

        if (discriminant < 0)
            return false;

        auto sqrtd = sqrt(discriminant);
        auto root = (h - sqrtd) / a;
        if (root <= ray_tmin || ray_tmax <= root) {
            root = (h + sqrtd) / a;
            if (root <= ray_tmin || ray_tmax <= root)
                return false;
        }

        rec.t = root;
        rec.p = r.at(rec.t);
        vec3 outward_normal = (rec.p - center) / radius;
        rec.set_face_normal(r, outward_normal);
        rec.mat = mat;

        return true;
    }

  private:
    point3 center;
    double radius;
    std::shared_ptr<material> mat;
};

// A list of hittable objects
class hittable_list : public hittable {
  public:
    std::vector<std::shared_ptr<hittable>> objects;

    hittable_list() {}
    hittable_list(std::shared_ptr<hittable> object) { add(object); }

    void clear() { objects.clear(); }
    void add(std::shared_ptr<hittable> object) {
        objects.push_back(object);
    }

    bool hit(const ray& r, double ray_tmin, double ray_tmax, hit_record& rec) const override {
        hit_record temp_rec;
        bool hit_anything = false;
        auto closest_so_far = ray_tmax;

        for (const auto& object : objects) {
            if (object->hit(r, ray_tmin, closest_so_far, temp_rec)) {
                hit_anything = true;
                closest_so_far = temp_rec.t;
                rec = temp_rec;
            }
        }

        return hit_anything;
    }
};

color ray_color(const ray& r, int depth, const hittable& world) {
    if (depth <= 0)
        return color(0,0,0);

    hit_record rec;

    if (world.hit(r, 0.001, std::numeric_limits<double>::infinity(), rec)) {
        ray scattered;
        color attenuation;
        color emission = rec.mat->emitted();

        if (rec.mat->scatter(r, rec, attenuation, scattered)) {
             return emission + attenuation * ray_color(scattered, depth-1, world);
        }

        return emission;
    }

    return color(0.0, 0.0, 0.0); // Black background
}

void render_tile(std::vector<color>& image_buffer, int image_width, int image_height,
                 int tile_y_start, int tile_y_end, int samples_per_pixel, int max_depth,
                 const hittable& world, const point3& camera_center, const vec3& pixel00_loc,
                 const vec3& pixel_delta_u, const vec3& pixel_delta_v, std::atomic<int>& scanlines_completed)
{
    for (int j = tile_y_start; j < tile_y_end; j++) {
        for (int i = 0; i < image_width; i++) {
            color pixel_color(0,0,0);
            for (int s = 0; s < samples_per_pixel; s++) {
                auto px = -0.5 + random_double();
                auto py = -0.5 + random_double();
                auto pixel_center = pixel00_loc + ((i + px) * pixel_delta_u) + ((j + py) * pixel_delta_v);
                auto ray_direction = pixel_center - camera_center;
                ray r(camera_center, ray_direction);
                pixel_color += ray_color(r, max_depth, world);
            }
            image_buffer[j * image_width + i] = pixel_color;
        }
        scanlines_completed++;
    }
}

int main() {
    // Image
    auto aspect_ratio = 16.0 / 9.0;
    int image_width = 400;
    int samples_per_pixel = 100;
    const int max_depth = 50; // Maximum number of bounces

    int image_height = int(image_width / aspect_ratio);
    image_height = (image_height < 1) ? 1 : image_height;

    // World
    hittable_list world;
    auto material_ground = std::make_shared<lambertian>(color(0.8, 0.8, 0.0));
    auto material_center = std::make_shared<lambertian>(color(0.1, 0.2, 0.5));
    auto material_light  = std::make_shared<diffuse_light>(color(4,4,4));

    world.add(std::make_shared<sphere>(point3( 0.0, -100.5, -1.0), 100.0, material_ground));
    world.add(std::make_shared<sphere>(point3( 0.0,    0.0, -1.0),   0.5, material_center));
    world.add(std::make_shared<sphere>(point3( 0.0,    1.0, -1.0),   0.5, material_light));

    // Camera
    auto focal_length = 1.0;
    auto viewport_height = 2.0;
    auto viewport_width = viewport_height * (double(image_width)/image_height);
    auto camera_center = point3(0, 0, 0);

    auto viewport_u = vec3(viewport_width, 0, 0);
    auto viewport_v = vec3(0, -viewport_height, 0);

    auto pixel_delta_u = viewport_u / image_width;
    auto pixel_delta_v = viewport_v / image_height;

    auto viewport_upper_left = camera_center - vec3(0, 0, focal_length) - viewport_u/2 - viewport_v/2;
    auto pixel00_loc = viewport_upper_left + 0.5 * (pixel_delta_u + pixel_delta_v);

    // Render
    std::vector<color> image_buffer(image_width * image_height);
    std::atomic<int> scanlines_completed = 0;

    const int num_threads = std::thread::hardware_concurrency();
    std::vector<std::thread> threads;
    int tile_height = image_height / num_threads;

    std::clog << "Starting render with " << num_threads << " threads.\n";

    for (int t = 0; t < num_threads; ++t) {
        int tile_y_start = t * tile_height;
        int tile_y_end = (t + 1 == num_threads) ? image_height : (t + 1) * tile_height;
        threads.emplace_back(render_tile, std::ref(image_buffer), image_width, image_height,
                             tile_y_start, tile_y_end, samples_per_pixel, max_depth,
                             std::cref(world), std::cref(camera_center), std::cref(pixel00_loc),
                             std::cref(pixel_delta_u), std::cref(pixel_delta_v), std::ref(scanlines_completed));
    }

    // Progress reporting thread
    std::thread progress_thread([&]() {
        while (scanlines_completed < image_height) {
            std::clog << "\rScanlines remaining: " << (image_height - scanlines_completed) << ' ' << std::flush;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });

    for (auto& thread : threads) {
        thread.join();
    }
    progress_thread.join();

    // Write final image from buffer
    std::cout << "P3\n" << image_width << ' ' << image_height << "\n255\n";
    for (int j = 0; j < image_height; ++j) {
        for (int i = 0; i < image_width; ++i) {
            write_color(std::cout, image_buffer[j * image_width + i], samples_per_pixel);
        }
    }

    std::clog << "\rDone.                 \n";

    return 0;
}