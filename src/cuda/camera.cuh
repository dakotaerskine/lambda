#pragma once

#include "ray.cuh"
#include "vector.cuh"

class Camera {
    public:
        __host__ __device__ Camera() {}
        __host__ __device__ Camera(const Vector & p, const Vector & c, const Vector & h, const Vector & v) : position(p), corner(c), horizontal(h), vertical(v) {}

        __host__ __device__ Ray getRay(double u, double v, double lambda, int lambdaIndex) const {
            Vector direction = corner + horizontal * u + vertical * v - position;
            direction.normalize();
            return Ray(position, direction, lambda, lambdaIndex);
        }

    private:
        Vector position, corner, horizontal, vertical;
};