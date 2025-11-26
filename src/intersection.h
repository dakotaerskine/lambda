#ifndef INTERSECTION_H
#define INTERSECTION_H

#include <cmath>

#include "ray.h"
#include "vector.h"

class Material;

class Intersection {
    public:
        Intersection() : t(INFINITY) {}

        double t;
        Vector point, normal;
        bool frontFacing;
        Material * material;
};

#endif