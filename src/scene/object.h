#pragma once

#include <cmath>

#include "core/platform.h"
#include "core/utils.h"
#include "math/intersection.h"
#include "math/matrix.h"
#include "math/random.h"
#include "math/ray.h"
#include "math/vector.h"

enum class ObjectType { SPHERE, QUAD, TRI };

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

        HOST_DEVICE static Object makeTri(int m, const Vector & c, const Vector & h, const Vector & v, const Vector & n0 = Vector(0, 0, 0), const Vector & n1 = Vector(0, 0, 0), const Vector & n2 = Vector(0, 0, 0), Float u0 = 0, Float u1 = 0, Float u2 = 0, Float v0 = 0, Float v1 = 0, Float v2 = 0) {
            Object object;

            object.type = ObjectType::TRI;
            object.material = m;
            object.tri.corner = c;
            object.tri.horizontal = h;
            object.tri.vertical = v;
            object.tri.normal = cross(h, v);

            if (n0.length() < EPSILON) {
                object.tri.n0 = normalize(object.tri.normal);
                object.tri.n1 = object.tri.n0;
                object.tri.n2 = object.tri.n0;
            }
            else {
                object.tri.n0 = n0;
                object.tri.n1 = n1;
                object.tri.n2 = n2;
            }
            
            object.tri.u0 = u0;
            object.tri.u1 = u1;
            object.tri.u2 = u2;
            object.tri.v0 = v0;
            object.tri.v1 = v1;
            object.tri.v2 = v2;

            return object;
        }

        HOST_DEVICE int getMaterial() const { return material; }

        HOST_DEVICE Vector min() const {
            switch (type) {
                case ObjectType::SPHERE: return minSphere();
                case ObjectType::QUAD: return minQuad();
                case ObjectType::TRI: return minTri();
            }

            return Vector();
        }

        HOST_DEVICE Vector max() const {
            switch (type) {
                case ObjectType::SPHERE: return maxSphere();
                case ObjectType::QUAD: return maxQuad();
                case ObjectType::TRI: return maxTri();
            }

            return Vector();
        }

        HOST_DEVICE Vector center() const {
            switch (type) {
                case ObjectType::SPHERE: return centerSphere();
                case ObjectType::QUAD: return centerQuad();
                case ObjectType::TRI: return centerTri();
            }

            return Vector();
        }

        HOST_DEVICE Float radius() const {
            switch (type) {
                case ObjectType::SPHERE: return radiusSphere();
                case ObjectType::QUAD: return radiusQuad();
                case ObjectType::TRI: return radiusTri();
            }

            return 0;
        }

        HOST_DEVICE Float area() const {
            switch (type) {
                case ObjectType::SPHERE: return areaSphere();
                case ObjectType::QUAD: return areaQuad();
                case ObjectType::TRI: return areaTri();
            }

            return 0;
        }

        HOST_DEVICE Float pdf(const Vector & point, const Vector & direction) const {
            switch (type) {
                case ObjectType::SPHERE: return pdfSphere(point, direction);
                case ObjectType::QUAD: return pdfQuad(point, direction);
                case ObjectType::TRI: return pdfTri(point, direction);
            }

            return 0;
        }

        HOST_DEVICE Vector sample(const Vector & point, Random & state) const {
            switch (type) {
                case ObjectType::SPHERE: return sampleSphere(point, state);
                case ObjectType::QUAD: return sampleQuad(point, state);
                case ObjectType::TRI: return sampleTri(point, state);
            }

            return Vector();
        }

        HOST_DEVICE bool intersect(int i, const Ray & r, Intersection & intersection) const {
            switch (type) {
                case ObjectType::SPHERE: return intersectSphere(i, r, intersection);
                case ObjectType::QUAD: return intersectQuad(i, r, intersection);
                case ObjectType::TRI: return intersectTri(i, r, intersection);
            }

            return false;
        }

    private:
        ObjectType type;
        int material;

        union {
            struct { Vector center; Float radius; } sphere;
            struct { Vector corner, horizontal, vertical, normal; } quad;
            struct { Vector corner, horizontal, vertical, normal, n0, n1, n2; Float u0, u1, u2, v0, v1, v2; } tri;
        };

        HOST_DEVICE Vector minSphere() const { return sphere.center - Vector(sphere.radius, sphere.radius, sphere.radius); }
        HOST_DEVICE Vector minQuad() const { return minV(quad.corner, minV(quad.corner + quad.horizontal, minV(quad.corner + quad.vertical, quad.corner + quad.horizontal + quad.vertical))) - Vector(EPSILON, EPSILON, EPSILON); }
        HOST_DEVICE Vector minTri() const { return minV(tri.corner, minV(tri.corner + tri.horizontal, tri.corner + tri.vertical)) - Vector(EPSILON, EPSILON, EPSILON); }

        HOST_DEVICE Vector maxSphere() const { return sphere.center + Vector(sphere.radius, sphere.radius, sphere.radius); }
        HOST_DEVICE Vector maxQuad() const { return maxV(quad.corner, maxV(quad.corner + quad.horizontal, maxV(quad.corner + quad.vertical, quad.corner + quad.horizontal + quad.vertical))) + Vector(EPSILON, EPSILON, EPSILON); }
        HOST_DEVICE Vector maxTri() const { return maxV(tri.corner, maxV(tri.corner + tri.horizontal, tri.corner + tri.vertical)) + Vector(EPSILON, EPSILON, EPSILON); }

        HOST_DEVICE Vector centerSphere() const { return sphere.center; }
        HOST_DEVICE Vector centerQuad() const { return quad.corner + 0.5 * quad.horizontal + 0.5 * quad.vertical; }
        HOST_DEVICE Vector centerTri() const { return tri.corner + (tri.horizontal + tri.vertical) / 3; }

        HOST_DEVICE Float radiusSphere() const { return sphere.radius; }

        HOST_DEVICE Float radiusQuad() const {
            Float diagonal1 = (quad.horizontal + quad.vertical).lengthSquared();
            Float diagonal2 = (quad.horizontal - quad.vertical).lengthSquared();

            return Float(0.5) * sqrtF(fmaxF(diagonal1, diagonal2));
        }

        HOST_DEVICE Float radiusTri() const {
            Vector center = centerTri();

            Float distance1 = (tri.corner - center).lengthSquared();
            Float distance2 = (tri.corner + tri.horizontal - center).lengthSquared();
            Float distance3 = (tri.corner + tri.vertical - center).lengthSquared();

            return sqrtF(fmaxF(distance1, fmaxF(distance2, distance3)));
        }

        HOST_DEVICE Float areaSphere() const { return 4 * PI * sphere.radius * sphere.radius; }
        HOST_DEVICE Float areaQuad() const { return quad.normal.length(); }
        HOST_DEVICE Float areaTri() const { return tri.normal.length() / 2; }

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

            if (fabsF(cosTheta) < EPSILON) return 0;

            Float t = dot(quad.corner - point, normalize(quad.normal)) / cosTheta;

            if (t < EPSILON) return 0;

            Vector local = (point + direction * t) - quad.corner;

            Float u = dot(quad.normal, cross(local, quad.vertical)) / quad.normal.lengthSquared();
            Float v = dot(quad.normal, cross(quad.horizontal, local)) / quad.normal.lengthSquared();

            if (u < 0 || u > 1 || v < 0 || v > 1) return 0;

            return t * t / (fabsF(cosTheta) * quad.normal.length());
        }

        HOST_DEVICE Float pdfTri(const Vector & point, const Vector & direction) const {
            Vector rayCrossVertical = cross(direction, tri.vertical);

            Float determinant = dot(tri.horizontal, rayCrossVertical);

            if (fabsF(determinant) < EPSILON_SQUARED) return 0;

            Float inverseDeterminant = 1 / determinant;

            Vector oc = point - tri.corner;

            Float alpha = inverseDeterminant * dot(oc, rayCrossVertical);

            if (alpha < 0 || alpha > 1) return 0;

            Vector ocCrossHorizontal = cross(oc, tri.horizontal);

            Float beta = inverseDeterminant * dot(direction, ocCrossHorizontal);

            if (beta < 0 || alpha + beta > 1) return 0;

            Float t = inverseDeterminant * dot(tri.vertical, ocCrossHorizontal);

            if (t < EPSILON) return 0;

            Float area = tri.normal.length() * 0.5f;
            Float cosTheta = dot(normalize(tri.normal), direction);

            return t * t / (fabsF(cosTheta) * area);
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

        HOST_DEVICE Vector sampleTri(const Vector & point, Random & state) const {
            Float u = randomFloat(state);
            Float v = randomFloat(state);

            if (u + v > 1) {
                u = 1 - u;
                v = 1 - v;
            }

            Vector sample = tri.corner + tri.horizontal * u + tri.vertical * v;

            return normalize(sample - point);
        }

        HOST_DEVICE bool intersectSphere(int i, const Ray & r, Intersection & intersection) const {
            Vector oc = r.getOrigin() - sphere.center;

            Float a = r.getDirection().lengthSquared();
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

            intersection.object = i;
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
            Vector n = normalize(quad.normal);

            Float denominator = dot(n, r.getDirection());

            if (fabsF(denominator) < EPSILON) return false;

            Float t = dot(n, quad.corner - r.getOrigin()) / denominator;

            if (t < EPSILON || t >= intersection.t) return false;

            Vector point = r.at(t);
            Vector projection = point - quad.corner;

            Float alpha = dot(quad.normal, cross(projection, quad.vertical)) / quad.normal.lengthSquared();
            Float beta = dot(quad.normal, cross(quad.horizontal, projection)) / quad.normal.lengthSquared();

            if (alpha < 0 || alpha > 1 || beta < 0 || beta > 1) return false;

            intersection.object = i;
            intersection.t = t;
            intersection.point = point;
            intersection.normal = normalize(quad.normal);
            intersection.u = alpha;
            intersection.v = beta;
            intersection.frontFacing = dot(r.getDirection(), intersection.normal) < 0;
            if (!intersection.frontFacing) intersection.normal *= -1;

            return true;
        }

        HOST_DEVICE bool intersectTri(int i, const Ray & r, Intersection & intersection) const {
            Vector rayCrossVertical = cross(r.getDirection(), tri.vertical);

            Float determinant = dot(tri.horizontal, rayCrossVertical);

            if (fabsF(determinant) < EPSILON_SQUARED) return false;

            Float inverseDeterminant = 1 / determinant;

            Vector oc = r.getOrigin() - tri.corner;

            Float alpha = inverseDeterminant * dot(oc, rayCrossVertical);

            if (alpha < 0 || alpha > 1) return false;

            Vector ocCrossHorizontal = cross(oc, tri.horizontal);

            Float beta = inverseDeterminant * dot(r.getDirection(), ocCrossHorizontal);

            if (beta < 0 || alpha + beta > 1) return false;

            Float t = inverseDeterminant * dot(tri.vertical, ocCrossHorizontal);

            if (t < EPSILON || t >= intersection.t) return false;

            intersection.object = i;
            intersection.t = t;
            intersection.point = r.at(t);
            intersection.normal = normalize((1 - alpha - beta) * tri.n0 + alpha * tri.n1 + beta * tri.n2);
            intersection.u = (1 - alpha - beta) * tri.u0 + alpha * tri.u1 + beta * tri.u2;
            intersection.v = (1 - alpha - beta) * tri.v0 + alpha * tri.v1 + beta * tri.v2;
            intersection.frontFacing = dot(r.getDirection(), tri.normal) < 0;
            if (!intersection.frontFacing) intersection.normal *= -1;

            return true;
        }
};