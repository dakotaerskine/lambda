#pragma once

#include "core/platform.h"
#include "math/ray.h"
#include "math/spectrum.h"
#include "math/vector.h"

class Camera {
    public:
        HOST_DEVICE Camera() {}
        HOST_DEVICE Camera(const Vector & p, const Vector & c, const Vector & h, const Vector & v) : position(p), corner(c), horizontal(h), vertical(v) {}

        HOST_DEVICE Ray getRay(Float u, Float v, const SampledSpectrum & lambdas) const {
            Vector direction = normalize(corner + horizontal * u + vertical * v - position);
            return Ray(position, direction, lambdas);
        }

    private:
        Vector position, corner, horizontal, vertical;
};