#pragma once

#include "intersection.cuh"
#include "object.cuh"
#include "ray.cuh"
#include "spectrum.cuh"

class Scene {
    public:
        __host__ __device__ Scene(Spectrum b = Spectrum()) : background(b) {}

        __host__ __device__ Spectrum getBackground() const { return background; }

        __host__ __device__ void addObject(Object o) { object = o; }

        __host__ __device__ const Object & getObject() const { return object; }

        __host__ __device__ bool hit(const Ray & r, Intersection & intersection) const { return object.intersect(r, intersection); }

    private:
        Object object;
        Spectrum background;
};