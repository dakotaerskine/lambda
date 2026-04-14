#pragma once

#include "intersection.cuh"
#include "material.cuh"
#include "ray.cuh"
#include "utils.cuh"
#include "vector.cuh"

class Object {
    public:
        __host__ __device__ Object() {}
        __host__ __device__ Object(const Material & m, const Vector & p, const Vector & n) : material(m), point(p), normal(n) {}
        
        __host__ __device__ const Material & getMaterial() const { return material; }

        __host__ __device__ bool intersect(const Ray & r, Intersection & intersection) const {
            double denominator = normal.dot(r.getDirection());

            if (fabs(denominator) < EPSILON) return false;

            double t = normal.dot(point - r.getOrigin()) / denominator;

            if (t < EPSILON) return false;

            intersection.t = t;
            intersection.point = r.at(t);
            intersection.normal = normal;
            intersection.frontFacing = r.getDirection().dot(intersection.normal) < 0;
            if (!intersection.frontFacing) intersection.normal *= -1;

            return true;
        }

    private:
        Material material;
        Vector point, normal;
};