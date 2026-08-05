#pragma once

#include "platform.h"
#include "vector.h"

class Ray {
    public:
        HOST_DEVICE Ray() {}
        HOST_DEVICE Ray(const Vector & o, const Vector & d, Float l, int i) : origin(o), direction(d), lambda(l), lambdaIndex(i) {}

        HOST_DEVICE const Vector & getOrigin() const { return origin; }
        HOST_DEVICE const Vector & getDirection() const { return direction; }
        HOST_DEVICE Float getLambda() const { return lambda; }
        HOST_DEVICE int getLambdaIndex() const { return lambdaIndex; }

        HOST_DEVICE Vector at(Float t) const { return origin + t * direction; }

    private:
        Vector origin, direction;
        Float lambda;
        int lambdaIndex;
};