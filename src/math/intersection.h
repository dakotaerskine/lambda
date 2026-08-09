#pragma once

#include "core/constants.h"
#include "core/platform.h"
#include "math/ray.h"
#include "math/vector.h"

class Intersection {
    public:
        HOST_DEVICE Intersection() : i(-1), t(T_MAX), frontFacing(false) {}

        int i;
        Float t;
        Vector point, normal;
        Float u, v;
        bool frontFacing;
};