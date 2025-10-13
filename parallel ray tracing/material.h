#ifndef MATERIAL_H
#define MATERIAL_H

#include "rtweekend.h"
// #include "ray.h"
// #include "vec3.h"

class hit_record;

class material {
  public:
    virtual ~material() = default;

    // A function to handle emitted light. Non-emitting materials return black.
    virtual color emitted() const {
        return color(0, 0, 0);
    }

    virtual bool scatter(
        const ray& r_in, const hit_record& rec, color& attenuation, ray& scattered
    ) const = 0;
};

class lambertian : public material {
  public:
    lambertian(const color& a) : albedo(a) {}

    bool scatter(const ray& r_in, const hit_record& rec, color& attenuation, ray& scattered)
    const override {
        auto scatter_direction = rec.normal + random_unit_vector();

        // Catch degenerate scatter direction
        if (scatter_direction.length_squared() < 1e-8)
            scatter_direction = rec.normal;

        scattered = ray(rec.p, scatter_direction);
        attenuation = albedo;
        return true;
    }

  private:
    color albedo;
};

// New material for emissive objects (lights)
class diffuse_light : public material {
  public:
    diffuse_light(const color& a) : emit(a) {}

    bool scatter(const ray& r_in, const hit_record& rec, color& attenuation, ray& scattered)
    const override {
        return false; // Lights do not scatter rays
    }

    color emitted() const override {
        return emit; // Return the emission color
    }

  private:
    color emit;
};

#endif