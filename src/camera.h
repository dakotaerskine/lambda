#ifndef CAMERA_H
#define CAMERA_H

#include "ray.h"
#include "vector.h"

class Camera {
    public:
        Camera() {}
        Camera(const Vector & p, const Vector & c, const Vector & h, const Vector & v) : position(p), corner(c), horizontal(h), vertical(v) {}

        Ray getRay(double u, double v, double lambda, int lambdaIndex) const {
            Vector direction = corner + horizontal * u + vertical * v - position;
            direction.normalize();
            return Ray(position, direction, lambda, lambdaIndex);
        }

    private:
        Vector position, corner, horizontal, vertical;
};

#endif