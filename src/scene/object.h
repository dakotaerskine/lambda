#pragma once

#include <cmath>

#include "core/platform.h"
#include "core/utils.h"
#include "math/intersection.h"
#include "math/matrix.h"
#include "math/random.h"
#include "math/ray.h"
#include "math/vector.h"

enum class ObjectType { SPHERE, QUAD };

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

        HOST_DEVICE static Object makeQuad(int m, const Vector & c, const Vector & h, const Vector & v) {
            Object object;

            object.type = ObjectType::QUAD;
            object.material = m;
            object.quad.corner = c;
            object.quad.horizontal = h;
            object.quad.vertical = v;
            object.quad.normal = cross(h, v);

            return object;
        }

        HOST_DEVICE int getMaterial() const { return material; }

        HOST_DEVICE Vector min() const {
            switch (type) {
                case ObjectType::SPHERE: return minSphere();
                case ObjectType::QUAD: return minQuad();
            }

            return Vector();
        }

        HOST_DEVICE Vector max() const {
            switch (type) {
                case ObjectType::SPHERE: return maxSphere();
                case ObjectType::QUAD: return maxQuad();
            }

            return Vector();
        }

        HOST_DEVICE Vector center() const {
            switch (type) {
                case ObjectType::SPHERE: return centerSphere();
                case ObjectType::QUAD: return centerQuad();
            }

            return Vector();
        }

        HOST_DEVICE Float radius() const {
            switch (type) {
                case ObjectType::SPHERE: return radiusSphere();
                case ObjectType::QUAD: return radiusQuad();
            }

            return 0;
        }

        HOST_DEVICE Float area() const {
            switch (type) {
                case ObjectType::SPHERE: return areaSphere();
                case ObjectType::QUAD: return areaQuad();
            }

            return 0;
        }

        HOST_DEVICE Float pdf(const Vector & point, const Vector & direction) const {
            switch (type) {
                case ObjectType::SPHERE: return pdfSphere(point, direction);
                case ObjectType::QUAD: return pdfQuad(point, direction);
            }

            return 0;
        }

        HOST_DEVICE Vector sample(const Vector & point, Random & state) const {
            switch (type) {
                case ObjectType::SPHERE: return sampleSphere(point, state);
                case ObjectType::QUAD: return sampleQuad(point, state);
            }

            return Vector();
        }

        HOST_DEVICE bool intersect(int i, const Ray & r, Intersection & intersection) const {
            switch (type) {
                case ObjectType::SPHERE: return intersectSphere(i, r, intersection);
                case ObjectType::QUAD: return intersectQuad(i, r, intersection);
            }

            return false;
        }

    private:
        ObjectType type;
        int material;

        union {
            struct { Vector center; Float radius; } sphere;
            struct { Vector corner, horizontal, vertical, normal; } quad;
        };

        HOST_DEVICE Vector minSphere() const { return sphere.center - Vector(sphere.radius, sphere.radius, sphere.radius); }
        HOST_DEVICE Vector minQuad() const { return minV(quad.corner, quad.corner + quad.horizontal + quad.vertical) - Vector(EPSILON, EPSILON, EPSILON); }

        HOST_DEVICE Vector maxSphere() const { return sphere.center + Vector(sphere.radius, sphere.radius, sphere.radius); }
        HOST_DEVICE Vector maxQuad() const { return maxV(quad.corner, quad.corner + quad.horizontal + quad.vertical) + Vector(EPSILON, EPSILON, EPSILON); }

        HOST_DEVICE Vector centerSphere() const { return sphere.center; }
        HOST_DEVICE Vector centerQuad() const { return quad.corner + 0.5 * quad.horizontal + 0.5 * quad.vertical; }

        HOST_DEVICE Float radiusSphere() const { return sphere.radius; }
        HOST_DEVICE Float radiusQuad() const { return Float(0.5) * (quad.horizontal + quad.vertical).length(); }

        HOST_DEVICE Float areaSphere() const { return 4 * PI * sphere.radius * sphere.radius; }
        HOST_DEVICE Float areaQuad() const { return quad.normal.length(); }

        HOST_DEVICE Float pdfSphere(const Vector & point, const Vector & direction) const {
            Vector oc = sphere.center - point;
            Float distanceSquared = oc.lengthSquared();
            Float radiusSquared = sphere.radius * sphere.radius;

            if (distanceSquared <= radiusSquared) return 1 / (4 * PI);

            Float cosThetaMax = sqrtF(1 - radiusSquared / distanceSquared);
            Float cosTheta = dot(normalize(oc), direction);

            if (cosTheta < cosThetaMax) return 0;

            return 1 / (2 * PI * (1 - cosThetaMax));
        }

        HOST_DEVICE Float pdfQuad(const Vector & point, const Vector & direction) const {
            Float cosTheta = dot(normalize(quad.normal), direction);

            if (fabs(cosTheta) < EPSILON) return 0;

            Float t = dot(quad.corner - point, normalize(quad.normal)) / cosTheta;

            if (t < EPSILON) return 0;

            Vector local = (point + direction * t) - quad.corner;

            Float u = dot(local, quad.horizontal) / quad.horizontal.lengthSquared();
            Float v = dot(local, quad.vertical) / quad.vertical.lengthSquared();

            if (u < 0 || u > 1 || v < 0 || v > 1) return 0;

            return t * t / (fabsF(cosTheta) * quad.normal.length());
        }

        HOST_DEVICE Vector sampleSphere(const Vector & point, Random & state) const {
            Vector oc = sphere.center - point;
            Float distanceSquared = oc.lengthSquared();
            Float radiusSquared = sphere.radius * sphere.radius;

            if (distanceSquared <= radiusSquared) return randomUnitVector(state);

            Float cosThetaMax = sqrtF(1 - radiusSquared / distanceSquared);

            return randomInCone(oc, cosThetaMax, state);
        }

        HOST_DEVICE Vector sampleQuad(const Vector & point, Random & state) const {
            Float u = randomFloat(state);
            Float v = randomFloat(state);

            Vector sample = quad.corner + quad.horizontal * u + quad.vertical * v;

            return normalize(sample - point);
        }

        HOST_DEVICE bool intersectSphere(int i, const Ray & r, Intersection & intersection) const {
            Vector oc = r.getOrigin() - sphere.center;

            Float a = 1;
            Float b = 2 * dot(r.getDirection(), oc);
            Float c = dot(oc, oc) - sphere.radius * sphere.radius;
            Float d = b * b - 4 * a * c;

            if (d < 0) return false;

            d = sqrtF(d);

            Float t1 = (-b - d) / (2 * a);
            Float t2 = (-b + d) / (2 * a);

            if (t1 < EPSILON && t2 < EPSILON) return false;

            Float t = (t1 < EPSILON) ? t2 : t1;

            if (t >= intersection.t) return false;

            intersection.i = i;
            intersection.t = t;
            intersection.point = r.at(t);
            intersection.normal = normalize(intersection.point - sphere.center);
            intersection.u = (atan2F(-intersection.normal[2], intersection.normal[0]) + PI) / (2 * PI);
            intersection.v = acosF(-intersection.normal[1]) / PI;
            intersection.frontFacing = dot(r.getDirection(), intersection.normal) < 0;
            if (!intersection.frontFacing) intersection.normal *= -1;

            return true;
        }

        HOST_DEVICE bool intersectQuad(int i, const Ray & r, Intersection & intersection) const {
            Float denominator = dot(quad.normal, r.getDirection());

            if (fabs(denominator) < EPSILON) return false;

            Float t = dot(quad.normal, quad.corner - r.getOrigin()) / denominator;

            if (t < EPSILON) return false;

            if (t >= intersection.t) return false;

            Vector point = r.at(t);
            Vector projection = point - quad.corner;

            Float alpha = dot(quad.normal, cross(projection, quad.vertical)) / quad.normal.lengthSquared();
            Float beta = dot(quad.normal, cross(quad.horizontal, projection)) / quad.normal.lengthSquared();

            if (alpha < 0 || alpha > 1 || beta < 0 || beta > 1) return false;

            intersection.i = i;
            intersection.t = t;
            intersection.point = point;
            intersection.normal = normalize(quad.normal);
            intersection.u = alpha;
            intersection.v = beta;
            intersection.frontFacing = dot(r.getDirection(), intersection.normal) < 0;
            if (!intersection.frontFacing) intersection.normal *= -1;

            return true;
        }
};