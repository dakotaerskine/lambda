#pragma once

#include "vector.cuh"

class Ray {
    public:
        __host__ __device__ Ray() {}
        __host__ __device__ Ray(const Vector & o, const Vector & d, double l, int i) : origin(o), direction(d), lambda(l), lambdaIndex(i) {}

        __host__ __device__ const Vector & getOrigin() const { return origin; }
        __host__ __device__ const Vector & getDirection() const { return direction; }
        __host__ __device__ double getLambda() const { return lambda; }
        __host__ __device__ int getLambdaIndex() const { return lambdaIndex; }

        __host__ __device__ Vector at(double t) const { return origin + t * direction; }

    private:
        Vector origin, direction;
        double lambda;
        int lambdaIndex;
};