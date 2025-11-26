#ifndef OBJECT_H
#define OBJECT_H

#include "intersection.h"
#include "material.h"
#include "ray.h"
#include "utils.h"
#include "vector.h"

class Object {
    public:
        Object() {}
        Object(Material * m) : material(m) {}

        Material * getMaterial() const { return material; }

        virtual bool intersect(const Ray & r, Intersection & intersection) const = 0;

    private:
        Material * material;
};

class Sphere : public Object {
    public:
        Sphere() {}
        Sphere(Material * m, const Vector & c, double r) : Object(m), center(c), radius(r) {}

        bool intersect(const Ray & r, Intersection & intersection) const {
            Vector oc = r.getOrigin() - center;

            double a = 1;
            double b = 2 * r.getDirection().dot(oc);
            double c = oc.dot(oc) - radius * radius;
            double d = b * b - 4 * a * c;

            if (d < 0) return false;

            d = sqrt(d);

            double t1 = (-b - d) / (2 * a);
            double t2 = (-b + d) / (2 * a);

            if (t1 < EPSILON && t2 < EPSILON) return false;

            double t = (t1 < EPSILON) ? t2 : t1;

            if (t >= intersection.t) return false;

            intersection.t = t;
            intersection.point = r.at(t);
            intersection.normal = intersection.point - center;
            intersection.normal.normalize();
            intersection.frontFacing = r.getDirection().dot(intersection.normal) < 0;
            if (!intersection.frontFacing) intersection.normal *= -1;
            intersection.material = getMaterial();

            return true;
        }

    private:
        Vector center;
        double radius;
};

class Plane : public Object {
    public:
        Plane() {}
        Plane(Material * m, const Vector & p, const Vector & n) : Object(m), point(p), normal(n) {}
        
        bool intersect(const Ray & r, Intersection & intersection) const {
            double denominator = normal.dot(r.getDirection());

            if (fabs(denominator) < EPSILON) return false;

            double t = normal.dot(point - r.getOrigin()) / denominator;

            if (t < EPSILON) return false;

            intersection.t = t;
            intersection.point = r.at(t);
            intersection.normal = normal;
            intersection.frontFacing = r.getDirection().dot(intersection.normal) < 0;
            if (!intersection.frontFacing) intersection.normal *= -1;
            intersection.material = getMaterial();

            return true;
        }

    private:
        Vector point, normal;
};

#endif