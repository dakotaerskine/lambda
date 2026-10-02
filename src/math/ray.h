#pragma once

#include "core/constants.h"
#include "core/platform.h"
#include "math/spectrum.h"
#include "math/vector.h"

namespace lambda {
    class Medium;

    class Ray {
        public:
            LAMBDA_HOST_DEVICE Ray() : direction(0, 1, 0) {}

            LAMBDA_HOST_DEVICE Ray(const Vector<Float, 3> & o, const Vector<Float, 3> & d, const SampledSpectrum & l, const Medium * m = nullptr) : origin(o), direction(d), lambdas(l), medium(m) {}

            LAMBDA_HOST_DEVICE void setMedium(const Medium * m) { medium = m; }

            LAMBDA_HOST_DEVICE const Vector<Float, 3> & getOrigin() const { return origin; }

            LAMBDA_HOST_DEVICE const Vector<Float, 3> & getDirection() const { return direction; }

            LAMBDA_HOST_DEVICE const SampledSpectrum & getLambdas() const { return lambdas; }

            LAMBDA_HOST_DEVICE const Medium * getMedium() const { return medium; }

            LAMBDA_HOST_DEVICE Vector<Float, 3> at(Float t) const { return origin + t * direction; }

        private:
            Vector<Float, 3> origin, direction;
            SampledSpectrum lambdas;
            const Medium * medium;
    };
}