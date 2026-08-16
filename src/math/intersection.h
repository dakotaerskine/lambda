#pragma once

#include "core/constants.h"
#include "core/platform.h"
#include "math/ray.h"
#include "math/vector.h"

class Intersection {
    public:
        HOST_DEVICE Intersection() : instance(-1), object(-1), t(MAX), frontFacing(false) {}

        int instance, object;
        Float t;
        Vector point, normal;
        Float u, v;
        bool frontFacing;
};