#pragma once

#include "ray.cuh"
#include "vector.cuh"

constexpr double T_MAX = 1e30; 

class Intersection {
    public:
        __host__ __device__ Intersection() : t(T_MAX) {}

        double t;
        Vector point, normal;
        bool frontFacing;
};