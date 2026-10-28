#pragma once

#include <cmath>

#include "core/platform.h"
#include "core/utils.h"
#include "math/matrix.h"
#include "math/random.h"
#include "math/ray.h"
#include "math/vector.h"
#include "scene/intersection.h"
#include "scene/mesh.h"

namespace lambda {
    class Material;
    class Medium;

    class Object {
        public:
            Object() : type(Type::SPHERE), material(nullptr), medium0(nullptr), medium1(nullptr) {}

            static Object makeSphere(Material * m, Medium * m0, Medium * m1, Float r) {
                Object object;

                object.type = Type::SPHERE;
                object.material = m;
                object.medium0 = m0;
                object.medium1 = m1;
                object.area = 4 * constants::PI * r * r;
                object.sphere.radius = r;

                return object;
            }

            static Object makeDisk(Material * m, Medium * m0, Medium * m1, Float r) {
                Object object;

                object.type = Type::DISK;
                object.material = m;
                object.medium0 = m0;
                object.medium1 = m1;
                object.area = constants::PI * r * r;
                object.disk.radius = r;

                return object;
            }

            static Object makeCylinder(Material * m, Medium * m0, Medium * m1, Float r, Float h) {
                Object object;

                object.type = Type::CYLINDER;
                object.material = m;
                object.medium0 = m0;
                object.medium1 = m1;
                object.area = 2 * constants::PI * r * h;
                object.cylinder.radius = r;
                object.cylinder.height = h;

                return object;
            }

            static Object makeTri(Material * m, Medium * m0, Medium * m1, Mesh * mesh, int i) {
                Object object;

                object.type = Type::TRI;
                object.material = m;
                object.medium0 = m0;
                object.medium1 = m1;

                Vector<Float, 3> corner = mesh->getVertex(i, 0);
                Vector<Float, 3> horizontal = mesh->getVertex(i, 1) - corner;
                Vector<Float, 3> vertical = mesh->getVertex(i, 2) - corner;

                object.area = Float(0.5) * cross(horizontal, vertical).length();

                object.tri.mesh = mesh;
                object.tri.index = i;

                return object;
            }

            static Object makePatch(Material * m, Medium * m0, Medium * m1, Mesh * mesh, int i) {
                Object object;

                object.type = Type::PATCH;
                object.material = m;
                object.medium0 = m0;
                object.medium1 = m1;

                Vector<Float, 3> corner0 = mesh->getVertex(i, 0);
                Vector<Float, 3> corner1 = mesh->getVertex(i, 1);
                Vector<Float, 3> corner2 = mesh->getVertex(i, 2);
                Vector<Float, 3> corner3 = mesh->getVertex(i, 3);

                Vector<Float, 3> edge0 = corner1 - corner0;
                Vector<Float, 3> edge1 = corner2 - corner1;
                Vector<Float, 3> edge2 = corner3 - corner2;
                Vector<Float, 3> edge3 = corner0 - corner3;

                if ((corner3 - corner0 - edge1).length() <= constants::EPSILON_RELATIVE * std::fmax(edge1.length(), edge3.length())) object.area = cross(edge0, edge3).length();
                else if (std::fabs(dot(cross(edge0, edge3), corner2 - corner0)) <= constants::EPSILON_RELATIVE * edge0.length() * edge1.length() * edge2.length()) object.area = Float(0.5) * cross(corner2 - corner0, corner3 - corner1).length();
                else {
                    Vector<Float, 3> points[constants::AREA_SUBDIVISIONS + 1][constants::AREA_SUBDIVISIONS + 1];

                    for (int j = 0; j <= constants::AREA_SUBDIVISIONS; j++) {
                        Float u = Float(j) / constants::AREA_SUBDIVISIONS;

                        for (int k = 0; k <= constants::AREA_SUBDIVISIONS; k++) {
                            Float v = Float(k) / constants::AREA_SUBDIVISIONS;

                            points[j][k] = utils::interpolate(utils::interpolate(corner0, corner1, v), utils::interpolate(corner3, corner2, v), u);
                        }
                    }

                    object.area = 0;

                    for (int j = 0; j < constants::AREA_SUBDIVISIONS; j++)
                        for (int k = 0; k < constants::AREA_SUBDIVISIONS; k++)
                            object.area += Float(0.5) * cross(points[j + 1][k + 1] - points[j][k], points[j + 1][k] - points[j][k + 1]).length();
                }

                object.patch.mesh = mesh;
                object.patch.index = i;

                return object;
            }

            LAMBDA_HOST_DEVICE const Material * getMaterial() const { return material; }

            LAMBDA_HOST_DEVICE const Medium * getMedium0() const { return medium0; }

            LAMBDA_HOST_DEVICE const Medium * getMedium1() const { return medium1; }

            LAMBDA_HOST_DEVICE Float getArea() const { return area; }

            LAMBDA_HOST_DEVICE Vector<Float, 3> getMin() const {
                switch (type) {
                    case Type::SPHERE: return Vector<Float, 3>(-sphere.radius, -sphere.radius, -sphere.radius);
                    case Type::DISK: return Vector<Float, 3>(-disk.radius, -constants::EPSILON_RELATIVE * disk.radius, -disk.radius);
                    case Type::CYLINDER: return Vector<Float, 3>(-cylinder.radius, Float(-0.5) * cylinder.height, -cylinder.radius);
                    case Type::TRI: return getMinTri();
                    case Type::PATCH: return getMinPatch();
                }

                return Vector<Float, 3>();
            }

            LAMBDA_HOST_DEVICE Vector<Float, 3> getMax() const {
                switch (type) {
                    case Type::SPHERE: return Vector<Float, 3>(sphere.radius, sphere.radius, sphere.radius);
                    case Type::DISK: return Vector<Float, 3>(disk.radius, constants::EPSILON_RELATIVE * disk.radius, disk.radius);
                    case Type::CYLINDER: return Vector<Float, 3>(cylinder.radius, Float(0.5) * cylinder.height, cylinder.radius);
                    case Type::TRI: return maxBoundsTri();
                    case Type::PATCH: return maxBoundsPatch();
                }

                return Vector<Float, 3>();
            }

            LAMBDA_HOST_DEVICE Vector<Float, 3> center() const {
                switch (type) {
                    case Type::SPHERE: return Vector<Float, 3>(0, 0, 0);
                    case Type::DISK: return Vector<Float, 3>(0, 0, 0);
                    case Type::CYLINDER: return Vector<Float, 3>(0, 0, 0);
                    case Type::TRI: return (tri.mesh->getVertex(tri.index, 0) + tri.mesh->getVertex(tri.index, 1) + tri.mesh->getVertex(tri.index, 2)) / 3;
                    case Type::PATCH: return (patch.mesh->getVertex(patch.index, 0) + patch.mesh->getVertex(patch.index, 1) + patch.mesh->getVertex(patch.index, 2) + patch.mesh->getVertex(patch.index, 3)) / 4;
                }

                return Vector<Float, 3>();
            }

            LAMBDA_HOST_DEVICE Float radius() const {
                switch (type) {
                    case Type::SPHERE: return sphere.radius;
                    case Type::DISK: return disk.radius;
                    case Type::CYLINDER: return std::sqrt(cylinder.radius * cylinder.radius + Float(0.25) * cylinder.height * cylinder.height);
                    case Type::TRI: return radiusTri();
                    case Type::PATCH: return radiusPatch();
                }

                return 0;
            }

            LAMBDA_HOST_DEVICE Float probabilityDensity(const Vector<Float, 3> & point, const Vector<Float, 3> & direction) const {
                switch (type) {
                    case Type::SPHERE: return probabilityDensitySphere(point, direction);
                    case Type::DISK: return probabilityDensityDisk(point, direction);
                    case Type::CYLINDER: return probabilityDensityCylinder(point, direction);
                    case Type::TRI: return probabilityDensityTri(point, direction);
                    case Type::PATCH: return probabilityDensityPatch(point, direction);
                }

                return 0;
            }

            LAMBDA_HOST_DEVICE Vector<Float, 3> sample(const Vector<Float, 3> & point, Float & probabilityDensity, Random & state) const {
                switch (type) {
                    case Type::SPHERE: return sampleSphere(point, probabilityDensity, state);
                    case Type::DISK: return sampleDisk(point, probabilityDensity, state);
                    case Type::CYLINDER: return sampleCylinder(point, probabilityDensity, state);
                    case Type::TRI: return sampleTri(point, probabilityDensity, state);
                    case Type::PATCH: return samplePatch(point, probabilityDensity, state);
                }

                return Vector<Float, 3>();
            }

            LAMBDA_HOST_DEVICE bool intersect(Intersection & intersection, const Ray & r) const {
                switch (type) {
                    case Type::SPHERE: return intersectSphere(intersection, r);
                    case Type::DISK: return intersectDisk(intersection, r);
                    case Type::CYLINDER: return intersectCylinder(intersection, r);
                    case Type::TRI: return intersectTri(intersection, r);
                    case Type::PATCH: return intersectPatch(intersection, r);
                }

                return false;
            }

        private:
            enum class Type { SPHERE, DISK, CYLINDER, TRI, PATCH };

            Type type;
            Material * material;
            Medium * medium0, * medium1;
            Float area;

            union {
                struct { Float radius; } sphere;
                struct { Float radius; } disk;
                struct { Float radius, height; } cylinder;
                struct { Mesh * mesh; int index; } tri;
                struct { Mesh * mesh; int index; } patch;
            };

            LAMBDA_HOST_DEVICE Vector<Float, 3> getMinTri() const {
                Vector<Float, 3> corner0 = tri.mesh->getVertex(tri.index, 0);
                Vector<Float, 3> corner1 = tri.mesh->getVertex(tri.index, 1);
                Vector<Float, 3> corner2 = tri.mesh->getVertex(tri.index, 2);

                Vector<Float, 3> minCorner = min(corner0, min(corner1, corner2));

                Float padding = constants::EPSILON_RELATIVE * std::fmax(std::fabs(minCorner[0]), std::fmax(std::fabs(minCorner[1]), std::fabs(minCorner[2])));

                return minCorner - Vector<Float, 3>(padding, padding, padding);
            }

            LAMBDA_HOST_DEVICE Vector<Float, 3> getMinPatch() const {
                Vector<Float, 3> corner0 = patch.mesh->getVertex(patch.index, 0);
                Vector<Float, 3> corner1 = patch.mesh->getVertex(patch.index, 1);
                Vector<Float, 3> corner2 = patch.mesh->getVertex(patch.index, 2);
                Vector<Float, 3> corner3 = patch.mesh->getVertex(patch.index, 3);

                Vector<Float, 3> minCorner = min(min(corner0, corner1), min(corner2, corner3));

                Float padding = constants::EPSILON_RELATIVE * std::fmax(std::fabs(minCorner[0]), std::fmax(std::fabs(minCorner[1]), std::fabs(minCorner[2])));

                return minCorner - Vector<Float, 3>(padding, padding, padding);
            }

            LAMBDA_HOST_DEVICE Vector<Float, 3> maxBoundsTri() const {
                Vector<Float, 3> corner0 = tri.mesh->getVertex(tri.index, 0);
                Vector<Float, 3> corner1 = tri.mesh->getVertex(tri.index, 1);
                Vector<Float, 3> corner2 = tri.mesh->getVertex(tri.index, 2);

                Vector<Float, 3> maxCorner = max(corner0, max(corner1, corner2));

                Float padding = constants::EPSILON_RELATIVE * std::fmax(std::fabs(maxCorner[0]), std::fmax(std::fabs(maxCorner[1]), std::fabs(maxCorner[2])));

                return maxCorner + Vector<Float, 3>(padding, padding, padding);
            }

            LAMBDA_HOST_DEVICE Vector<Float, 3> maxBoundsPatch() const {
                Vector<Float, 3> corner0 = patch.mesh->getVertex(patch.index, 0);
                Vector<Float, 3> corner1 = patch.mesh->getVertex(patch.index, 1);
                Vector<Float, 3> corner2 = patch.mesh->getVertex(patch.index, 2);
                Vector<Float, 3> corner3 = patch.mesh->getVertex(patch.index, 3);

                Vector<Float, 3> maxCorner = max(max(corner0, corner1), max(corner2, corner3));

                Float padding = constants::EPSILON_RELATIVE * std::fmax(std::fabs(maxCorner[0]), std::fmax(std::fabs(maxCorner[1]), std::fabs(maxCorner[2])));

                return maxCorner + Vector<Float, 3>(padding, padding, padding);
            }

            LAMBDA_HOST_DEVICE Float radiusTri() const {
                Vector<Float, 3> corner0 = tri.mesh->getVertex(tri.index, 0);
                Vector<Float, 3> corner1 = tri.mesh->getVertex(tri.index, 1);
                Vector<Float, 3> corner2 = tri.mesh->getVertex(tri.index, 2);

                Vector<Float, 3> center = (corner0 + corner1 + corner2) / 3;

                Float distance1 = (corner0 - center).lengthSquared();
                Float distance2 = (corner1 - center).lengthSquared();
                Float distance3 = (corner2 - center).lengthSquared();

                return std::sqrt(std::fmax(distance1, std::fmax(distance2, distance3)));
            }

            LAMBDA_HOST_DEVICE Float radiusPatch() const {
                Vector<Float, 3> corner0 = patch.mesh->getVertex(patch.index, 0);
                Vector<Float, 3> corner1 = patch.mesh->getVertex(patch.index, 1);
                Vector<Float, 3> corner2 = patch.mesh->getVertex(patch.index, 2);
                Vector<Float, 3> corner3 = patch.mesh->getVertex(patch.index, 3);

                Vector<Float, 3> center = (corner0 + corner1 + corner2 + corner3) / 4;

                Float distance1 = (corner0 - center).lengthSquared();
                Float distance2 = (corner1 - center).lengthSquared();
                Float distance3 = (corner2 - center).lengthSquared();
                Float distance4 = (corner3 - center).lengthSquared();

                return std::sqrt(std::fmax(distance1, std::fmax(distance2, std::fmax(distance3, distance4))));
            }

            LAMBDA_HOST_DEVICE Float probabilityDensitySphere(const Vector<Float, 3> & point, const Vector<Float, 3> & direction) const {
                Float distanceSquared = point.lengthSquared();
                Float radiusSquared = sphere.radius * sphere.radius;

                if (distanceSquared <= radiusSquared) return 1 / (4 * constants::PI);

                Float cosThetaMax = std::sqrt(1 - radiusSquared / distanceSquared);
                Float cosTheta = dot(normalize(-point), direction);

                if (cosTheta < cosThetaMax) return 0;

                return 1 / (2 * constants::PI * (1 - cosThetaMax));
            }

            LAMBDA_HOST_DEVICE Float probabilityDensityDisk(const Vector<Float, 3> & point, const Vector<Float, 3> & direction) const {
                Float t, radiusSquared;
                Vector<Float, 3> hitPoint;

                if (!rayDisk(point, direction, disk.radius, constants::MAX, t, radiusSquared, hitPoint)) return false;

                Float cosTheta = std::fabs(direction[1]);

                return 1 / (constants::PI * disk.radius * disk.radius) * t * t / cosTheta;
            }

            LAMBDA_HOST_DEVICE Float probabilityDensityCylinder(const Vector<Float, 3> & point, const Vector<Float, 3> & direction) const {
                Float t, rho;
                Vector<Float, 3> hitPoint;

                if (!rayCylinder(point, direction, cylinder.radius, cylinder.height, constants::MAX, t, rho, hitPoint)) return 0;

                Float alpha = rho <= cylinder.radius ? constants::PI : std::acos(utils::clamp(cylinder.radius / rho, Float(-1), Float(1)));

                Float cosTheta = std::fabs(dot(normalize(Vector<Float, 3>(hitPoint[0], 0, hitPoint[2])), direction));

                if (cosTheta <= constants::GRAZING_COS) return 0;

                return 1 / (2 * alpha * cylinder.radius * cylinder.height) * t * t / cosTheta;
            }

            LAMBDA_HOST_DEVICE Float probabilityDensityTri(const Vector<Float, 3> & point, const Vector<Float, 3> & direction) const {
                Vector<Float, 3> corner0 = tri.mesh->getVertex(tri.index, 0);
                Vector<Float, 3> corner1 = tri.mesh->getVertex(tri.index, 1);
                Vector<Float, 3> corner2 = tri.mesh->getVertex(tri.index, 2);

                Float t, alpha, beta;

                if (!rayTri(point, direction, corner0, corner1, corner2, constants::MAX, t, alpha, beta)) return 0;

                Float cosTheta = dot(normalize(cross(corner1 - corner0, corner2 - corner0)), direction);

                return t * t / (std::fabs(cosTheta) * area);
            }

            LAMBDA_HOST_DEVICE Float probabilityDensityPatch(const Vector<Float, 3> & point, const Vector<Float, 3> & direction) const {
                Vector<Float, 3> corner0 = patch.mesh->getVertex(patch.index, 0);
                Vector<Float, 3> corner1 = patch.mesh->getVertex(patch.index, 1);
                Vector<Float, 3> corner2 = patch.mesh->getVertex(patch.index, 2);
                Vector<Float, 3> corner3 = patch.mesh->getVertex(patch.index, 3);

                Float w0 = cross(corner3 - corner0, corner1 - corner0).length();
                Float w1 = cross(corner3 - corner0, corner2 - corner3).length();
                Float w2 = cross(corner2 - corner1, corner1 - corner0).length();
                Float w3 = cross(corner2 - corner1, corner2 - corner3).length();

                Float totalWeight = w0 + w1 + w2 + w3;

                Float t1, u1, v1, t2, u2, v2;

                if (!rayPatch(point, direction, corner0, corner1, corner2, corner3, constants::MAX, t1, u1, v1, t2, u2, v2)) return 0;

                Float sum = 0;

                Vector<Float, 3> normal1 = cross(utils::interpolate(corner3 - corner0, corner2 - corner1, v1), utils::interpolate(corner1 - corner0, corner2 - corner3, u1));

                Float jacobian1 = normal1.length();

                if (t1 > 0 && t1 < constants::MAX && jacobian1 > 0) {
                    Float cosTheta = std::fabs(dot(normal1 / jacobian1, direction));

                    if (cosTheta > constants::GRAZING_COS) sum += 4 * ((1 - u1) * (1 - v1) * w0 + u1 * (1 - v1) * w1 + (1 - u1) * v1 * w2 + u1 * v1 * w3) / totalWeight / jacobian1 * t1 * t1 / cosTheta;
                }

                Vector<Float, 3> normal2 = cross(utils::interpolate(corner3 - corner0, corner2 - corner1, v2), utils::interpolate(corner1 - corner0, corner2 - corner3, u2));

                Float jacobian2 = normal2.length();

                if (t2 > 0 && t2 < constants::MAX && jacobian2 > 0) {
                    Float cosTheta = std::fabs(dot(normal2 / jacobian2, direction));

                    if (cosTheta > constants::GRAZING_COS) sum += 4 * ((1 - u2) * (1 - v2) * w0 + u2 * (1 - v2) * w1 + (1 - u2) * v2 * w2 + u2 * v2 * w3) / totalWeight / jacobian2 * t2 * t2 / cosTheta;
                }

                return sum;
            }

            LAMBDA_HOST_DEVICE Vector<Float, 3> sampleSphere(const Vector<Float, 3> & point, Float & probabilityDensity, Random & state) const {
                Float distanceSquared = point.lengthSquared();
                Float radiusSquared = sphere.radius * sphere.radius;

                if (distanceSquared <= radiusSquared) {
                    probabilityDensity = 1 / (4 * constants::PI);

                    return utils::randomUnitVector(state);
                }

                Float cosThetaMax = std::sqrt(1 - radiusSquared / distanceSquared);

                Vector<Float, 3> direction = utils::randomInCone(normalize(-point), cosThetaMax, state);

                if (dot(normalize(-point), direction) < cosThetaMax) probabilityDensity = 0;
                else probabilityDensity = 1 / (2 * constants::PI * (1 - cosThetaMax));

                return direction;
            }

            LAMBDA_HOST_DEVICE Vector<Float, 3> sampleDisk(const Vector<Float, 3> & point, Float & probabilityDensity, Random & state) const {
                Vector<Float, 3> sample = utils::randomInHemisphere(Vector<Float, 3>(0, 1, 0), state) * disk.radius;

                sample[1] = 0;

                Vector<Float, 3> direction = normalize(sample - point);

                probabilityDensity = 0;

                if (std::fabs(direction[1]) > constants::EPSILON_RELATIVE_SQUARED * direction.lengthSquared()) {
                    Float distanceSquared = -point[1] / direction[1];

                    if (distanceSquared > 0 && sample[0] * sample[0] + sample[2] * sample[2] <= disk.radius * disk.radius) {
                        distanceSquared *= distanceSquared;

                        Float cosTheta = std::fabs(direction[1]);

                        probabilityDensity = 1 / (constants::PI * disk.radius * disk.radius) * distanceSquared / cosTheta;
                    }
                }

                return direction;
            }

            LAMBDA_HOST_DEVICE Vector<Float, 3> sampleCylinder(const Vector<Float, 3> & point, Float & probabilityDensity, Random & state) const {
                Float rho = std::sqrt(point[0] * point[0] + point[2] * point[2]);

                Float theta = rho <= cylinder.radius ? utils::randomFloat(state) * 2 * constants::PI : std::atan2(point[2], point[0]) + (2 * utils::randomFloat(state) - 1) * std::acos(utils::clamp(cylinder.radius / rho, Float(-1), Float(1)));

                Float y = utils::randomFloat(state) * cylinder.height - Float(0.5) * cylinder.height;

                Vector<Float, 3> sample(cylinder.radius * std::cos(theta), y, cylinder.radius * std::sin(theta));

                Vector<Float, 3> direction = sample - point;

                Float distanceSquared = direction.lengthSquared();

                direction /= std::sqrt(distanceSquared);

                probabilityDensity = 0;

                if (std::fabs(sample[1]) <= Float(0.5) * cylinder.height) {
                    Float alpha = rho <= cylinder.radius ? constants::PI : std::acos(utils::clamp(cylinder.radius / rho, Float(-1), Float(1)));

                    Float cosTheta = std::fabs(dot(normalize(Vector<Float, 3>(sample[0], 0, sample[2])), direction));

                    if (cosTheta <= constants::GRAZING_COS) return direction;

                    probabilityDensity = 1 / (2 * alpha * cylinder.radius * cylinder.height) * distanceSquared / cosTheta;
                }

                return direction;
            }

            LAMBDA_HOST_DEVICE Vector<Float, 3> sampleTri(const Vector<Float, 3> & point, Float & probabilityDensity, Random & state) const {
                Float u = utils::randomFloat(state);
                Float v = utils::randomFloat(state);

                if (u + v > 1) {
                    u = 1 - u;
                    v = 1 - v;
                }

                Vector<Float, 3> corner = tri.mesh->getVertex(tri.index, 0);
                Vector<Float, 3> horizontal = tri.mesh->getVertex(tri.index, 1) - corner;
                Vector<Float, 3> vertical = tri.mesh->getVertex(tri.index, 2) - corner;

                Vector<Float, 3> sample = corner + horizontal * u + vertical * v;

                Vector<Float, 3> direction = sample - point;

                Float distanceSquared = direction.lengthSquared();

                direction /= std::sqrt(distanceSquared);

                probabilityDensity = distanceSquared / (std::fabs(dot(normalize(cross(horizontal, vertical)), direction)) * area);

                return direction;
            }

            LAMBDA_HOST_DEVICE Vector<Float, 3> samplePatch(const Vector<Float, 3> & point, Float & probabilityDensity, Random & state) const {
                Vector<Float, 3> corner0 = patch.mesh->getVertex(patch.index, 0);
                Vector<Float, 3> corner1 = patch.mesh->getVertex(patch.index, 1);
                Vector<Float, 3> corner2 = patch.mesh->getVertex(patch.index, 2);
                Vector<Float, 3> corner3 = patch.mesh->getVertex(patch.index, 3);

                Float w0 = cross(corner3 - corner0, corner1 - corner0).length();
                Float w1 = cross(corner3 - corner0, corner2 - corner3).length();
                Float w2 = cross(corner2 - corner1, corner1 - corner0).length();
                Float w3 = cross(corner2 - corner1, corner2 - corner3).length();

                Float v = utils::randomInLinear(w0 + w1, w2 + w3, state);
                Float u = utils::randomInLinear(utils::interpolate(w0, w2, v), utils::interpolate(w1, w3, v), state);

                Vector<Float, 3> sample = utils::interpolate(utils::interpolate(corner0, corner1, v), utils::interpolate(corner3, corner2, v), u);

                Vector<Float, 3> direction = normalize(sample - point);

                probabilityDensity = probabilityDensityPatch(point, direction);

                return direction;
            }

            LAMBDA_HOST_DEVICE bool intersectSphere(Intersection & intersection, const Ray & r) const {
                Float a = r.getDirection().lengthSquared();
                Float b = 2 * dot(r.getDirection(), r.getOrigin());
                Float c = r.getOrigin().lengthSquared() - sphere.radius * sphere.radius;
                Float d = b * b - 4 * a * c;

                if (d < 0) return false;

                d = std::sqrt(d);

                Float t1 = (-b - d) / (2 * a);
                Float t2 = (-b + d) / (2 * a);

                if (t1 <= 0 && t2 <= 0) return false;

                Float t = (t1 <= 0) ? t2 : t1;

                if (t >= intersection.t) return false;

                intersection.object = this;
                intersection.t = t;
                intersection.point = r.at(t);
                intersection.point *= sphere.radius / intersection.point.length();
                intersection.normal = normalize(intersection.point);
                intersection.tangent = Vector<Float, 3>(-intersection.point[2], 0, intersection.point[0]);

                Float tangentLengthSquared = intersection.tangent.lengthSquared();

                if (tangentLengthSquared <= constants::EPSILON_RELATIVE_SQUARED * sphere.radius * sphere.radius) intersection.tangent = Vector<Float, 3>(1, 0, 0);
                else intersection.tangent /= std::sqrt(tangentLengthSquared);

                intersection.textureCoordinate[0] = (std::atan2(intersection.normal[2], intersection.normal[0]) + constants::PI) / (2 * constants::PI);
                intersection.textureCoordinate[1] = std::acos(utils::clamp(intersection.normal[1], Float(-1), Float(1))) / constants::PI;
                intersection.geometricNormal = intersection.normal;
                intersection.isSurface = true;

                return true;
            }

            LAMBDA_HOST_DEVICE bool intersectDisk(Intersection & intersection, const Ray & r) const {
                Float t, radiusSquared;
                Vector<Float, 3> point;

                if (!rayDisk(r.getOrigin(), r.getDirection(), disk.radius, intersection.t, t, radiusSquared, point)) return false;

                intersection.object = this;
                intersection.t = t;
                intersection.point = point;
                intersection.normal = Vector<Float, 3>(0, 1, 0);
                intersection.tangent = Vector<Float, 3>(-point[2], 0, point[0]);

                Float tangentLengthSquared = intersection.tangent.lengthSquared();

                if (tangentLengthSquared <= constants::EPSILON_RELATIVE_SQUARED * disk.radius * disk.radius) intersection.tangent = Vector<Float, 3>(1, 0, 0);
                else intersection.tangent /= std::sqrt(tangentLengthSquared);

                intersection.textureCoordinate = Vector<Float, 2>(std::sqrt(radiusSquared) / disk.radius, std::atan2(point[2], point[0]) / (2 * constants::PI) + Float(0.5));
                intersection.geometricNormal = intersection.normal;
                intersection.isSurface = true;

                return true;
            }

            LAMBDA_HOST_DEVICE bool intersectCylinder(Intersection & intersection, const Ray & r) const {
                Float t, rho;
                Vector<Float, 3> point;

                if (!rayCylinder(r.getOrigin(), r.getDirection(), cylinder.radius, cylinder.height, intersection.t, t, rho, point)) return false;

                intersection.object = this;
                intersection.t = t;
                intersection.point = point;
                intersection.point *= cylinder.radius / std::sqrt(point[0] * point[0] + point[2] * point[2]);
                intersection.normal = normalize(Vector<Float, 3>(point[0], 0, point[2]));
                intersection.tangent = normalize(Vector<Float, 3>(-point[2], 0, point[0]));
                intersection.textureCoordinate = Vector<Float, 2>(std::atan2(point[2], point[0]) / (2 * constants::PI) + Float(0.5), (point[1] + Float(0.5) * cylinder.height) / cylinder.height);
                intersection.geometricNormal = intersection.normal;
                intersection.isSurface = true;

                return true;
            }

            LAMBDA_HOST_DEVICE bool intersectTri(Intersection & intersection, const Ray & r) const {
                Vector<Float, 3> corner0 = tri.mesh->getVertex(tri.index, 0);
                Vector<Float, 3> corner1 = tri.mesh->getVertex(tri.index, 1);
                Vector<Float, 3> corner2 = tri.mesh->getVertex(tri.index, 2);

                Float t, alpha, beta;

                if (!rayTri(r.getOrigin(), r.getDirection(), corner0, corner1, corner2, intersection.t, t, alpha, beta)) return false;

                Vector<Float, 3> horizontal = corner1 - corner0;
                Vector<Float, 3> vertical = corner2 - corner0;

                Vector<Float, 3> flatNormal = normalize(cross(horizontal, vertical));

                Vector<Float, 3> normal0 = tri.mesh->getNormal(tri.index, 0);
                Vector<Float, 3> normal1 = tri.mesh->getNormal(tri.index, 1);
                Vector<Float, 3> normal2 = tri.mesh->getNormal(tri.index, 2);

                if (!tri.mesh->hasNormals() || normal0 == Vector<Float, 3>()) normal0 = flatNormal;
                if (!tri.mesh->hasNormals() || normal1 == Vector<Float, 3>()) normal1 = flatNormal;
                if (!tri.mesh->hasNormals() || normal2 == Vector<Float, 3>()) normal2 = flatNormal;

                Vector<Float, 2> textureCoordinate0 = tri.mesh->getTextureCoordinate(tri.index, 0);
                Vector<Float, 2> textureCoordinate1 = tri.mesh->getTextureCoordinate(tri.index, 1);
                Vector<Float, 2> textureCoordinate2 = tri.mesh->getTextureCoordinate(tri.index, 2);

                if (!tri.mesh->hasTextureCoordinates() || (textureCoordinate1 - textureCoordinate0 == Vector<Float, 2>() && textureCoordinate0 == Vector<Float, 2>())) textureCoordinate1 = Vector<Float, 2>(1, 0);
                if (!tri.mesh->hasTextureCoordinates() || (textureCoordinate2 - textureCoordinate0 == Vector<Float, 2>() && textureCoordinate0 == Vector<Float, 2>())) textureCoordinate2 = Vector<Float, 2>(0, 1);
                if (!tri.mesh->hasTextureCoordinates()) textureCoordinate0 = Vector<Float, 2>(0, 0);

                Vector<Float, 3> normal = (1 - alpha - beta) * normal0 + alpha * normal1 + beta * normal2;

                Float normalLengthSquared = normal.lengthSquared();

                if (normalLengthSquared <= constants::GRAZING_COS_SQUARED) normal = flatNormal;
                else normal /= std::sqrt(normalLengthSquared);

                Matrix<Float, 2> invDelta = inverse(Matrix<Float, 2>(textureCoordinate1[0] - textureCoordinate0[0], textureCoordinate2[0] - textureCoordinate0[0], textureCoordinate1[1] - textureCoordinate0[1], textureCoordinate2[1] - textureCoordinate0[1]));

                Vector<Float, 3> rawTangent = invDelta.get(0, 0) * horizontal + invDelta.get(1, 0) * vertical;
                Vector<Float, 3> tangent = rawTangent - normal * dot(normal, rawTangent);

                Float tangentLengthSquared = tangent.lengthSquared();

                if (tangentLengthSquared <= constants::EPSILON_RELATIVE_SQUARED * rawTangent.lengthSquared()) tangent = std::fabs(normal[0]) > std::fabs(normal[1]) ? normalize(Vector<Float, 3>(-normal[2], 0, normal[0])) : normalize(Vector<Float, 3>(0, normal[2], -normal[1]));
                else tangent /= std::sqrt(tangentLengthSquared);

                intersection.object = this;
                intersection.t = t;
                intersection.point = (1 - alpha - beta) * corner0 + alpha * corner1 + beta * corner2;
                intersection.normal = normal;
                intersection.tangent = tangent;
                intersection.textureCoordinate = (1 - alpha - beta) * textureCoordinate0 + alpha * textureCoordinate1 + beta * textureCoordinate2;
                intersection.geometricNormal = flatNormal;
                intersection.isSurface = true;

                return true;
            }

            LAMBDA_HOST_DEVICE bool intersectPatch(Intersection & intersection, const Ray & r) const {
                Vector<Float, 3> corner0 = patch.mesh->getVertex(patch.index, 0);
                Vector<Float, 3> corner1 = patch.mesh->getVertex(patch.index, 1);
                Vector<Float, 3> corner2 = patch.mesh->getVertex(patch.index, 2);
                Vector<Float, 3> corner3 = patch.mesh->getVertex(patch.index, 3);

                Float t1, u1, v1, t2, u2, v2;

                if (!rayPatch(r.getOrigin(), r.getDirection(), corner0, corner1, corner2, corner3, intersection.t, t1, u1, v1, t2, u2, v2)) return false;

                Float t, u, v;

                if (t1 > 0 && t1 < intersection.t && (t2 <= 0 || t1 < t2)) {
                    t = t1;
                    u = u1;
                    v = v1;
                } else {
                    t = t2;
                    u = u2;
                    v = v2;
                }

                Vector<Float, 3> dpdu = utils::interpolate(corner3 - corner0, corner2 - corner1, v);
                Vector<Float, 3> dpdv = utils::interpolate(corner1 - corner0, corner2 - corner3, u);

                Vector<Float, 3> flatNormal = normalize(cross(dpdu, dpdv));

                Vector<Float, 3> normal0 = patch.mesh->getNormal(patch.index, 0);
                Vector<Float, 3> normal1 = patch.mesh->getNormal(patch.index, 1);
                Vector<Float, 3> normal2 = patch.mesh->getNormal(patch.index, 2);
                Vector<Float, 3> normal3 = patch.mesh->getNormal(patch.index, 3);

                if (!patch.mesh->hasNormals() || normal0 == Vector<Float, 3>()) normal0 = flatNormal;
                if (!patch.mesh->hasNormals() || normal1 == Vector<Float, 3>()) normal1 = flatNormal;
                if (!patch.mesh->hasNormals() || normal2 == Vector<Float, 3>()) normal2 = flatNormal;
                if (!patch.mesh->hasNormals() || normal3 == Vector<Float, 3>()) normal3 = flatNormal;

                Vector<Float, 2> textureCoordinate0 = patch.mesh->getTextureCoordinate(patch.index, 0);
                Vector<Float, 2> textureCoordinate1 = patch.mesh->getTextureCoordinate(patch.index, 1);
                Vector<Float, 2> textureCoordinate2 = patch.mesh->getTextureCoordinate(patch.index, 2);
                Vector<Float, 2> textureCoordinate3 = patch.mesh->getTextureCoordinate(patch.index, 3);

                if (!patch.mesh->hasTextureCoordinates() || (textureCoordinate1 - textureCoordinate0 == Vector<Float, 2>() && textureCoordinate0 == Vector<Float, 2>())) textureCoordinate1 = Vector<Float, 2>(1, 0);
                if (!patch.mesh->hasTextureCoordinates() || (textureCoordinate2 - textureCoordinate0 == Vector<Float, 2>() && textureCoordinate0 == Vector<Float, 2>())) textureCoordinate2 = Vector<Float, 2>(0, 1);
                if (!patch.mesh->hasTextureCoordinates() || (textureCoordinate3 - textureCoordinate0 == Vector<Float, 2>() && textureCoordinate0 == Vector<Float, 2>())) textureCoordinate3 = Vector<Float, 2>(1, 1);
                if (!patch.mesh->hasTextureCoordinates()) textureCoordinate0 = Vector<Float, 2>(0, 0);

                Vector<Float, 3> normal = utils::interpolate(utils::interpolate(normal0, normal1, v), utils::interpolate(normal3, normal2, v), u);

                Float normalLengthSquared = normal.lengthSquared();

                if (normalLengthSquared <= constants::GRAZING_COS_SQUARED) normal = flatNormal;
                else normal /= std::sqrt(normalLengthSquared);

                Matrix<Float, 2> invDelta = inverse(Matrix<Float, 2>(utils::interpolate(textureCoordinate3[0] - textureCoordinate0[0], textureCoordinate2[0] - textureCoordinate1[0], v), utils::interpolate(textureCoordinate3[1] - textureCoordinate0[1], textureCoordinate2[1] - textureCoordinate1[1], v), utils::interpolate(textureCoordinate1[0] - textureCoordinate0[0], textureCoordinate2[0] - textureCoordinate3[0], u), utils::interpolate(textureCoordinate1[1] - textureCoordinate0[1], textureCoordinate2[1] - textureCoordinate3[1], u)));

                Vector<Float, 3> rawTangent = invDelta.get(0, 0) * (corner1 - corner0) + invDelta.get(1, 0) * (corner3 - corner0);
                Vector<Float, 3> tangent = rawTangent - normal * dot(normal, rawTangent);

                Float tangentLengthSquared = tangent.lengthSquared();

                if (tangentLengthSquared <= constants::EPSILON_RELATIVE_SQUARED * rawTangent.lengthSquared()) tangent = std::fabs(normal[0]) > std::fabs(normal[1]) ? normalize(Vector<Float, 3>(-normal[2], 0, normal[0])) : normalize(Vector<Float, 3>(0, normal[2], -normal[1]));
                else tangent /= std::sqrt(tangentLengthSquared);

                intersection.object = this;
                intersection.t = t;
                intersection.point = utils::interpolate(utils::interpolate(corner0, corner1, v), utils::interpolate(corner3, corner2, v), u);
                intersection.normal = normal;
                intersection.tangent = tangent;
                intersection.textureCoordinate = utils::interpolate(utils::interpolate(textureCoordinate0, textureCoordinate1, v), utils::interpolate(textureCoordinate3, textureCoordinate2, v), u);
                intersection.geometricNormal = flatNormal;
                intersection.isSurface = true;

                return true;
            }

            LAMBDA_HOST_DEVICE static bool rayDisk(const Vector<Float, 3> & point, const Vector<Float, 3> & direction, Float radius, Float tMax, Float & t, Float & radiusSquared, Vector<Float, 3> & hitPoint) {
                if (std::fabs(direction[1]) == 0) return false;

                t = -point[1] / direction[1];

                if (t <= 0 || t >= tMax) return false;

                hitPoint = point + direction * t;

                radiusSquared = hitPoint[0] * hitPoint[0] + hitPoint[2] * hitPoint[2];

                if (radiusSquared > radius * radius) return false;

                return true;
            }

            LAMBDA_HOST_DEVICE static bool rayCylinder(const Vector<Float, 3> & point, const Vector<Float, 3> & direction, Float radius, Float height, Float tMax, Float & t, Float & rho, Vector<Float, 3> & hitPoint) {
                Float a = direction[0] * direction[0] + direction[2] * direction[2];

                if (a <= constants::EPSILON_RELATIVE_SQUARED * direction.lengthSquared()) return false;

                Float b = 2 * (point[0] * direction[0] + point[2] * direction[2]);
                Float c = point[0] * point[0] + point[2] * point[2] - radius * radius;
                Float d = b * b - 4 * a * c;

                if (d < 0) return false;

                d = std::sqrt(d);

                Float t1 = (-b - d) / (2 * a);
                Float t2 = (-b + d) / (2 * a);

                if (t1 <= 0 && t2 <= 0) return false;

                t = (t1 <= 0) ? t2 : t1;

                hitPoint = point + direction * t;

                rho = std::sqrt(point[0] * point[0] + point[2] * point[2]);

                if (std::fabs(hitPoint[1]) > Float(0.5) * height) {
                    if (rho > radius) return false;

                    t = t2;

                    hitPoint = point + direction * t;

                    if (std::fabs(hitPoint[1]) > Float(0.5) * height) return false;
                };

                return t > 0 && t < tMax;
            }

            LAMBDA_HOST_DEVICE static bool rayTri(const Vector<Float, 3> & point, const Vector<Float, 3> & direction, const Vector<Float, 3> & corner0, const Vector<Float, 3> & corner1, const Vector<Float, 3> & corner2, Float tMax, Float & t, Float & alpha, Float & beta) {
                Vector<Float, 3> p0 = corner0 - point;
                Vector<Float, 3> p1 = corner1 - point;
                Vector<Float, 3> p2 = corner2 - point;

                Float ax = std::fabs(direction[0]);
                Float ay = std::fabs(direction[1]);
                Float az = std::fabs(direction[2]);

                int kz = (ax > ay && ax > az) ? 0 : (ay > az) ? 1 : 2;
                int kx = kz == 2 ? 0 : kz + 1;
                int ky = kx == 2 ? 0 : kx + 1;

                if (direction[kz] == 0) return false;

                Float sx = -direction[kx] / direction[kz];
                Float sy = -direction[ky] / direction[kz];
                Float sz = 1 / direction[kz];

                p0[kx] += sx * p0[kz];
                p0[ky] += sy * p0[kz];
                p1[kx] += sx * p1[kz];
                p1[ky] += sy * p1[kz];
                p2[kx] += sx * p2[kz];
                p2[ky] += sy * p2[kz];

                Float e0 = p1[kx] * p2[ky] - p1[ky] * p2[kx];
                Float e1 = p2[kx] * p0[ky] - p2[ky] * p0[kx];
                Float e2 = p0[kx] * p1[ky] - p0[ky] * p1[kx];

                if (e0 == 0 || e1 == 0 || e2 == 0) {
                    e0 = Float(double(p1[kx]) * double(p2[ky]) - double(p1[ky]) * double(p2[kx]));
                    e1 = Float(double(p2[kx]) * double(p0[ky]) - double(p2[ky]) * double(p0[kx]));
                    e2 = Float(double(p0[kx]) * double(p1[ky]) - double(p0[ky]) * double(p1[kx]));
                }

                if ((e0 < 0 || e1 < 0 || e2 < 0) && (e0 > 0 || e1 > 0 || e2 > 0)) return false;

                Float determinant = e0 + e1 + e2;

                if (determinant == 0) return false;

                t = (e0 * p0[kz] + e1 * p1[kz] + e2 * p2[kz]) * sz;

                if ((determinant < 0 && (t >= 0 || t < tMax * determinant)) || (determinant > 0 && (t <= 0 || t > tMax * determinant))) return false;

                Float inverseDeterminant = 1 / determinant;

                t *= inverseDeterminant;

                alpha = e1 * inverseDeterminant;
                beta = e2 * inverseDeterminant;

                return true;
            }

            LAMBDA_HOST_DEVICE static bool rayPatch(const Vector<Float, 3> & point, const Vector<Float, 3> & direction, const Vector<Float, 3> & corner0, const Vector<Float, 3> & corner1, const Vector<Float, 3> & corner2, const Vector<Float, 3> & corner3, Float tMax, Float & t1, Float & u1, Float & v1, Float & t2, Float & u2, Float & v2) {
                Float a = dot(cross(corner3 - corner0, corner1 - corner2), direction);
                Float c = dot(cross(corner0 - point, direction), corner1 - corner0);
                Float b = dot(cross(corner3 - point, direction), corner2 - corner3) - (a + c);

                if (a == 0 && b == 0) return 0;

                Float d = b * b - 4 * a * c;

                if (d < 0) return 0;

                d = std::sqrt(d);

                Float q = Float(-0.5) * (b + (b < 0 ? -d : d));

                u1 = a == 0 ? -c / b : q / a;
                u2 = a != 0 && q != 0 ? c / q : -1;

                t1 = constants::MAX;
                v1 = 0;
                t2 = constants::MAX;
                v2 = 0;

                if (u1 >= 0 && u1 <= 1) {
                    Vector<Float, 3> uo = utils::interpolate(corner0, corner3, u1);
                    Vector<Float, 3> ud = utils::interpolate(corner1, corner2, u1) - uo;
                    Vector<Float, 3> delta = uo - point;
                    Vector<Float, 3> perp = cross(direction, ud);

                    Float perpLengthSquared = perp.lengthSquared();

                    v1 = dot(cross(delta, direction), perp) / perpLengthSquared;

                    if (v1 >= 0 && v1 <= 1) t1 = dot(cross(delta, ud), perp) / perpLengthSquared;
                }

                if (u2 >= 0 && u2 <= 1) {
                    Vector<Float, 3> uo = utils::interpolate(corner0, corner3, u2);
                    Vector<Float, 3> ud = utils::interpolate(corner1, corner2, u2) - uo;
                    Vector<Float, 3> delta = uo - point;
                    Vector<Float, 3> perp = cross(direction, ud);

                    Float perpLengthSquared = perp.lengthSquared();

                    v2 = dot(cross(delta, direction), perp) / perpLengthSquared;

                    if (v2 >= 0 && v2 <= 1) t2 = dot(cross(delta, ud), perp) / perpLengthSquared;
                }

                return (t1 > 0 && t1 < tMax) || (t2 > 0 && t2 < tMax);
            }
    };
}