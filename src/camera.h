#pragma once

#include "platform.h"
#include "ray.h"
#include "vector.h"

class Camera {
    public:
        HOST_DEVICE Camera() {}
        HOST_DEVICE Camera(const Vector & p, const Vector & c, const Vector & h, const Vector & v) : position(p), corner(c), horizontal(h), vertical(v) {}

        HOST_DEVICE Ray getRay(Float u, Float v, Float lambda) const {
            Vector direction = (corner + horizontal * u + vertical * v - position).normalize();
            return Ray(position, direction, lambda);
        }

    private:
        Vector position, corner, horizontal, vertical;
};