#ifndef COLOR_H
#define COLOR_H

#include "vec3.h"
#include <iostream>
#include <algorithm>

using color = vec3;

inline double linear_to_gamma(double linear_component) {
    return sqrt(linear_component);
}

void write_color(std::ostream& out, const color& pixel_color, int samples_per_pixel) {
    auto r = pixel_color.x();
    auto g = pixel_color.y();
    auto b = pixel_color.z();

    // Divide the color by the number of samples.
    auto scale = 1.0 / samples_per_pixel;
    r *= scale;
    g *= scale;
    b *= scale;

    // Apply the gamma correction.
    r = linear_to_gamma(r);
    g = linear_to_gamma(g);
    b = linear_to_gamma(b);

    // Translate the [0,1] component values to the byte range [0,255].
    int rbyte = int(256 * std::clamp(r, 0.0, 0.999));
    int gbyte = int(256 * std::clamp(g, 0.0, 0.999));
    int bbyte = int(256 * std::clamp(b, 0.0, 0.999));


    // Write out the pixel color components.
    out << rbyte << ' ' << gbyte << ' ' << bbyte << '\n';
}

#endif
