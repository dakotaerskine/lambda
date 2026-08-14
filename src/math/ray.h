#pragma once

#include <cassert>

#include "core/constants.h"
#include "core/platform.h"
#include "math/spectrum.h"
#include "math/vector.h"

class Ray {
    public:
        HOST_DEVICE Ray() {}
        HOST_DEVICE Ray(const Vector & o, const Vector & d, const SampledSpectrum & l) : origin(o), direction(d) {
            for (int i = 0; i < HERO_COUNT; i++)
                lambdas[i] = l[i];
        }

        HOST_DEVICE const Vector & getOrigin() const { return origin; }
        HOST_DEVICE const Vector & getDirection() const { return direction; }
        HOST_DEVICE const SampledSpectrum & getLambdas() const { return lambdas; }

        HOST_DEVICE Vector at(Float t) const { return origin + t * direction; }

    private:
        Vector origin, direction;
        SampledSpectrum lambdas;
};