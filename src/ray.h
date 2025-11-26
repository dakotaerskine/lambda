#ifndef RAY_H
#define RAY_H

#include "vector.h"

class Ray {
    public:
        Ray() {}
        Ray(const Vector & o, const Vector & d, double l, int i) : origin(o), direction(d), lambda(l), lambdaIndex(i) {}

        const Vector & getOrigin() const { return origin; }
        const Vector & getDirection() const { return direction; }
        double getLambda() const { return lambda; }
        int getLambdaIndex() const { return lambdaIndex; }

        Vector at(double t) const { return origin + t * direction; }

    private:
        Vector origin, direction;
        double lambda;
        int lambdaIndex;
};

#endif