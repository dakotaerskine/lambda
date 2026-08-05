#pragma once

#include "constants.h"
#include "platform.h"
#include "ray.h"
#include "vector.h"

class Intersection {
    public:
        HOST_DEVICE Intersection() : i(-1), t(T_MAX), frontFacing(false) {}

        int i;
        Float t;
        Vector point, normal;
        Float u, v;
        bool frontFacing;
};