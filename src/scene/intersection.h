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
            LAMBDA_HOST_DEVICE Intersection() : object(nullptr), instance(nullptr), i(-1), t(constants::MAX), pointError(0), frontFacing(true), useTransformed(false), isSurface(true) {}

            const Object * object;
            const Instance * instance;
            int i;
            Float t;
            Vector<Float, 3> point, normal, tangent, transformedPoint, transformedNormal, transformedTangent, geometricNormal;
            Vector<Float, 2> textureCoordinate;
            Float pointError;
            bool frontFacing, useTransformed, isSurface;
    };
}