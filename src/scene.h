#ifndef SCENE_H
#define SCENE_H

#include <vector>

#include "intersection.h"
#include "object.h"
#include "ray.h"
#include "spectrum.h"

class Scene {
    public:
        Scene(Spectrum b = Spectrum()) : background(b) {}

        Spectrum getBackground() const { return background; }

        void addObject(Object * o) { objects.push_back(o); }

        bool hit(const Ray & r, Intersection & intersection) const {
            bool hitObject = false;

            for (Object * object : objects)
                if (object->intersect(r, intersection)) hitObject = true;
                
            return hitObject;
        }

    private:
        std::vector<Object *> objects;
        Spectrum background;
};

#endif