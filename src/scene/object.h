#pragma once

#include <cmath>

#include "core/platform.h"
#include "core/utils.h"
#include "math/intersection.h"
#include "math/matrix.h"
#include "math/random.h"
#include "math/ray.h"
#include "math/vector.h"
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

                if ((corner3 - corner0 - (corner2 - corner1)).length() < constants::EPSILON) object.area = cross(corner1 - corner0, corner3 - corner0).length();
                else if (std::fabs(dot(cross(corner1 - corner0, corner3 - corner0), corner2 - corner0)) < constants::EPSILON) object.area = Float(0.5) * cross(corner2 - corner0, corner3 - corner1).length();
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

            LAMBDA_HOST_DEVICE Vector<Float, 3> minBounds() const {
                switch (type) {
                    case Type::SPHERE: return Vector<Float, 3>(-sphere.radius, -sphere.radius, -sphere.radius);
                    case Type::DISK: return Vector<Float, 3>(-disk.radius, -constants::EPSILON, -disk.radius);
                    case Type::CYLINDER: return Vector<Float, 3>(-cylinder.radius, Float(-0.5) * cylinder.height, -cylinder.radius);
                    case Type::TRI: return min(tri.mesh->getVertex(tri.index, 0), min(tri.mesh->getVertex(tri.index, 1), tri.mesh->getVertex(tri.index, 2))) - Vector<Float, 3>(constants::EPSILON, constants::EPSILON, constants::EPSILON);
                    case Type::PATCH: return min(patch.mesh->getVertex(patch.index, 0), min(patch.mesh->getVertex(patch.index, 1), min(patch.mesh->getVertex(patch.index, 2), patch.mesh->getVertex(patch.index, 3)))) - Vector<Float, 3>(constants::EPSILON, constants::EPSILON, constants::EPSILON);
                }

                return Vector<Float, 3>();
            }

            LAMBDA_HOST_DEVICE Vector<Float, 3> maxBounds() const {
                switch (type) {
                    case Type::SPHERE: return Vector<Float, 3>(sphere.radius, sphere.radius, sphere.radius);
                    case Type::DISK: return Vector<Float, 3>(disk.radius, constants::EPSILON, disk.radius);
                    case Type::CYLINDER: return Vector<Float, 3>(cylinder.radius, Float(0.5) * cylinder.height, cylinder.radius);
                    case Type::TRI: return max(tri.mesh->getVertex(tri.index, 0), max(tri.mesh->getVertex(tri.index, 1), tri.mesh->getVertex(tri.index, 2))) + Vector<Float, 3>(constants::EPSILON, constants::EPSILON, constants::EPSILON);
                    case Type::PATCH: return max(patch.mesh->getVertex(patch.index, 0), max(patch.mesh->getVertex(patch.index, 1), max(patch.mesh->getVertex(patch.index, 2), patch.mesh->getVertex(patch.index, 3)))) + Vector<Float, 3>(constants::EPSILON, constants::EPSILON, constants::EPSILON);
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

            LAMBDA_HOST_DEVICE Float pdf(const Vector<Float, 3> & point, const Vector<Float, 3> & direction) const {
                switch (type) {
                    case Type::SPHERE: return pdfSphere(point, direction);
                    case Type::DISK: return pdfDisk(point, direction);
                    case Type::CYLINDER: return pdfCylinder(point, direction);
                    case Type::TRI: return pdfTri(point, direction);
                    case Type::PATCH: return pdfPatch(point, direction);
                }

                return 0;
            }

            LAMBDA_HOST_DEVICE Vector<Float, 3> sample(const Vector<Float, 3> & point, Random & state) const {
                switch (type) {
                    case Type::SPHERE: return sampleSphere(point, state);
                    case Type::DISK: return sampleDisk(point, state);
                    case Type::CYLINDER: return sampleCylinder(point, state);
                    case Type::TRI: return sampleTri(point, state);
                    case Type::PATCH: return samplePatch(point, state);
                }

                return Vector<Float, 3>();
            }

            LAMBDA_HOST_DEVICE bool intersect(const Ray & r, Intersection & intersection) const {
                switch (type) {
                    case Type::SPHERE: return intersectSphere(r, intersection);
                    case Type::DISK: return intersectDisk(r, intersection);
                    case Type::CYLINDER: return intersectCylinder(r, intersection);
                    case Type::TRI: return intersectTri(r, intersection);
                    case Type::PATCH: return intersectPatch(r, intersection);
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
                Vector<Float, 3> corner0 = tri.mesh->getVertex(tri.index, 0);
                Vector<Float, 3> corner1 = tri.mesh->getVertex(tri.index, 1);
                Vector<Float, 3> corner2 = tri.mesh->getVertex(tri.index, 2);
                Vector<Float, 3> corner3 = tri.mesh->getVertex(tri.index, 3);

                Vector<Float, 3> center = (corner0 + corner1 + corner2 + corner3) / 4;

                Float distance1 = (corner0 - center).lengthSquared();
                Float distance2 = (corner1 - center).lengthSquared();
                Float distance3 = (corner2 - center).lengthSquared();
                Float distance4 = (corner3 - center).lengthSquared();

                return std::sqrt(std::fmax(distance1, std::fmax(distance2, std::fmax(distance3, distance4))));
            }

            LAMBDA_HOST_DEVICE Float pdfSphere(const Vector<Float, 3> & point, const Vector<Float, 3> & direction) const {
                Float distanceSquared = point.lengthSquared();
                Float radiusSquared = sphere.radius * sphere.radius;

                if (distanceSquared <= radiusSquared) return 1 / (4 * constants::PI);

                Float cosThetaMax = std::sqrt(1 - radiusSquared / distanceSquared);
                Float cosTheta = dot(normalize(-point), direction);

                if (cosTheta < cosThetaMax) return 0;

                return 1 / (2 * constants::PI * (1 - cosThetaMax));
            }

            LAMBDA_HOST_DEVICE Float pdfDisk(const Vector<Float, 3> & point, const Vector<Float, 3> & direction) const {
                Float distanceSquared = -point[1] / direction[1];

                if (distanceSquared < constants::EPSILON) return 0;

                Vector<Float, 3> hitPoint = point + direction * distanceSquared;

                if (hitPoint[0] * hitPoint[0] + hitPoint[2] * hitPoint[2] > disk.radius * disk.radius) return 0;

                distanceSquared *= distanceSquared;

                Float cosTheta = std::fabs(direction[1]);

                return 1 / (constants::PI * disk.radius * disk.radius) * distanceSquared / std::fmax(cosTheta, constants::EPSILON);
            }

            LAMBDA_HOST_DEVICE Float pdfCylinder(const Vector<Float, 3> & point, const Vector<Float, 3> & direction) const {
                Float a = direction[0] * direction[0] + direction[2] * direction[2];

                if (a < constants::EPSILON_SQUARED) return 0;

                Float b = 2 * (point[0] * direction[0] + point[2] * direction[2]);
                Float c = point[0] * point[0] + point[2] * point[2] - cylinder.radius * cylinder.radius;
                Float d = b * b - 4 * a * c;

                if (d < 0) return 0;

                d = std::sqrt(d);

                Float t1 = (-b - d) / (2 * a);
                Float t2 = (-b + d) / (2 * a);

                if (t1 < constants::EPSILON && t2 < constants::EPSILON) return 0;

                Float t = (t1 < constants::EPSILON) ? t2 : t1;

                Vector<Float, 3> hitPoint = point + direction * t;

                if (std::fabs(hitPoint[1]) > Float(0.5) * cylinder.height) {
                    t = t2;

                    hitPoint = point + direction * t;

                    if (std::fabs(hitPoint[1]) > Float(0.5) * cylinder.height) return 0;
                };

                Float distanceSquared = t * t;

                Float cosTheta = std::fabs(dot(normalize(Vector<Float, 3>(hitPoint[0], 0, hitPoint[2])), direction));

                return 1 / (2 * constants::PI * cylinder.radius * cylinder.height) * distanceSquared / std::fmax(cosTheta, constants::EPSILON);
            }

            LAMBDA_HOST_DEVICE Float pdfTri(const Vector<Float, 3> & point, const Vector<Float, 3> & direction) const {
                Vector<Float, 3> corner = tri.mesh->getVertex(tri.index, 0);
                Vector<Float, 3> horizontal = tri.mesh->getVertex(tri.index, 1) - corner;
                Vector<Float, 3> vertical = tri.mesh->getVertex(tri.index, 2) - corner;

                Vector<Float, 3> rayCrossVertical = cross(direction, vertical);

                Float determinant = dot(horizontal, rayCrossVertical);

                if (std::fabs(determinant) < constants::EPSILON_SQUARED) return 0;

                Float inverseDeterminant = 1 / determinant;

                Vector<Float, 3> oc = point - corner;

                Float alpha = inverseDeterminant * dot(oc, rayCrossVertical);

                if (alpha < 0 || alpha > 1) return 0;

                Vector<Float, 3> ocCrossHorizontal = cross(oc, horizontal);

                Float beta = inverseDeterminant * dot(direction, ocCrossHorizontal);

                if (beta < 0 || alpha + beta > 1) return 0;

                Float t = inverseDeterminant * dot(vertical, ocCrossHorizontal);

                if (t < constants::EPSILON) return 0;

                Vector<Float, 3> normal = cross(horizontal, vertical);

                normal = normalize(normal);

                Float cosTheta = dot(normalize(normal), direction);

                return t * t / (std::fabs(cosTheta) * area);
            }

            LAMBDA_HOST_DEVICE Float pdfPatch(const Vector<Float, 3> & point, const Vector<Float, 3> & direction) const {
                Vector<Float, 3> corner0 = patch.mesh->getVertex(patch.index, 0);
                Vector<Float, 3> corner1 = patch.mesh->getVertex(patch.index, 1);
                Vector<Float, 3> corner2 = patch.mesh->getVertex(patch.index, 2);
                Vector<Float, 3> corner3 = patch.mesh->getVertex(patch.index, 3);

                Float w0 = cross(corner3 - corner0, corner1 - corner0).length();
                Float w1 = cross(corner3 - corner0, corner2 - corner3).length();
                Float w2 = cross(corner2 - corner1, corner1 - corner0).length();
                Float w3 = cross(corner2 - corner1, corner2 - corner3).length();

                Float totalWeight = w0 + w1 + w2 + w3;

                Float a = dot(cross(corner3 - corner0, corner1 - corner2), direction);
                Float c = dot(cross(corner0 - point, direction), corner1 - corner0);
                Float b = dot(cross(corner3 - point, direction), corner2 - corner3) - (a + c);

                if (std::fabs(a) < constants::EPSILON && std::fabs(b) < constants::EPSILON) return 0;

                Float d = b * b - 4 * a * c;

                if (d < 0) return 0;

                d = std::sqrt(d);

                Float q = Float(-0.5) * (b + (b < 0 ? -d : d));

                Float u1 = std::fabs(a) < constants::EPSILON ? -c / b : q / a;
                Float u2 = std::fabs(a) > constants::EPSILON && std::fabs(q) > constants::EPSILON ? c / q : -1;

                Float sum = 0;

                if (u1 >= 0 && u1 <= 1) {
                    Vector<Float, 3> uo = utils::interpolate(corner0, corner3, u1);
                    Vector<Float, 3> ud = utils::interpolate(corner1, corner2, u1) - uo;
                    Vector<Float, 3> delta = uo - point;
                    Vector<Float, 3> perp = cross(direction, ud);

                    Float perpLengthSquared = perp.lengthSquared();

                    Float v1 = dot(cross(delta, direction), perp) / perpLengthSquared;
                    Float t1 = dot(cross(delta, ud), perp) / perpLengthSquared;

                    if (v1 >= 0 && v1 <= 1 && t1 > constants::EPSILON) {
                        Vector<Float, 3> dpdu = utils::interpolate(corner3 - corner0, corner2 - corner1, v1);
                        Vector<Float, 3> dpdv = utils::interpolate(corner1 - corner0, corner2 - corner3, u1);

                        Vector<Float, 3> normal = cross(dpdu, dpdv);

                        Float jacobian = normal.length();

                        if (jacobian > constants::EPSILON) {
                            Float cosTheta = std::fabs(dot(normal / jacobian, direction));

                            if (cosTheta > constants::EPSILON) sum += 4 * ((1 - u1) * (1 - v1) * w0 + u1 * (1 - v1) * w1 + (1 - u1) * v1 * w2 + u1 * v1 * w3) / totalWeight / jacobian * t1 * t1 / cosTheta;
                        }
                    }
                }

                if (u2 >= 0 && u2 <= 1) {
                    Vector<Float, 3> uo = utils::interpolate(corner0, corner3, u2);
                    Vector<Float, 3> ud = utils::interpolate(corner1, corner2, u2) - uo;
                    Vector<Float, 3> delta = uo - point;
                    Vector<Float, 3> perp = cross(direction, ud);

                    Float perpLengthSquared = perp.lengthSquared();

                    Float v2 = dot(cross(delta, direction), perp) / perpLengthSquared;
                    Float t2 = dot(cross(delta, ud), perp) / perpLengthSquared;

                    if (v2 >= 0 && v2 <= 1 && t2 > constants::EPSILON) {
                        Vector<Float, 3> dpdu = utils::interpolate(corner3 - corner0, corner2 - corner1, v2);
                        Vector<Float, 3> dpdv = utils::interpolate(corner1 - corner0, corner2 - corner3, u2);

                        Vector<Float, 3> normal = cross(dpdu, dpdv);

                        Float jacobian = normal.length();

                        if (jacobian > constants::EPSILON) {
                            Float cosTheta = std::fabs(dot(normal / jacobian, direction));

                            if (cosTheta > constants::EPSILON) sum += 4 * ((1 - u2) * (1 - v2) * w0 + u2 * (1 - v2) * w1 + (1 - u2) * v2 * w2 + u2 * v2 * w3) / totalWeight / jacobian * t2 * t2 / cosTheta;
                        }
                    }
                }

                return sum;
            }

            LAMBDA_HOST_DEVICE Vector<Float, 3> sampleSphere(const Vector<Float, 3> & point, Random & state) const {
                Float distanceSquared = point.lengthSquared();
                Float radiusSquared = sphere.radius * sphere.radius;

                if (distanceSquared <= radiusSquared) return utils::randomUnitVector(state);

                Float cosThetaMax = std::sqrt(1 - radiusSquared / distanceSquared);

                return utils::randomInCone(normalize(-point), cosThetaMax, state);
            }

            LAMBDA_HOST_DEVICE Vector<Float, 3> sampleDisk(const Vector<Float, 3> & point, Random & state) const {
                Vector<Float, 3> sample = utils::randomInHemisphere(Vector<Float, 3>(0, 1, 0), state) * disk.radius;

                sample[1] = 0;

                return normalize(sample - point);
            }

            LAMBDA_HOST_DEVICE Vector<Float, 3> sampleCylinder(const Vector<Float, 3> & point, Random & state) const {
                Float theta = utils::randomFloat(state) * 2 * constants::PI;
                Float y = utils::randomFloat(state) * cylinder.height - Float(0.5) * cylinder.height;

                Vector<Float, 3> direction = Vector<Float, 3>(cylinder.radius * std::cos(theta), y, cylinder.radius * std::sin(theta)) - point;

                return normalize(direction);
            }

            LAMBDA_HOST_DEVICE Vector<Float, 3> sampleTri(const Vector<Float, 3> & point, Random & state) const {
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

                return normalize(sample - point);
            }

            LAMBDA_HOST_DEVICE Vector<Float, 3> samplePatch(const Vector<Float, 3> & point, Random & state) const {
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

                return normalize(utils::interpolate(utils::interpolate(corner0, corner1, v), utils::interpolate(corner3, corner2, v), u) - point);
            }

            LAMBDA_HOST_DEVICE bool intersectSphere(const Ray & r, Intersection & intersection) const {
                Float a = r.getDirection().lengthSquared();
                Float b = 2 * dot(r.getDirection(), r.getOrigin());
                Float c = r.getOrigin().lengthSquared() - sphere.radius * sphere.radius;
                Float d = b * b - 4 * a * c;

                if (d < 0) return false;

                d = std::sqrt(d);

                Float t1 = (-b - d) / (2 * a);
                Float t2 = (-b + d) / (2 * a);

                if (t1 < constants::EPSILON && t2 < constants::EPSILON) return false;

                Float t = (t1 < constants::EPSILON) ? t2 : t1;

                if (t >= intersection.t) return false;

                intersection.t = t;
                intersection.point = r.at(t);
                intersection.normal = normalize(intersection.point);
                intersection.tangent = normalize(Vector<Float, 3>(-intersection.point[2], 0, intersection.point[0]));
                intersection.textureCoordinate[0] = (std::atan2(intersection.normal[2], intersection.normal[0]) + constants::PI) / (2 * constants::PI);
                intersection.textureCoordinate[1] = std::acos(intersection.normal[1]) / constants::PI;
                intersection.isSurface = true;

                return true;
            }

            LAMBDA_HOST_DEVICE bool intersectDisk(const Ray & r, Intersection & intersection) const {
                if (std::fabs(r.getDirection()[1]) < constants::EPSILON) return false;

                Float t = -r.getOrigin()[1] / r.getDirection()[1];

                if (t < constants::EPSILON || t >= intersection.t) return false;

                Vector<Float, 3> point = r.at(t);

                Float radiusSquared = point[0] * point[0] + point[2] * point[2];

                if (radiusSquared > disk.radius * disk.radius) return false;

                intersection.t = t;
                intersection.point = point;
                intersection.normal = Vector<Float, 3>(0, 1, 0);
                intersection.tangent = normalize(Vector<Float, 3>(-point[2], 0, point[0]));
                intersection.textureCoordinate = Vector<Float, 2>(std::sqrt(radiusSquared) / disk.radius, std::atan2(point[2], point[0]) / (2 * constants::PI) + Float(0.5));
                intersection.isSurface = true;

                return true;
            }

            LAMBDA_HOST_DEVICE bool intersectCylinder(const Ray & r, Intersection & intersection) const {
                Float a = r.getDirection()[0] * r.getDirection()[0] + r.getDirection()[2] * r.getDirection()[2];

                if (a < constants::EPSILON_SQUARED) return false;

                Float b = 2 * (r.getOrigin()[0] * r.getDirection()[0] + r.getOrigin()[2] * r.getDirection()[2]);
                Float c = r.getOrigin()[0] * r.getOrigin()[0] + r.getOrigin()[2] * r.getOrigin()[2] - cylinder.radius * cylinder.radius;
                Float d = b * b - 4 * a * c;

                if (d < 0) return false;

                d = std::sqrt(d);

                Float t1 = (-b - d) / (2 * a);
                Float t2 = (-b + d) / (2 * a);

                if (t1 < constants::EPSILON && t2 < constants::EPSILON) return false;

                Float t = (t1 < constants::EPSILON) ? t2 : t1;

                Vector<Float, 3> point = r.at(t);

                if (std::fabs(point[1]) > Float(0.5) * cylinder.height) {
                    t = t2;

                    point = r.at(t);

                    if (std::fabs(point[1]) > Float(0.5) * cylinder.height) return false;
                }

                if (t >= intersection.t) return false;

                intersection.t = t;
                intersection.point = point;
                intersection.normal = normalize(Vector<Float, 3>(point[0], 0, point[2]));
                intersection.tangent = normalize(Vector<Float, 3>(-point[2], 0, point[0]));
                intersection.textureCoordinate = Vector<Float, 2>(std::atan2(point[2], point[0]) / (2 * constants::PI) + Float(0.5), (point[1] + Float(0.5) * cylinder.height) / cylinder.height);
                intersection.isSurface = true;

                return true;
            }

            LAMBDA_HOST_DEVICE bool intersectTri(const Ray & r, Intersection & intersection) const {
                Vector<Float, 3> corner = tri.mesh->getVertex(tri.index, 0);
                Vector<Float, 3> horizontal = tri.mesh->getVertex(tri.index, 1) - corner;
                Vector<Float, 3> vertical = tri.mesh->getVertex(tri.index, 2) - corner;

                Vector<Float, 3> rayCrossVertical = cross(r.getDirection(), vertical);

                Float determinant = dot(horizontal, rayCrossVertical);

                if (std::fabs(determinant) < constants::EPSILON_SQUARED) return false;

                Float inverseDeterminant = 1 / determinant;

                Vector<Float, 3> oc = r.getOrigin() - corner;

                Float alpha = inverseDeterminant * dot(oc, rayCrossVertical);

                if (alpha < 0 || alpha > 1) return false;

                Vector<Float, 3> ocCrossHorizontal = cross(oc, horizontal);

                Float beta = inverseDeterminant * dot(r.getDirection(), ocCrossHorizontal);

                if (beta < 0 || alpha + beta > 1) return false;

                Float t = inverseDeterminant * dot(vertical, ocCrossHorizontal);

                if (t < constants::EPSILON || t >= intersection.t) return false;

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

                if (normalLengthSquared < constants::EPSILON_SQUARED) normal = flatNormal;
                else normal /= std::sqrt(normalLengthSquared);

                Matrix<Float, 2> invDelta = inverse(Matrix<Float, 2>(textureCoordinate1[0] - textureCoordinate0[0], textureCoordinate2[0] - textureCoordinate0[0], textureCoordinate1[1] - textureCoordinate0[1], textureCoordinate2[1] - textureCoordinate0[1]));

                Vector<Float, 3> rawTangent = invDelta.get(0, 0) * horizontal + invDelta.get(1, 0) * vertical;
                Vector<Float, 3> tangent = rawTangent - normal * dot(normal, rawTangent);

                Float tangentLengthSquared = tangent.lengthSquared();

                if (tangentLengthSquared < constants::EPSILON_SQUARED) tangent = normal == flatNormal ? rawTangent : cross(normal, rawTangent);
                else tangent /= std::sqrt(tangentLengthSquared);

                intersection.t = t;
                intersection.point = r.at(t);
                intersection.normal = normal;
                intersection.tangent = tangent;
                intersection.textureCoordinate = (1 - alpha - beta) * textureCoordinate0 + alpha * textureCoordinate1 + beta * textureCoordinate2;
                intersection.isSurface = true;

                return true;
            }

            LAMBDA_HOST_DEVICE bool intersectPatch(const Ray & r, Intersection & intersection) const {
                Vector<Float, 3> corner0 = patch.mesh->getVertex(patch.index, 0);
                Vector<Float, 3> corner1 = patch.mesh->getVertex(patch.index, 1);
                Vector<Float, 3> corner2 = patch.mesh->getVertex(patch.index, 2);
                Vector<Float, 3> corner3 = patch.mesh->getVertex(patch.index, 3);

                Float a = dot(cross(corner3 - corner0, corner1 - corner2), r.getDirection());
                Float c = dot(cross(corner0 - r.getOrigin(), r.getDirection()), corner1 - corner0);
                Float b = dot(cross(corner3 - r.getOrigin(), r.getDirection()), corner2 - corner3) - (a + c);

                if (std::fabs(a) < constants::EPSILON && std::fabs(b) < constants::EPSILON) return false;

                Float d = b * b - 4 * a * c;

                if (d < 0) return false;

                d = std::sqrt(d);

                Float q = Float(-0.5) * (b + (b < 0 ? -d : d));

                Float u1 = std::fabs(a) < constants::EPSILON ? -c / b : q / a;
                Float u2 = std::fabs(a) > constants::EPSILON && std::fabs(q) > constants::EPSILON ? c / q : -1;

                Float t = constants::MAX, u = 0, v = 0;

                if (u1 >= 0 && u1 <= 1) {
                    Vector<Float, 3> uo = utils::interpolate(corner0, corner3, u1);
                    Vector<Float, 3> ud = utils::interpolate(corner1, corner2, u1) - uo;
                    Vector<Float, 3> delta = uo - r.getOrigin();
                    Vector<Float, 3> perp = cross(r.getDirection(), ud);

                    Float perpLengthSquared = perp.lengthSquared();

                    Float v1 = dot(cross(delta, r.getDirection()), perp) / perpLengthSquared;
                    Float t1 = dot(cross(delta, ud), perp) / perpLengthSquared;

                    if (v1 >= 0 && v1 <= 1 && t1 > constants::EPSILON) {
                        t = t1;
                        u = u1;
                        v = v1;
                    }
                }

                if (u2 >= 0 && u2 <= 1) {
                    Vector<Float, 3> uo = utils::interpolate(corner0, corner3, u2);
                    Vector<Float, 3> ud = utils::interpolate(corner1, corner2, u2) - uo;
                    Vector<Float, 3> delta = uo - r.getOrigin();
                    Vector<Float, 3> perp = cross(r.getDirection(), ud);

                    Float perpLengthSquared = perp.lengthSquared();

                    Float v2 = dot(cross(delta, r.getDirection()), perp) / perpLengthSquared;
                    Float t2 = dot(cross(delta, ud), perp) / perpLengthSquared;

                    if (v2 >= 0 && v2 <= 1 && t2 > constants::EPSILON && t2 < t) {
                        t = t2;
                        u = u2;
                        v = v2;
                    }
                }

                if (t >= intersection.t) return false;

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

                if (normalLengthSquared < constants::EPSILON_SQUARED) normal = flatNormal;
                else normal /= std::sqrt(normalLengthSquared);

                Matrix<Float, 2> invDelta = inverse(Matrix<Float, 2>(utils::interpolate(textureCoordinate3[0] - textureCoordinate0[0], textureCoordinate2[0] - textureCoordinate1[0], v), utils::interpolate(textureCoordinate3[1] - textureCoordinate0[1], textureCoordinate2[1] - textureCoordinate1[1], v), utils::interpolate(textureCoordinate1[0] - textureCoordinate0[0], textureCoordinate2[0] - textureCoordinate3[0], u), utils::interpolate(textureCoordinate1[1] - textureCoordinate0[1], textureCoordinate2[1] - textureCoordinate3[1], u)));

                Vector<Float, 3> rawTangent = invDelta.get(0, 0) * (corner1 - corner0) + invDelta.get(1, 0) * (corner3 - corner0);
                Vector<Float, 3> tangent = rawTangent - normal * dot(normal, rawTangent);

                Float tangentLengthSquared = tangent.lengthSquared();

                if (tangentLengthSquared < constants::EPSILON_SQUARED) tangent = normal == flatNormal ? rawTangent : normalize(dpdu);
                else tangent /= std::sqrt(tangentLengthSquared);

                intersection.t = t;
                intersection.point = utils::interpolate(utils::interpolate(corner0, corner1, v), utils::interpolate(corner3, corner2, v), u);
                intersection.normal = normal;
                intersection.tangent = tangent;
                intersection.textureCoordinate = utils::interpolate(utils::interpolate(textureCoordinate0, textureCoordinate1, v), utils::interpolate(textureCoordinate3, textureCoordinate2, v), u);
                intersection.isSurface = true;

                return true;
            }
    };
}