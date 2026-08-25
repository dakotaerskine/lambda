#pragma once

#include <cassert>

#include "core/constants.h"
#include "core/platform.h"
#include "math/spectrum.h"
#include "math/vector.h"

class Ray {
    public:
        HOST_DEVICE Ray() {}
        HOST_DEVICE Ray(const Vector<Float> & o, const Vector<Float> & d, const SampledSpectrum & l) : origin(o), direction(d) {
            for (int i = 0; i < HERO_COUNT; i++)
                lambdas[i] = l[i];
        }

        HOST_DEVICE const Vector<Float> & getOrigin() const { return origin; }
        HOST_DEVICE const Vector<Float> & getDirection() const { return direction; }
        HOST_DEVICE const SampledSpectrum & getLambdas() const { return lambdas; }

        HOST_DEVICE Vector<Float> at(Float t) const { return origin + t * direction; }

    private:
        Vector<Float> origin, direction;
        SampledSpectrum lambdas;
};