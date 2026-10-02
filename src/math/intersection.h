#pragma once

#include "core/constants.h"
#include "core/platform.h"
#include "math/ray.h"
#include "math/vector.h"

namespace lambda {
    class Object;
    class Instance;

    class Intersection {
        public:
            LAMBDA_HOST_DEVICE Intersection() : object(nullptr), instance(nullptr), t(constants::MAX), frontFacing(true), useTransformed(false), isSurface(true) {}

            const Object * object;
            const Instance * instance;
            Float t;
            Vector<Float, 3> point, normal, tangent;
            Vector<Float, 3> transformedPoint, transformedNormal, transformedTangent;
            Vector<Float, 2> textureCoordinate;
            bool frontFacing, useTransformed, isSurface;
    };
}