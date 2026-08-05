#pragma once

#include "intersection.h"
#include "object.h"
#include "platform.h"
#include "ray.h"
#include "spectrum.h"

class Scene {
    public:
        HOST_DEVICE Scene(Spectrum b, Object * o, int n) : background(b), objects(o), numObjects(n) {}

        HOST_DEVICE const Spectrum & getBackground() const { return background; }

        HOST_DEVICE const Object & getObject(int i) const { return objects[i]; }

        HOST_DEVICE bool hit(const Ray & r, Intersection & intersection) const {
            bool hitObject = false;

            for (int i = 0; i < numObjects; i++)
                if (objects[i].intersect(i, r, intersection)) hitObject = true;

            return hitObject;
        }

    private:
        Spectrum background;
        Object * objects;
        int numObjects;
};