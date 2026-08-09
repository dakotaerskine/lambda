#pragma once

#include <cmath>

#include "core/platform.h"
#include "core/utils.h"
#include "math/intersection.h"
#include "math/matrix.h"
#include "math/random.h"
#include "math/ray.h"
#include "math/vector.h"

enum class ObjectType { SPHERE, PLANE };

class Object {
    public:
        HOST_DEVICE Object() : type(ObjectType::SPHERE) {}

        HOST_DEVICE static Object makeSphere(int m, const Vector & c, Float r) {
            Object object;

            object.type = ObjectType::SPHERE;
            object.material = m;
            object.sphere.center = c;
            object.sphere.radius = r;

            return object;
        }

        HOST_DEVICE static Object makePlane(int m, const Vector & p, const Vector & n) {
            Object object;

            object.type = ObjectType::PLANE;
            object.material = m;
            object.plane.point = p;
            object.plane.normal = n.normalize();

            return object;
        }

        HOST_DEVICE int getMaterial() const { return material; }

        HOST_DEVICE bool intersect(int i, const Ray & r, Intersection & intersection) const {
            switch (type) {
                case ObjectType::SPHERE: return intersectSphere(i, r, intersection);
                case ObjectType::PLANE: return intersectPlane(i, r, intersection);
            }

            return false;
        }

    private:
        ObjectType type;
        int material;

        union {
            struct { Vector center; Float radius; } sphere;
            struct { Vector point, normal; } plane;
        };

        HOST_DEVICE bool intersectSphere(int i, const Ray & r, Intersection & intersection) const {
            Vector oc = r.getOrigin() - sphere.center;

            Float a = 1;
            Float b = 2 * r.getDirection().dot(oc);
            Float c = oc.dot(oc) - sphere.radius * sphere.radius;
            Float d = b * b - 4 * a * c;

            if (d < 0) return false;

            d = sqrt(d);

            Float t1 = (-b - d) / (2 * a);
            Float t2 = (-b + d) / (2 * a);

            if (t1 < EPSILON && t2 < EPSILON) return false;

            Float t = (t1 < EPSILON) ? t2 : t1;

            if (t >= intersection.t) return false;

            intersection.i = i;
            intersection.t = t;
            intersection.point = r.at(t);
            intersection.normal = (intersection.point - sphere.center).normalize();
            intersection.u = (atan2(-intersection.normal[2], intersection.normal[0]) + PI) / (2 * PI);
            intersection.v = acos(-intersection.normal[1]) / PI;
            intersection.frontFacing = r.getDirection().dot(intersection.normal) < 0;
            if (!intersection.frontFacing) intersection.normal *= -1;

            return true;
        }

        HOST_DEVICE bool intersectPlane(int i, const Ray & r, Intersection & intersection) const {
            Float denominator = plane.normal.dot(r.getDirection());

            if (fabs(denominator) < EPSILON) return false;

            Float t = plane.normal.dot(plane.point - r.getOrigin()) / denominator;

            if (t < EPSILON) return false;

            if (t >= intersection.t) return false;

            intersection.i = i;
            intersection.t = t;
            intersection.point = r.at(t);
            intersection.normal = plane.normal;

            Vector uAxis = fabs(plane.normal[0]) > fabs(plane.normal[1]) ? Vector(0, 1, 0) : Vector(1, 0, 0);
            uAxis = uAxis.cross(plane.normal).normalize();
            Vector vAxis = plane.normal.cross(uAxis).normalize();

            intersection.u = uAxis.dot(intersection.point - plane.point);
            intersection.v = vAxis.dot(intersection.point - plane.point);

            intersection.frontFacing = r.getDirection().dot(intersection.normal) < 0;
            if (!intersection.frontFacing) intersection.normal *= -1;

            return true;
        }
};