#include <cmath>
#include <vector>

#include <gtest/gtest.h>

#include "core/constants.h"
#include "core/utils.h"
#include "geometry/background.h"
#include "geometry/bvh.h"
#include "geometry/instance.h"
#include "geometry/intersection.h"
#include "geometry/mesh.h"
#include "geometry/object.h"
#include "geometry/scene.h"
#include "test/test.h"
#include "test/tolerances.h"

namespace lambda {
    namespace {
        class MeshTest : public RandomizedTest {};

        TEST_F(MeshTest, TriangleVertices) {
            Vector<Float, 3> vertices[4] = { Vector<Float, 3>(0, 0, 0), Vector<Float, 3>(1, 0, 0), Vector<Float, 3>(0, 1, 0), Vector<Float, 3>(1, 1, 0) };
            Vector<int, 3> faces[2] = { Vector<int, 3>(0, 1, 2), Vector<int, 3>(1, 3, 2) };

            Mesh mesh = Mesh::makeTri(vertices, nullptr, nullptr, faces);

            expectEqual(mesh.getVertex(0, 1), vertices[1]);
            expectEqual(mesh.getVertex(1, 0), vertices[1]);
            expectEqual(mesh.getVertex(1, 1), vertices[3]);
            expectEqual(mesh.getVertex(1, 2), vertices[2]);
        }

        TEST_F(MeshTest, TriangleCornerOutOfRangeIsZero) {
            Vector<Float, 3> vertices[3] = { Vector<Float, 3>(1, 0, 0), Vector<Float, 3>(1, 1, 0), Vector<Float, 3>(1, 0, 1) };
            Vector<int, 3> faces[1] = { Vector<int, 3>(0, 1, 2) };

            Mesh mesh = Mesh::makeTri(vertices, nullptr, nullptr, faces);

            expectEqual(mesh.getVertex(0, 3), Vector<Float, 3>());
            expectEqual(mesh.getVertex(0, -1), Vector<Float, 3>());
        }

        TEST_F(MeshTest, QuadVertices) {
            Vector<Float, 3> vertices[4] = { Vector<Float, 3>(0, 0, 0), Vector<Float, 3>(1, 0, 0), Vector<Float, 3>(1, 1, 0), Vector<Float, 3>(0, 1, 0) };
            Vector<int, 4> faces[1] = { Vector<int, 4>(3, 2, 1, 0) };

            Mesh mesh = Mesh::makeQuad(vertices, nullptr, nullptr, faces);

            for (int j = 0; j < 4; j++) expectEqual(mesh.getVertex(0, j), vertices[3 - j]);

            expectEqual(mesh.getVertex(0, 4), Vector<Float, 3>());
        }

        TEST_F(MeshTest, MissingAttributesAreReportedAndZero) {
            Vector<Float, 3> vertices[3] = { Vector<Float, 3>(0, 0, 0), Vector<Float, 3>(1, 0, 0), Vector<Float, 3>(0, 1, 0) };
            Vector<int, 3> faces[1] = { Vector<int, 3>(0, 1, 2) };

            Mesh mesh = Mesh::makeTri(vertices, nullptr, nullptr, faces);

            EXPECT_FALSE(mesh.hasNormals());
            EXPECT_FALSE(mesh.hasTextureCoordinates());

            expectEqual(mesh.getNormal(0, 0), Vector<Float, 3>());
            expectEqual(mesh.getTextureCoordinate(0, 0), Vector<Float, 2>());
        }

        TEST_F(MeshTest, TriangleAttributesFollowFaceIndices) {
            Vector<Float, 3> vertices[4] = { Vector<Float, 3>(0, 0, 0), Vector<Float, 3>(1, 0, 0), Vector<Float, 3>(0, 1, 0), Vector<Float, 3>(1, 1, 0) };
            Vector<Float, 3> normals[4] = { Vector<Float, 3>(0, 0, 1), Vector<Float, 3>(0, 1, 0), Vector<Float, 3>(1, 0, 0), Vector<Float, 3>(0, 0, -1) };
            Vector<Float, 2> coordinates[4] = { Vector<Float, 2>(0, 0), Vector<Float, 2>(1, 0), Vector<Float, 2>(0, 1), Vector<Float, 2>(1, 1) };
            Vector<int, 3> faces[2] = { Vector<int, 3>(0, 1, 2), Vector<int, 3>(1, 3, 2) };

            Mesh mesh = Mesh::makeTri(vertices, normals, coordinates, faces);

            EXPECT_TRUE(mesh.hasNormals());
            EXPECT_TRUE(mesh.hasTextureCoordinates());

            expectEqual(mesh.getNormal(1, 1), normals[3]);
            expectEqual(mesh.getTextureCoordinate(1, 1), coordinates[3]);
            expectEqual(mesh.getNormal(1, 3), Vector<Float, 3>());
            expectEqual(mesh.getTextureCoordinate(1, 3), Vector<Float, 2>());
        }

        TEST_F(MeshTest, QuadAttributesFollowFaceIndices) {
            Vector<Float, 3> vertices[4] = { Vector<Float, 3>(0, 0, 0), Vector<Float, 3>(1, 0, 0), Vector<Float, 3>(1, 1, 0), Vector<Float, 3>(0, 1, 0) };
            Vector<Float, 3> normals[4] = { Vector<Float, 3>(0, 0, 1), Vector<Float, 3>(0, 1, 0), Vector<Float, 3>(1, 0, 0), Vector<Float, 3>(0, 0, -1) };
            Vector<Float, 2> coordinates[4] = { Vector<Float, 2>(0, 0), Vector<Float, 2>(1, 0), Vector<Float, 2>(1, 1), Vector<Float, 2>(0, 1) };
            Vector<int, 4> faces[1] = { Vector<int, 4>(0, 1, 2, 3) };

            Mesh mesh = Mesh::makeQuad(vertices, normals, coordinates, faces);

            EXPECT_TRUE(mesh.hasNormals());
            EXPECT_TRUE(mesh.hasTextureCoordinates());

            expectEqual(mesh.getNormal(0, 2), normals[2]);
            expectEqual(mesh.getTextureCoordinate(0, 3), coordinates[3]);
        }

        class ObjectTest : public RandomizedTest {
            protected:
                Mesh triangle;
                Mesh quad;

                void SetUp() override {
                    Vector<Float, 3> triangleVertices[3] = { Vector<Float, 3>(0, 0, 0), Vector<Float, 3>(1, 0, 0), Vector<Float, 3>(0, 1, 0) };
                    Vector<int, 3> triangleFaces[1] = { Vector<int, 3>(0, 1, 2) };

                    triangle = Mesh::makeTri(allocator.allocate(triangleVertices, 3), nullptr, nullptr, allocator.allocate(triangleFaces, 1));

                    Vector<Float, 3> quadVertices[4] = { Vector<Float, 3>(0, 0, 0), Vector<Float, 3>(1, 0, 0), Vector<Float, 3>(1, 1, 0), Vector<Float, 3>(0, 1, 0) };
                    Vector<int, 4> quadFaces[1] = { Vector<int, 4>(0, 1, 2, 3) };

                    quad = Mesh::makeQuad(allocator.allocate(quadVertices, 4), nullptr, nullptr, allocator.allocate(quadFaces, 1));
                }

                Ray makeRay(const Vector<Float, 3> & origin, const Vector<Float, 3> & direction) { return Ray(origin, direction, SampledSpectrum<Float>(550), nullptr); }

                void expectSamplingConsistency(const Object & object, const Vector<Float, 3> & point) {
                    Float sum = 0;
                    Float sumSquared = 0;

                    for (int i = 0; i < 50000; i++) {
                        Float value = object.probabilityDensity(point, utils::randomUnitVector(state)) * 4 * constants::PI;

                        sum += value;
                        sumSquared += value * value;
                    }

                    Float mean = sum / 50000;
                    Float variance = std::fmax(sumSquared / 50000 - mean * mean, Float(0));

                    EXPECT_NEAR(mean, 1, 5 * std::sqrt(variance / 50000) + tolerances::COARSE);

                    int hits = 0;
                    int consistent = 0;
                    int samples = 0;

                    for (int i = 0; i < 2000; i++) {
                        Float probabilityDensity = 0;

                        Vector<Float, 3> direction = object.sample(point, probabilityDensity, state);

                        if (!(probabilityDensity > 0)) continue;

                        samples++;

                        EXPECT_NEAR(direction.length(), 1, tolerances::ACCUMULATED);

                        if (std::fabs(object.probabilityDensity(point, direction) / probabilityDensity - 1) <= Float(0.02)) consistent++;

                        Intersection hit;

                        if (object.intersect(hit, makeRay(point, direction))) hits++;
                    }

                    EXPECT_GT(samples, 1000);
                    EXPECT_GE(hits, samples * 99 / 100);
                    EXPECT_GE(consistent, samples * 98 / 100);
                }

                void expectBoundsContainSurface(const Object & object, const Vector<Float, 3> & target) {
                    Vector<Float, 3> padding(Float(1e-3));

                    for (int i = 0; i < 2000; i++) {
                        Vector<Float, 3> origin = target + 10 * utils::randomUnitVector(state);

                        Intersection hit;

                        if (!object.intersect(hit, makeRay(origin, target + utils::randomUnitVector(state) - origin))) continue;

                        for (int j = 0; j < 3; j++) {
                            EXPECT_GE(hit.point[j], object.getMin()[j] - padding[j]);
                            EXPECT_LE(hit.point[j], object.getMax()[j] + padding[j]);
                        }
                    }
                }
        };

        TEST_F(ObjectTest, SphereGeometry) {
            Object sphere = Object::makeSphere(nullptr, nullptr, nullptr, 2);

            expectEqual(sphere.getMin(), Vector<Float, 3>(-2, -2, -2));
            expectEqual(sphere.getMax(), Vector<Float, 3>(2, 2, 2));
            expectNear(sphere.center(), Vector<Float, 3>());

            EXPECT_NEAR(sphere.radius(), 2, tolerances::STANDARD);
            EXPECT_NEAR(sphere.getArea(), 16 * constants::PI, tolerances::COARSE);
        }

        TEST_F(ObjectTest, StoresMaterialAndMedia) {
            Material material = Material::makeDiffuse(makeSpectrumTexture(Float(0.5)), nullptr);
            Medium inside = Medium::makeHomogeneous(makeSpectrum(0), makeSpectrum(1), 0, nullptr);
            Medium outside = Medium::makeHomogeneous(makeSpectrum(1), makeSpectrum(0), 0, nullptr);

            Object object = Object::makeSphere(&material, &inside, &outside, 1);

            EXPECT_EQ(object.getMaterial(), &material);
            EXPECT_EQ(object.getMedium0(), &inside);
            EXPECT_EQ(object.getMedium1(), &outside);

            Object bare = Object::makeSphere(nullptr, nullptr, nullptr, 1);

            EXPECT_EQ(bare.getMaterial(), nullptr);
            EXPECT_EQ(bare.getMedium0(), nullptr);
            EXPECT_EQ(bare.getMedium1(), nullptr);
        }

        TEST_F(ObjectTest, SphereIntersectionFromOutside) {
            Object sphere = Object::makeSphere(nullptr, nullptr, nullptr, 2);

            Intersection hit;

            ASSERT_TRUE(sphere.intersect(hit, makeRay(Vector<Float, 3>(0, 0, -5), Vector<Float, 3>(0, 0, 1))));

            EXPECT_NEAR(hit.t, 3, tolerances::ACCUMULATED);
            EXPECT_EQ(hit.object, &sphere);
            EXPECT_TRUE(hit.isSurface);

            expectNear(hit.point, Vector<Float, 3>(0, 0, -2), tolerances::ACCUMULATED);
            expectNear(hit.normal, Vector<Float, 3>(0, 0, -1), tolerances::ACCUMULATED);
            expectNear(hit.geometricNormal, Vector<Float, 3>(0, 0, -1), tolerances::ACCUMULATED);

            EXPECT_NEAR(hit.textureCoordinate[0], Float(0.25), tolerances::ACCUMULATED);
            EXPECT_NEAR(hit.textureCoordinate[1], Float(0.5), tolerances::ACCUMULATED);
        }

        TEST_F(ObjectTest, SphereIntersectionFromInside) {
            Object sphere = Object::makeSphere(nullptr, nullptr, nullptr, 2);

            Intersection hit;

            ASSERT_TRUE(sphere.intersect(hit, makeRay(Vector<Float, 3>(), Vector<Float, 3>(0, 0, 1))));

            EXPECT_NEAR(hit.t, 2, tolerances::ACCUMULATED);

            expectNear(hit.normal, Vector<Float, 3>(0, 0, 1), tolerances::ACCUMULATED);
        }

        TEST_F(ObjectTest, SphereIntersectionScalesWithDirectionLength) {
            Object sphere = Object::makeSphere(nullptr, nullptr, nullptr, 2);

            Intersection hit;

            ASSERT_TRUE(sphere.intersect(hit, makeRay(Vector<Float, 3>(0, 0, -5), Vector<Float, 3>(0, 0, 2))));

            EXPECT_NEAR(hit.t, Float(1.5), tolerances::ACCUMULATED);

            expectNear(hit.point, Vector<Float, 3>(0, 0, -2), tolerances::ACCUMULATED);
        }

        TEST_F(ObjectTest, SphereMissesRaysThatPassAwayFromIt) {
            Object sphere = Object::makeSphere(nullptr, nullptr, nullptr, 2);

            Intersection hit;

            EXPECT_FALSE(sphere.intersect(hit, makeRay(Vector<Float, 3>(3, 0, -5), Vector<Float, 3>(0, 0, 1))));
            EXPECT_FALSE(sphere.intersect(hit, makeRay(Vector<Float, 3>(0, 0, -5), Vector<Float, 3>(0, 0, -1))));
            EXPECT_FALSE(sphere.intersect(hit, makeRay(Vector<Float, 3>(0, 0, 5), Vector<Float, 3>(0, 0, 1))));

            EXPECT_EQ(hit.object, nullptr);
            EXPECT_EQ(hit.t, constants::MAX);
        }

        TEST_F(ObjectTest, SphereRejectsHitsBeyondCurrentClosest) {
            Object sphere = Object::makeSphere(nullptr, nullptr, nullptr, 2);

            Intersection hit;

            hit.t = 2;

            EXPECT_FALSE(sphere.intersect(hit, makeRay(Vector<Float, 3>(0, 0, -5), Vector<Float, 3>(0, 0, 1))));
            EXPECT_EQ(hit.t, 2);
            EXPECT_EQ(hit.object, nullptr);

            hit.t = 4;

            EXPECT_TRUE(sphere.intersect(hit, makeRay(Vector<Float, 3>(0, 0, -5), Vector<Float, 3>(0, 0, 1))));
            EXPECT_NEAR(hit.t, 3, tolerances::ACCUMULATED);
        }

        TEST_F(ObjectTest, SphereTangentIsUnitAndPerpendicularToNormal) {
            Object sphere = Object::makeSphere(nullptr, nullptr, nullptr, 2);

            for (int i = 0; i < 100; i++) {
                Vector<Float, 3> origin = 5 * utils::randomUnitVector(state);

                Intersection hit;

                ASSERT_TRUE(sphere.intersect(hit, makeRay(origin, -origin)));

                EXPECT_NEAR(hit.tangent.length(), 1, tolerances::ACCUMULATED);
                EXPECT_NEAR(dot(hit.tangent, hit.normal), 0, tolerances::ACCUMULATED);
            }
        }

        TEST_F(ObjectTest, SphereTangentAtPoleIsDefined) {
            Object sphere = Object::makeSphere(nullptr, nullptr, nullptr, 2);

            Intersection hit;

            ASSERT_TRUE(sphere.intersect(hit, makeRay(Vector<Float, 3>(0, 5, 0), Vector<Float, 3>(0, -1, 0))));

            expectNear(hit.tangent, Vector<Float, 3>(1, 0, 0));
        }

        TEST_F(ObjectTest, SphereBoundsContainSurface) {
            Object sphere = Object::makeSphere(nullptr, nullptr, nullptr, 2);

            expectBoundsContainSurface(sphere, Vector<Float, 3>());
        }

        TEST_F(ObjectTest, SphereSamplingFromOutsideIsConsistent) {
            expectSamplingConsistency(Object::makeSphere(nullptr, nullptr, nullptr, 1), Vector<Float, 3>(0, 0, 3));
        }

        TEST_F(ObjectTest, SphereSamplingFromInsideIsConsistent) {
            expectSamplingConsistency(Object::makeSphere(nullptr, nullptr, nullptr, 1), Vector<Float, 3>(Float(0.2), 0, 0));
        }

        TEST_F(ObjectTest, SphereDensityIsZeroOutsideSubtendedCone) {
            Object sphere = Object::makeSphere(nullptr, nullptr, nullptr, 1);

            EXPECT_EQ(sphere.probabilityDensity(Vector<Float, 3>(0, 0, 3), Vector<Float, 3>(0, 1, 0)), 0);
            EXPECT_GT(sphere.probabilityDensity(Vector<Float, 3>(0, 0, 3), Vector<Float, 3>(0, 0, -1)), 0);
        }

        TEST_F(ObjectTest, DiskGeometry) {
            Object disk = Object::makeDisk(nullptr, nullptr, nullptr, 2);

            EXPECT_NEAR(disk.getArea(), 4 * constants::PI, tolerances::COARSE);
            EXPECT_NEAR(disk.radius(), 2, tolerances::ACCUMULATED);

            expectNear(disk.center(), Vector<Float, 3>());
        }

        TEST_F(ObjectTest, DiskIntersectionIsTwoSided) {
            Object disk = Object::makeDisk(nullptr, nullptr, nullptr, 2);

            Intersection above;
            Intersection below;

            ASSERT_TRUE(disk.intersect(above, makeRay(Vector<Float, 3>(Float(0.5), 5, 0), Vector<Float, 3>(0, -1, 0))));
            ASSERT_TRUE(disk.intersect(below, makeRay(Vector<Float, 3>(Float(0.5), -5, 0), Vector<Float, 3>(0, 1, 0))));

            EXPECT_NEAR(above.t, 5, tolerances::ACCUMULATED);
            EXPECT_NEAR(below.t, 5, tolerances::ACCUMULATED);

            expectNear(above.point, Vector<Float, 3>(Float(0.5), 0, 0), tolerances::ACCUMULATED);
            expectNear(above.normal, Vector<Float, 3>(0, 1, 0));
            expectNear(below.normal, Vector<Float, 3>(0, 1, 0));
            expectNear(above.geometricNormal, Vector<Float, 3>(0, 1, 0));
        }

        TEST_F(ObjectTest, DiskTextureCoordinatesAreRadialAndAngular) {
            Object disk = Object::makeDisk(nullptr, nullptr, nullptr, 2);

            Intersection hit;

            ASSERT_TRUE(disk.intersect(hit, makeRay(Vector<Float, 3>(1, 5, 0), Vector<Float, 3>(0, -1, 0))));

            EXPECT_NEAR(hit.textureCoordinate[0], Float(0.5), tolerances::ACCUMULATED);
            EXPECT_NEAR(hit.textureCoordinate[1], Float(0.5), tolerances::ACCUMULATED);
        }

        TEST_F(ObjectTest, DiskMissesOutsideRadiusAndParallelRays) {
            Object disk = Object::makeDisk(nullptr, nullptr, nullptr, 2);

            Intersection hit;

            EXPECT_FALSE(disk.intersect(hit, makeRay(Vector<Float, 3>(3, 5, 0), Vector<Float, 3>(0, -1, 0))));
            EXPECT_FALSE(disk.intersect(hit, makeRay(Vector<Float, 3>(-5, 0, 0), Vector<Float, 3>(1, 0, 0))));
            EXPECT_FALSE(disk.intersect(hit, makeRay(Vector<Float, 3>(0, 5, 0), Vector<Float, 3>(0, 1, 0))));
        }

        TEST_F(ObjectTest, DiskRejectsHitsBeyondCurrentClosest) {
            Object disk = Object::makeDisk(nullptr, nullptr, nullptr, 2);

            Intersection hit;

            hit.t = 4;

            EXPECT_FALSE(disk.intersect(hit, makeRay(Vector<Float, 3>(0, 5, 0), Vector<Float, 3>(0, -1, 0))));
        }

        TEST_F(ObjectTest, DiskBoundsContainSurface) {
            Object disk = Object::makeDisk(nullptr, nullptr, nullptr, 2);

            expectBoundsContainSurface(disk, Vector<Float, 3>());
        }

        TEST_F(ObjectTest, DiskSamplingIsConsistent) {
            expectSamplingConsistency(Object::makeDisk(nullptr, nullptr, nullptr, 1), Vector<Float, 3>(Float(0.2), Float(0.5), Float(0.1)));
        }

        TEST_F(ObjectTest, CylinderGeometry) {
            Object cylinder = Object::makeCylinder(nullptr, nullptr, nullptr, 1, 2);

            EXPECT_NEAR(cylinder.getArea(), 4 * constants::PI, tolerances::COARSE);

            expectNear(cylinder.center(), Vector<Float, 3>());
        }

        TEST_F(ObjectTest, CylinderIntersectionOnSideWall) {
            Object cylinder = Object::makeCylinder(nullptr, nullptr, nullptr, 1, 2);

            Intersection hit;

            ASSERT_TRUE(cylinder.intersect(hit, makeRay(Vector<Float, 3>(-5, 0, 0), Vector<Float, 3>(1, 0, 0))));

            EXPECT_NEAR(hit.t, 4, tolerances::ACCUMULATED);

            expectNear(hit.point, Vector<Float, 3>(-1, 0, 0), tolerances::ACCUMULATED);
            expectNear(hit.normal, Vector<Float, 3>(-1, 0, 0), tolerances::ACCUMULATED);
            expectNear(hit.tangent, Vector<Float, 3>(0, 0, -1), tolerances::ACCUMULATED);

            EXPECT_NEAR(hit.textureCoordinate[0], 1, tolerances::ACCUMULATED);
            EXPECT_NEAR(hit.textureCoordinate[1], Float(0.5), tolerances::ACCUMULATED);
        }

        TEST_F(ObjectTest, CylinderHeightMapsToTextureCoordinate) {
            Object cylinder = Object::makeCylinder(nullptr, nullptr, nullptr, 1, 2);

            Intersection hit;

            ASSERT_TRUE(cylinder.intersect(hit, makeRay(Vector<Float, 3>(-5, Float(0.5), 0), Vector<Float, 3>(1, 0, 0))));

            EXPECT_NEAR(hit.textureCoordinate[1], Float(0.75), tolerances::ACCUMULATED);
        }

        TEST_F(ObjectTest, CylinderIsOpenAndFinite) {
            Object cylinder = Object::makeCylinder(nullptr, nullptr, nullptr, 1, 2);

            Intersection hit;

            EXPECT_FALSE(cylinder.intersect(hit, makeRay(Vector<Float, 3>(-5, 2, 0), Vector<Float, 3>(1, 0, 0))));
            EXPECT_FALSE(cylinder.intersect(hit, makeRay(Vector<Float, 3>(0, 5, 0), Vector<Float, 3>(0, -1, 0))));
            EXPECT_FALSE(cylinder.intersect(hit, makeRay(Vector<Float, 3>(-5, 0, 3), Vector<Float, 3>(1, 0, 0))));
        }

        TEST_F(ObjectTest, CylinderIntersectionFromInside) {
            Object cylinder = Object::makeCylinder(nullptr, nullptr, nullptr, 1, 2);

            Intersection hit;

            ASSERT_TRUE(cylinder.intersect(hit, makeRay(Vector<Float, 3>(), Vector<Float, 3>(1, 0, 0))));

            EXPECT_NEAR(hit.t, 1, tolerances::ACCUMULATED);
        }

        TEST_F(ObjectTest, CylinderBoundsContainSurface) {
            Object cylinder = Object::makeCylinder(nullptr, nullptr, nullptr, 1, 2);

            expectBoundsContainSurface(cylinder, Vector<Float, 3>());
        }

        TEST_F(ObjectTest, CylinderSamplingIsConsistent) {
            expectSamplingConsistency(Object::makeCylinder(nullptr, nullptr, nullptr, 1, 2), Vector<Float, 3>(3, Float(0.3), 0));
        }

        TEST_F(ObjectTest, TriangleGeometry) {
            Object tri = Object::makeTri(nullptr, nullptr, nullptr, &triangle, 0);

            EXPECT_NEAR(tri.getArea(), Float(0.5), tolerances::STANDARD);
            EXPECT_NEAR(tri.radius(), std::sqrt(Float(5)) / 3, tolerances::ACCUMULATED);

            expectNear(tri.center(), Vector<Float, 3>(Float(1) / 3, Float(1) / 3, 0), tolerances::ACCUMULATED);

            for (int i = 0; i < 3; i++) {
                EXPECT_LE(tri.getMin()[i], 0);
                EXPECT_GE(tri.getMax()[i], i == 2 ? 0 : 1);
            }
        }

        TEST_F(ObjectTest, TriangleIntersectionIsTwoSidedWithFlatNormal) {
            Object tri = Object::makeTri(nullptr, nullptr, nullptr, &triangle, 0);

            Intersection front;
            Intersection back;

            ASSERT_TRUE(tri.intersect(front, makeRay(Vector<Float, 3>(Float(0.25), Float(0.25), 5), Vector<Float, 3>(0, 0, -1))));
            ASSERT_TRUE(tri.intersect(back, makeRay(Vector<Float, 3>(Float(0.25), Float(0.25), -5), Vector<Float, 3>(0, 0, 1))));

            EXPECT_NEAR(front.t, 5, tolerances::ACCUMULATED);
            EXPECT_NEAR(back.t, 5, tolerances::ACCUMULATED);

            expectNear(front.point, Vector<Float, 3>(Float(0.25), Float(0.25), 0), tolerances::ACCUMULATED);
            expectNear(front.normal, Vector<Float, 3>(0, 0, 1), tolerances::ACCUMULATED);
            expectNear(back.normal, Vector<Float, 3>(0, 0, 1), tolerances::ACCUMULATED);
            expectNear(front.geometricNormal, Vector<Float, 3>(0, 0, 1), tolerances::ACCUMULATED);
        }

        TEST_F(ObjectTest, TriangleDefaultsToBarycentricTextureCoordinates) {
            Object tri = Object::makeTri(nullptr, nullptr, nullptr, &triangle, 0);

            Intersection hit;

            ASSERT_TRUE(tri.intersect(hit, makeRay(Vector<Float, 3>(Float(0.25), Float(0.5), 5), Vector<Float, 3>(0, 0, -1))));

            expectNear(hit.textureCoordinate, Vector<Float, 2>(Float(0.25), Float(0.5)), tolerances::ACCUMULATED);
            expectNear(hit.tangent, Vector<Float, 3>(1, 0, 0), tolerances::ACCUMULATED);
        }

        TEST_F(ObjectTest, TriangleMissesOutsideEdges) {
            Object tri = Object::makeTri(nullptr, nullptr, nullptr, &triangle, 0);

            Intersection hit;

            EXPECT_FALSE(tri.intersect(hit, makeRay(Vector<Float, 3>(Float(0.8), Float(0.8), 5), Vector<Float, 3>(0, 0, -1))));
            EXPECT_FALSE(tri.intersect(hit, makeRay(Vector<Float, 3>(Float(-0.1), Float(0.2), 5), Vector<Float, 3>(0, 0, -1))));
            EXPECT_FALSE(tri.intersect(hit, makeRay(Vector<Float, 3>(Float(0.2), Float(0.2), 5), Vector<Float, 3>(1, 0, 0))));
            EXPECT_FALSE(tri.intersect(hit, makeRay(Vector<Float, 3>(Float(0.2), Float(0.2), 5), Vector<Float, 3>(0, 0, 1))));
        }

        TEST_F(ObjectTest, TriangleRejectsHitsBeyondCurrentClosest) {
            Object tri = Object::makeTri(nullptr, nullptr, nullptr, &triangle, 0);

            Intersection hit;

            hit.t = 4;

            EXPECT_FALSE(tri.intersect(hit, makeRay(Vector<Float, 3>(Float(0.25), Float(0.25), 5), Vector<Float, 3>(0, 0, -1))));
        }

        TEST_F(ObjectTest, TriangleInterpolatesVertexNormals) {
            Vector<Float, 3> vertices[3] = { Vector<Float, 3>(0, 0, 0), Vector<Float, 3>(1, 0, 0), Vector<Float, 3>(0, 1, 0) };
            Vector<Float, 3> normals[3] = { Vector<Float, 3>(0, 0, 1), Vector<Float, 3>(0, 0, 1), Vector<Float, 3>(0, 1, 0) };
            Vector<int, 3> faces[1] = { Vector<int, 3>(0, 1, 2) };

            Mesh mesh = Mesh::makeTri(vertices, normals, nullptr, faces);

            Object tri = Object::makeTri(nullptr, nullptr, nullptr, &mesh, 0);

            Intersection hit;

            ASSERT_TRUE(tri.intersect(hit, makeRay(Vector<Float, 3>(0, Float(0.5), 5), Vector<Float, 3>(0, 0, -1))));

            expectNear(hit.normal, normalize(Vector<Float, 3>(0, Float(0.5), Float(0.5))), tolerances::ACCUMULATED);
            expectNear(hit.geometricNormal, Vector<Float, 3>(0, 0, 1), tolerances::ACCUMULATED);

            EXPECT_NEAR(hit.normal.length(), 1, tolerances::ACCUMULATED);
            EXPECT_NEAR(dot(hit.tangent, hit.normal), 0, tolerances::ACCUMULATED);
        }

        TEST_F(ObjectTest, TriangleUsesProvidedTextureCoordinates) {
            Vector<Float, 3> vertices[3] = { Vector<Float, 3>(0, 0, 0), Vector<Float, 3>(1, 0, 0), Vector<Float, 3>(0, 1, 0) };
            Vector<Float, 2> coordinates[3] = { Vector<Float, 2>(0, 0), Vector<Float, 2>(0, 1), Vector<Float, 2>(1, 0) };
            Vector<int, 3> faces[1] = { Vector<int, 3>(0, 1, 2) };

            Mesh mesh = Mesh::makeTri(vertices, nullptr, coordinates, faces);

            Object tri = Object::makeTri(nullptr, nullptr, nullptr, &mesh, 0);

            Intersection hit;

            ASSERT_TRUE(tri.intersect(hit, makeRay(Vector<Float, 3>(Float(0.25), Float(0.5), 5), Vector<Float, 3>(0, 0, -1))));

            expectNear(hit.textureCoordinate, Vector<Float, 2>(Float(0.5), Float(0.25)), tolerances::ACCUMULATED);
            expectNear(hit.tangent, Vector<Float, 3>(0, 1, 0), tolerances::ACCUMULATED);
        }

        TEST_F(ObjectTest, TriangleBoundsContainSurface) {
            Object tri = Object::makeTri(nullptr, nullptr, nullptr, &triangle, 0);

            expectBoundsContainSurface(tri, Vector<Float, 3>(Float(0.3), Float(0.3), 0));
        }

        TEST_F(ObjectTest, TriangleSamplingIsConsistent) {
            expectSamplingConsistency(Object::makeTri(nullptr, nullptr, nullptr, &triangle, 0), Vector<Float, 3>(Float(0.3), Float(0.3), Float(0.5)));
        }

        TEST_F(ObjectTest, PatchGeometry) {
            Object patch = Object::makePatch(nullptr, nullptr, nullptr, &quad, 0);

            EXPECT_NEAR(patch.getArea(), 1, tolerances::STANDARD);

            expectNear(patch.center(), Vector<Float, 3>(Float(0.5), Float(0.5), 0), tolerances::ACCUMULATED);
            EXPECT_NEAR(patch.radius(), std::sqrt(Float(0.5)), tolerances::ACCUMULATED);
        }

        TEST_F(ObjectTest, PatchAreaOfNonParallelogramIsSubdivided) {
            Vector<Float, 3> vertices[4] = { Vector<Float, 3>(0, 0, 0), Vector<Float, 3>(2, 0, 0), Vector<Float, 3>(1, 1, 0), Vector<Float, 3>(0, 1, 0) };
            Vector<int, 4> faces[1] = { Vector<int, 4>(0, 1, 2, 3) };

            Mesh mesh = Mesh::makeQuad(vertices, nullptr, nullptr, faces);

            Object patch = Object::makePatch(nullptr, nullptr, nullptr, &mesh, 0);

            EXPECT_NEAR(patch.getArea(), Float(1.5), tolerances::STATISTICAL);
        }

        TEST_F(ObjectTest, PatchNormalFollowsWindingLikeTriangle) {
            Object patch = Object::makePatch(nullptr, nullptr, nullptr, &quad, 0);
            Object tri = Object::makeTri(nullptr, nullptr, nullptr, &triangle, 0);

            Ray r = makeRay(Vector<Float, 3>(Float(0.25), Float(0.25), 5), Vector<Float, 3>(0, 0, -1));

            Intersection patchHit;
            Intersection triangleHit;

            ASSERT_TRUE(patch.intersect(patchHit, r));
            ASSERT_TRUE(tri.intersect(triangleHit, r));

            expectNear(patchHit.geometricNormal, triangleHit.geometricNormal, tolerances::ACCUMULATED);
        }

        TEST_F(ObjectTest, PatchIntersectionIsTwoSided) {
            Object patch = Object::makePatch(nullptr, nullptr, nullptr, &quad, 0);

            Intersection front;
            Intersection back;

            ASSERT_TRUE(patch.intersect(front, makeRay(Vector<Float, 3>(Float(0.75), Float(0.25), 5), Vector<Float, 3>(0, 0, -1))));
            ASSERT_TRUE(patch.intersect(back, makeRay(Vector<Float, 3>(Float(0.75), Float(0.25), -5), Vector<Float, 3>(0, 0, 1))));

            EXPECT_NEAR(front.t, 5, tolerances::ACCUMULATED);

            expectNear(front.point, Vector<Float, 3>(Float(0.75), Float(0.25), 0), tolerances::ACCUMULATED);
            expectNear(front.normal, Vector<Float, 3>(0, 0, 1), tolerances::ACCUMULATED);
            expectNear(back.normal, Vector<Float, 3>(0, 0, 1), tolerances::ACCUMULATED);
            expectNear(front.geometricNormal, Vector<Float, 3>(0, 0, 1), tolerances::ACCUMULATED);
        }

        TEST_F(ObjectTest, PatchMissesOutsideCorners) {
            Object patch = Object::makePatch(nullptr, nullptr, nullptr, &quad, 0);

            Intersection hit;

            EXPECT_FALSE(patch.intersect(hit, makeRay(Vector<Float, 3>(Float(1.5), Float(0.5), 5), Vector<Float, 3>(0, 0, -1))));
            EXPECT_FALSE(patch.intersect(hit, makeRay(Vector<Float, 3>(Float(0.5), Float(-0.5), 5), Vector<Float, 3>(0, 0, -1))));
        }

        TEST_F(ObjectTest, PatchBoundsContainSurface) {
            Object patch = Object::makePatch(nullptr, nullptr, nullptr, &quad, 0);

            expectBoundsContainSurface(patch, Vector<Float, 3>(Float(0.5), Float(0.5), 0));
        }

        TEST_F(ObjectTest, PatchSamplingIsConsistent) {
            expectSamplingConsistency(Object::makePatch(nullptr, nullptr, nullptr, &quad, 0), Vector<Float, 3>(Float(0.4), Float(0.6), Float(0.5)));
        }

        class InstanceTest : public WorldTest {
            protected:
                Mesh mesh;
                Material * diffuse;
                Material * emissive;

                void SetUp() override {
                    Vector<Float, 3> vertices[6] = { Vector<Float, 3>(0, 0, 0), Vector<Float, 3>(1, 0, 0), Vector<Float, 3>(0, 1, 0), Vector<Float, 3>(0, 0, 1), Vector<Float, 3>(1, 0, 1), Vector<Float, 3>(0, 1, 1) };
                    Vector<int, 3> faces[2] = { Vector<int, 3>(0, 1, 2), Vector<int, 3>(3, 4, 5) };

                    mesh = Mesh::makeTri(allocator.allocate(vertices, 6), nullptr, nullptr, allocator.allocate(faces, 2));

                    diffuse = allocator.make(Material::makeDiffuse(makeSpectrumTexture(Float(0.5)), nullptr));
                    emissive = allocator.make(Material::makeEmissive(makeSpectrumTexture(3)));
                }

                Object * makeSpheres(int count, Material * material, Float radius = 1) {
                    Object * objects = allocator.allocate<Object>(count);

                    for (int i = 0; i < count; i++) objects[i] = Object::makeSphere(material, nullptr, nullptr, radius);

                    return objects;
                }

                Object * makeTriangles(Material * material) {
                    Object * objects = allocator.allocate<Object>(2);

                    for (int i = 0; i < 2; i++) objects[i] = Object::makeTri(material, nullptr, nullptr, &mesh, i);

                    return objects;
                }
        };

        TEST_F(InstanceTest, BoundsFollowTransform) {
            Instance * instance = makeInstance(makeSpheres(1, diffuse), 1, nullptr, Vector<Float, 3>(5, 0, 0), Vector<Float, 3>(), Vector<Float, 3>(2, 2, 2));

            expectNear(instance->getMin(), Vector<Float, 3>(3, -2, -2), tolerances::COARSE);
            expectNear(instance->getMax(), Vector<Float, 3>(7, 2, 2), tolerances::COARSE);
            expectNear(instance->center(), Vector<Float, 3>(5, 0, 0), tolerances::COARSE);
        }

        TEST_F(InstanceTest, ObjectCenterAndRadiusFollowTransform) {
            Instance * instance = makeInstance(makeSpheres(2, diffuse), 2, nullptr, Vector<Float, 3>(5, 1, 0), Vector<Float, 3>(), Vector<Float, 3>(2, 2, 2));

            EXPECT_EQ(instance->getCount(), 2);

            for (int i = 0; i < 2; i++) {
                expectNear(instance->center(i), Vector<Float, 3>(5, 1, 0), tolerances::ACCUMULATED);

                EXPECT_NEAR(instance->radius(i), 2, tolerances::ACCUMULATED);
            }
        }

        TEST_F(InstanceTest, RadiusUsesLargestScale) {
            Instance * instance = makeInstance(makeSpheres(1, diffuse), 1, nullptr, Vector<Float, 3>(), Vector<Float, 3>(), Vector<Float, 3>(1, 3, 2));

            EXPECT_NEAR(instance->radius(0), 3, tolerances::ACCUMULATED);
        }

        TEST_F(InstanceTest, MaterialOverridesObjectMaterial) {
            Object * objects = makeSpheres(1, diffuse);

            Instance * inherited = makeInstance(objects, 1);
            Instance * overridden = makeInstance(objects, 1, emissive);

            EXPECT_EQ(inherited->getMaterial(0), diffuse);
            EXPECT_EQ(overridden->getMaterial(0), emissive);
        }

        TEST_F(InstanceTest, MediaOverrideObjectMedia) {
            Medium inside = Medium::makeHomogeneous(makeSpectrum(0), makeSpectrum(1), 0, nullptr);
            Medium outside = Medium::makeHomogeneous(makeSpectrum(1), makeSpectrum(0), 0, nullptr);
            Medium replacement = Medium::makeHomogeneous(makeSpectrum(1), makeSpectrum(1), 0, nullptr);

            Object * objects = allocator.allocate<Object>(1);

            objects[0] = Object::makeSphere(diffuse, &inside, &outside, 1);

            Instance inherited(objects, 1, nullptr, nullptr, nullptr, nullptr, nullptr, Vector<Float, 3>(), Vector<Float, 3>(), Vector<Float, 3>(1, 1, 1));
            Instance overridden(objects, 1, nullptr, nullptr, nullptr, &replacement, &replacement, Vector<Float, 3>(), Vector<Float, 3>(), Vector<Float, 3>(1, 1, 1));

            EXPECT_EQ(inherited.getMedium0(0), &inside);
            EXPECT_EQ(inherited.getMedium1(0), &outside);
            EXPECT_EQ(overridden.getMedium0(0), &replacement);
            EXPECT_EQ(overridden.getMedium1(0), &replacement);
        }

        TEST_F(InstanceTest, ObjectPowerScalesWithAreaEmissionAndTransform) {
            Instance * instance = makeInstance(makeSpheres(1, emissive), 1, nullptr, Vector<Float, 3>(), Vector<Float, 3>(), Vector<Float, 3>(2, 2, 2));

            EXPECT_NEAR(instance->getPower(0), 4 * constants::PI * 4 * 3, tolerances::COARSE);
            EXPECT_NEAR(instance->getPower(), 4 * constants::PI * 4 * 3, tolerances::COARSE);
        }

        TEST_F(InstanceTest, OverridingMaterialDefinesPower) {
            Instance * instance = makeInstance(makeSpheres(1, nullptr), 1, emissive, Vector<Float, 3>(), Vector<Float, 3>(), Vector<Float, 3>(2, 2, 2));

            EXPECT_NEAR(instance->getPower(0), 4 * constants::PI * 4 * 3, tolerances::COARSE);
            EXPECT_NEAR(instance->getPower(), 4 * constants::PI * 4 * 3, tolerances::COARSE);
        }

        TEST_F(InstanceTest, ObjectWithoutMaterialHasNoPower) {
            Instance * instance = makeInstance(makeSpheres(1, nullptr), 1);

            EXPECT_EQ(instance->getPower(0), 0);
        }

        TEST_F(InstanceTest, IntersectionFromOutsideFacesRay) {
            Instance * instance = makeInstance(makeSpheres(1, diffuse), 1, nullptr, Vector<Float, 3>(), Vector<Float, 3>(), Vector<Float, 3>(2, 2, 2));

            Ray r = makeRay(Vector<Float, 3>(0, 0, -10), Vector<Float, 3>(0, 0, 1));

            Intersection hit;

            ASSERT_TRUE(instance->intersect(0, hit, r));

            EXPECT_NEAR(hit.t, 8, tolerances::ACCUMULATED);
            EXPECT_EQ(hit.instance, instance);
            EXPECT_EQ(hit.i, 0);
            EXPECT_TRUE(hit.frontFacing);
            EXPECT_FALSE(hit.useTransformed);

            expectNear(hit.transformedPoint, Vector<Float, 3>(0, 0, -2), tolerances::ACCUMULATED);
            expectNear(hit.transformedNormal, Vector<Float, 3>(0, 0, -1), tolerances::ACCUMULATED);
            expectNear(hit.geometricNormal, Vector<Float, 3>(0, 0, -1), tolerances::ACCUMULATED);

            EXPECT_NEAR(hit.transformedTangent.length(), 1, tolerances::ACCUMULATED);
        }

        TEST_F(InstanceTest, IntersectionFromInsideFlipsNormalsTowardRay) {
            Instance * instance = makeInstance(makeSpheres(1, diffuse), 1);

            Ray r = makeRay(Vector<Float, 3>(), Vector<Float, 3>(0, 0, 1));

            Intersection hit;

            ASSERT_TRUE(instance->intersect(0, hit, r));

            EXPECT_FALSE(hit.frontFacing);

            expectNear(hit.transformedNormal, Vector<Float, 3>(0, 0, -1), tolerances::ACCUMULATED);
            expectNear(hit.geometricNormal, Vector<Float, 3>(0, 0, -1), tolerances::ACCUMULATED);
        }

        TEST_F(InstanceTest, IntersectionUsesTransformedPointWhenMaterialIsOverridden) {
            Instance * instance = makeInstance(makeSpheres(1, nullptr), 1, diffuse);

            Intersection hit;

            ASSERT_TRUE(instance->intersect(0, hit, makeRay(Vector<Float, 3>(0, 0, -5), Vector<Float, 3>(0, 0, 1))));

            EXPECT_TRUE(hit.useTransformed);
        }

        TEST_F(InstanceTest, IntersectionMissesAndLeavesStateUntouched) {
            Instance * instance = makeInstance(makeSpheres(1, diffuse), 1);

            Intersection hit;

            EXPECT_FALSE(instance->intersect(0, hit, makeRay(Vector<Float, 3>(5, 0, -5), Vector<Float, 3>(0, 0, 1))));

            EXPECT_EQ(hit.instance, nullptr);
            EXPECT_EQ(hit.t, constants::MAX);
        }

        TEST_F(InstanceTest, HitFindsNearestObject) {
            Instance * instance = makeInstance(makeTriangles(diffuse), 2);

            Intersection hit;

            ASSERT_TRUE(instance->hit(hit, makeRay(Vector<Float, 3>(Float(0.25), Float(0.25), 5), Vector<Float, 3>(0, 0, -1))));

            EXPECT_NEAR(hit.t, 4, tolerances::ACCUMULATED);
            EXPECT_EQ(hit.instance, instance);
            EXPECT_EQ(hit.i, 1);

            ASSERT_TRUE(instance->hit(hit = Intersection(), makeRay(Vector<Float, 3>(Float(0.25), Float(0.25), -5), Vector<Float, 3>(0, 0, 1))));

            EXPECT_NEAR(hit.t, 5, tolerances::ACCUMULATED);
            EXPECT_EQ(hit.i, 0);
        }

        TEST_F(InstanceTest, HitMissesAndRespectsCurrentClosest) {
            Instance * instance = makeInstance(makeTriangles(diffuse), 2);

            Ray r = makeRay(Vector<Float, 3>(Float(0.25), Float(0.25), 5), Vector<Float, 3>(0, 0, -1));

            Intersection miss;

            EXPECT_FALSE(instance->hit(miss, makeRay(Vector<Float, 3>(Float(0.9), Float(0.9), 5), Vector<Float, 3>(0, 0, -1))));
            EXPECT_EQ(miss.instance, nullptr);

            Intersection closer;

            closer.t = 3;

            EXPECT_FALSE(instance->hit(closer, r));
            EXPECT_EQ(closer.t, 3);
        }

        TEST_F(InstanceTest, HitAppliesTransform) {
            Instance * instance = makeInstance(makeTriangles(diffuse), 2, nullptr, Vector<Float, 3>(0, 0, 10));

            Intersection hit;

            ASSERT_TRUE(instance->hit(hit, makeRay(Vector<Float, 3>(Float(0.25), Float(0.25), 15), Vector<Float, 3>(0, 0, -1))));

            EXPECT_NEAR(hit.t, 4, tolerances::ACCUMULATED);

            expectNear(hit.transformedPoint, Vector<Float, 3>(Float(0.25), Float(0.25), 11), tolerances::ACCUMULATED);
        }

        TEST_F(InstanceTest, OccludedHitReportsAnyOccluder) {
            Instance * instance = makeInstance(makeTriangles(diffuse), 2);

            Intersection blocked;
            Intersection open;

            EXPECT_TRUE(instance->hit(blocked, makeRay(Vector<Float, 3>(Float(0.25), Float(0.25), 5), Vector<Float, 3>(0, 0, -1)), nullptr, -1));
            EXPECT_FALSE(instance->hit(open, makeRay(Vector<Float, 3>(Float(0.9), Float(0.9), 5), Vector<Float, 3>(0, 0, -1)), nullptr, -1));
        }

        TEST_F(InstanceTest, OccludedHitSkipsExcludedObject) {
            Object * objects = makeTriangles(diffuse);

            Instance * instance = makeInstance(objects, 2);

            Ray r = makeRay(Vector<Float, 3>(Float(0.25), Float(0.25), 5), Vector<Float, 3>(0, 0, -1));

            Intersection skipFirst;
            Intersection skipSecond;

            EXPECT_TRUE(instance->hit(skipFirst, r, instance, 0));
            EXPECT_TRUE(instance->hit(skipSecond, r, instance, 1));

            Instance * single = makeInstance(objects, 1);

            Ray up = makeRay(Vector<Float, 3>(Float(0.25), Float(0.25), -5), Vector<Float, 3>(0, 0, 1));

            Intersection skipped;
            Intersection counted;

            EXPECT_FALSE(single->hit(skipped, up, single, 0));
            EXPECT_TRUE(single->hit(counted, up, nullptr, -1));
        }

        TEST_F(InstanceTest, OccludedHitIgnoresObjectsWithoutMaterial) {
            Instance * invisible = makeInstance(makeTriangles(nullptr), 2);
            Instance * overridden = makeInstance(makeTriangles(nullptr), 2, diffuse);

            Ray r = makeRay(Vector<Float, 3>(Float(0.25), Float(0.25), 5), Vector<Float, 3>(0, 0, -1));

            Intersection ignored;
            Intersection counted;

            EXPECT_FALSE(invisible->hit(ignored, r, nullptr, -1));
            EXPECT_TRUE(overridden->hit(counted, r, nullptr, -1));
        }

        TEST_F(InstanceTest, LightSamplingTargetsObjectsInProportionToPower) {
            Material * dim = allocator.make(Material::makeEmissive(makeSpectrumTexture(1)));

            Object * objects = allocator.allocate<Object>(2);

            objects[0] = Object::makeSphere(dim, nullptr, nullptr, 1);
            objects[1] = Object::makeSphere(emissive, nullptr, nullptr, 1);

            Instance * instance = makeInstance(objects, 2);

            int counts[2] = { 0, 0 };

            for (int i = 0; i < 5000; i++) {
                Float probabilityDensity = 0;
                int objectIndex = -1;

                instance->sampleLight(Vector<Float, 3>(0, 0, 5), probabilityDensity, objectIndex, state);

                ASSERT_GE(objectIndex, 0);
                ASSERT_LT(objectIndex, 2);

                counts[objectIndex]++;
            }

            EXPECT_NEAR(Float(counts[1]) / 5000, Float(0.75), 5 * std::sqrt(Float(0.75) * Float(0.25) / 5000));
        }

        TEST_F(InstanceTest, LightSamplingPointsTowardTransformedObject) {
            Instance * instance = makeInstance(makeSpheres(1, emissive), 1, nullptr, Vector<Float, 3>(), Vector<Float, 3>(), Vector<Float, 3>(2, 2, 2));

            Vector<Float, 3> point(0, 0, 10);

            Float cosThetaMax = std::sqrt(1 - Float(4) / 100);

            for (int i = 0; i < 200; i++) {
                Float probabilityDensity = 0;
                int objectIndex = -1;

                Vector<Float, 3> direction = instance->sampleLight(point, probabilityDensity, objectIndex, state);

                EXPECT_NEAR(direction.length(), 1, tolerances::ACCUMULATED);
                EXPECT_GE(dot(direction, Vector<Float, 3>(0, 0, -1)), cosThetaMax - tolerances::ACCUMULATED);
                EXPECT_EQ(objectIndex, 0);
                EXPECT_NEAR(instance->probabilityDensity(objectIndex, point, direction) / probabilityDensity, 1, tolerances::COARSE);
            }
        }

        class BVHNodeTest : public WorldTest {};

        TEST_F(BVHNodeTest, BVHNodeDefaultsToEmptyInvertedBox) {
            BVHNode<Object> node;

            expectEqual(node.getMin(), Vector<Float, 3>(constants::MAX, constants::MAX, constants::MAX));
            expectEqual(node.getMax(), Vector<Float, 3>(-constants::MAX, -constants::MAX, -constants::MAX));

            EXPECT_FALSE(node.isLeaf());
            EXPECT_EQ(node.getCount(), 0);
            EXPECT_EQ(node.getPower(), 0);
        }

        TEST_F(BVHNodeTest, BVHNodeInteriorStoresBoundsCountAndPower) {
            BVHNode<Object> node(Vector<Float, 3>(0, 1, 2), Vector<Float, 3>(3, 4, 5), 7, Float(2.5));

            expectEqual(node.getMin(), Vector<Float, 3>(0, 1, 2));
            expectEqual(node.getMax(), Vector<Float, 3>(3, 4, 5));

            EXPECT_FALSE(node.isLeaf());
            EXPECT_EQ(node.getCount(), 7);
            EXPECT_EQ(node.getPower(), Float(2.5));
            EXPECT_EQ(node.getElement(0), nullptr);
        }

        TEST_F(BVHNodeTest, BVHNodeLeafExposesElements) {
            Object objects[2];
            Object * elements[2] = { objects, objects + 1 };

            BVHNode<Object> node(Vector<Float, 3>(0, 0, 0), Vector<Float, 3>(1, 1, 1), elements, 2, 1);

            EXPECT_TRUE(node.isLeaf());
            EXPECT_EQ(node.getCount(), 2);
            EXPECT_EQ(node.getElement(0), objects);
            EXPECT_EQ(node.getElement(1), objects + 1);
        }

        TEST_F(BVHNodeTest, BVHNodeIntersectsRayCrossingBox) {
            BVHNode<Object> node(Vector<Float, 3>(0, 0, 0), Vector<Float, 3>(1, 1, 1), 0, 0);

            Intersection intersection;

            EXPECT_TRUE(node.intersect(intersection, makeRay(Vector<Float, 3>(-5, Float(0.5), Float(0.5)), Vector<Float, 3>(1, 0, 0))));
            EXPECT_TRUE(node.intersect(intersection, makeRay(Vector<Float, 3>(5, Float(0.5), Float(0.5)), Vector<Float, 3>(-1, 0, 0))));
            EXPECT_TRUE(node.intersect(intersection, makeRay(Vector<Float, 3>(-1, -1, -1), Vector<Float, 3>(1, 1, 1))));
        }

        TEST_F(BVHNodeTest, BVHNodeIntersectsRayStartingInside) {
            BVHNode<Object> node(Vector<Float, 3>(0, 0, 0), Vector<Float, 3>(1, 1, 1), 0, 0);

            Intersection intersection;

            EXPECT_TRUE(node.intersect(intersection, makeRay(Vector<Float, 3>(Float(0.5), Float(0.5), Float(0.5)), Vector<Float, 3>(0, 1, 0))));
        }

        TEST_F(BVHNodeTest, BVHNodeMissesRayPassingBesideBox) {
            BVHNode<Object> node(Vector<Float, 3>(0, 0, 0), Vector<Float, 3>(1, 1, 1), 0, 0);

            Intersection intersection;

            EXPECT_FALSE(node.intersect(intersection, makeRay(Vector<Float, 3>(-5, Float(1.5), Float(0.5)), Vector<Float, 3>(1, 0, 0))));
            EXPECT_FALSE(node.intersect(intersection, makeRay(Vector<Float, 3>(-5, Float(0.5), Float(-0.5)), Vector<Float, 3>(1, 0, 0))));
        }

        TEST_F(BVHNodeTest, BVHNodeMissesBoxBehindRay) {
            BVHNode<Object> node(Vector<Float, 3>(0, 0, 0), Vector<Float, 3>(1, 1, 1), 0, 0);

            Intersection intersection;

            EXPECT_FALSE(node.intersect(intersection, makeRay(Vector<Float, 3>(-5, Float(0.5), Float(0.5)), Vector<Float, 3>(-1, 0, 0))));
        }

        TEST_F(BVHNodeTest, BVHNodeMissesParallelRayOutsideSlab) {
            BVHNode<Object> node(Vector<Float, 3>(0, 0, 0), Vector<Float, 3>(1, 1, 1), 0, 0);

            Intersection intersection;

            EXPECT_FALSE(node.intersect(intersection, makeRay(Vector<Float, 3>(Float(0.5), 2, -5), Vector<Float, 3>(0, 0, 1))));
            EXPECT_FALSE(node.intersect(intersection, makeRay(Vector<Float, 3>(Float(0.5), -2, -5), Vector<Float, 3>(0, 0, 1))));
            EXPECT_TRUE(node.intersect(intersection, makeRay(Vector<Float, 3>(Float(0.5), Float(0.5), -5), Vector<Float, 3>(0, 0, 1))));
        }

        TEST_F(BVHNodeTest, BVHNodeIgnoresBoxBeyondClosestIntersection) {
            BVHNode<Object> node(Vector<Float, 3>(0, 0, 0), Vector<Float, 3>(1, 1, 1), 0, 0);

            Ray r = makeRay(Vector<Float, 3>(-5, Float(0.5), Float(0.5)), Vector<Float, 3>(1, 0, 0));

            Intersection closer;
            Intersection farther;

            closer.t = 2;
            farther.t = 10;

            EXPECT_FALSE(node.intersect(closer, r));
            EXPECT_TRUE(node.intersect(farther, r));
        }

        class BackgroundTest : public WorldTest {};

        TEST_F(BackgroundTest, JacobianOfEquirectangularFollowsSineOfPolarAngle) {
            Background background = makeConstantBackground(1);
            Float values[1] = { 1 };
            Background equirectangular = makeBackground(false, values, 1, 1);

            EXPECT_NEAR(equirectangular.jacobian(Float(0.5)), 2 * constants::PI * constants::PI, tolerances::ACCUMULATED);
            EXPECT_NEAR(equirectangular.jacobian(0), 0, tolerances::ACCUMULATED);
            EXPECT_NEAR(equirectangular.jacobian(Float(0.25)), equirectangular.jacobian(Float(0.75)), tolerances::ACCUMULATED);
            EXPECT_NEAR(background.jacobian(Float(0.3)), 4 * constants::PI, tolerances::ACCUMULATED);
        }

        TEST_F(BackgroundTest, JacobianOfOctahedralIsConstant) {
            Background background = makeConstantBackground(1);

            for (Float y = 0; y <= 1; y += Float(0.25)) EXPECT_NEAR(background.jacobian(y), 4 * constants::PI, tolerances::ACCUMULATED);
        }

        TEST_F(BackgroundTest, PowerScalesWithRadiusSquaredAndEmission) {
            Background background = makeConstantBackground(3);

            EXPECT_NEAR(background.getPower(), 12 * constants::PI, tolerances::COARSE);

            background.setRadius(2);

            EXPECT_NEAR(background.getPower(), 48 * constants::PI, tolerances::COARSE);
        }

        TEST_F(BackgroundTest, EvaluateConstantEmission) {
            Background background = makeConstantBackground(Float(2.5));

            SampledSpectrum<Float> radiance = background.evaluate(makeRay(Vector<Float, 3>(), Vector<Float, 3>(0, 0, 1)));

            for (int i = 0; i < constants::HERO_COUNT; i++) EXPECT_EQ(radiance[i], Float(2.5));
        }

        TEST_F(BackgroundTest, EquirectangularEvaluateMapsPolesToRows) {
            Float values[2] = { 1, 3 };

            Background background = makeBackground(false, values, 1, 2);

            EXPECT_NEAR(background.evaluate(makeRay(Vector<Float, 3>(), Vector<Float, 3>(0, 1, 0)))[0], 1, tolerances::ACCUMULATED);
            EXPECT_NEAR(background.evaluate(makeRay(Vector<Float, 3>(), Vector<Float, 3>(0, -1, 0)))[0], 3, tolerances::ACCUMULATED);
        }

        TEST_F(BackgroundTest, EquirectangularEvaluateMapsAzimuthToColumns) {
            Float values[4] = { 0, 1, 2, 3 };

            Background background = makeBackground(false, values, 4, 1);

            Float angle = constants::PI / 4;

            EXPECT_NEAR(background.evaluate(makeRay(Vector<Float, 3>(), Vector<Float, 3>(std::cos(angle), 0, std::sin(angle))))[0], 2, tolerances::ACCUMULATED);
            EXPECT_NEAR(background.evaluate(makeRay(Vector<Float, 3>(), Vector<Float, 3>(1, 0, 0)))[0], Float(1.5), tolerances::ACCUMULATED);
        }

        TEST_F(BackgroundTest, EvaluateAppliesRotation) {
            Float values[4] = { 0, 1, 2, 3 };

            Background background = makeBackground(false, values, 4, 1, Vector<Float, 3>(0, constants::PI / 2, 0));

            EXPECT_NEAR(background.evaluate(makeRay(Vector<Float, 3>(), Vector<Float, 3>(1, 0, 0)))[0], Float(0.5), tolerances::ACCUMULATED);
        }

        TEST_F(BackgroundTest, ConstantBackgroundDensityIsUniform) {
            Background background = makeConstantBackground(2);

            for (int i = 0; i < 100; i++) EXPECT_NEAR(background.probabilityDensity(utils::randomUnitVector(state)), 1 / (4 * constants::PI), tolerances::ACCUMULATED);
        }

        TEST_F(BackgroundTest, ConstantBackgroundSamplesUniformDirections) {
            Background background = makeConstantBackground(2);

            Vector<Float, 3> sum;

            for (int i = 0; i < 5000; i++) {
                Vector<Float, 3> direction = background.sample(state);

                EXPECT_NEAR(direction.length(), 1, tolerances::ACCUMULATED);

                sum += direction;
            }

            expectNear(sum / 5000, Vector<Float, 3>(), tolerances::STATISTICAL);
        }

        TEST_F(BackgroundTest, OctahedralSamplingConcentratesOnBrightTexel) {
            Float values[4] = { 0, 0, 0, 1 };

            Vector<Float, 3> rotations[2] = { Vector<Float, 3>(), Vector<Float, 3>(Float(0.7), Float(-0.4), Float(1.1)) };

            for (const Vector<Float, 3> & rotation : rotations) {
                Background background = makeBackground(true, values, 2, 2, rotation);

                for (int i = 0; i < 1000; i++) EXPECT_GT(background.probabilityDensity(background.sample(state)), 0);

                int bright = 0;
                Float sum = 0;

                for (int i = 0; i < 5000; i++) {
                    Float density = background.probabilityDensity(utils::randomUnitVector(state));

                    if (density > 0) bright++;

                    sum += density * 4 * constants::PI;
                }

                EXPECT_NEAR(Float(bright) / 5000, Float(0.25), 5 * std::sqrt(Float(0.25) * Float(0.75) / 5000));
                EXPECT_NEAR(sum / 5000, 1, tolerances::STATISTICAL);
            }
        }

        TEST_F(BackgroundTest, OctahedralDensityIsConstantWithinTexel) {
            Float values[4] = { 0, 0, 0, 1 };

            Background background = makeBackground(true, values, 2, 2);

            for (int i = 0; i < 1000; i++) {
                Float density = background.probabilityDensity(utils::randomUnitVector(state));

                if (density > 0) {
                    EXPECT_NEAR(density, 1 / constants::PI, tolerances::ACCUMULATED);
                }
            }
        }

        TEST_F(BackgroundTest, EquirectangularSamplingStaysInBrightRegion) {
            Float values[16] = {};

            values[1] = 1;

            Background background = makeBackground(false, values, 4, 4);

            for (int i = 0; i < 1000; i++) {
                Vector<Float, 3> direction = background.sample(state);

                EXPECT_NEAR(direction.length(), 1, tolerances::ACCUMULATED);

                EXPECT_GE(direction[1], std::cos(constants::PI / 4) - tolerances::ACCUMULATED);
                EXPECT_GE(direction[0], -tolerances::ACCUMULATED);
                EXPECT_LE(direction[2], tolerances::ACCUMULATED);

                EXPECT_GT(background.probabilityDensity(direction), 0);
            }
        }

        TEST_F(BackgroundTest, EquirectangularDensityVanishesInDarkRegion) {
            Float values[16] = {};

            values[1] = 1;

            Background background = makeBackground(false, values, 4, 4);

            EXPECT_EQ(background.probabilityDensity(Vector<Float, 3>(0, -1, 0)), 0);
            EXPECT_EQ(background.probabilityDensity(Vector<Float, 3>(-1, 0, 0)), 0);
        }

        TEST_F(BackgroundTest, DensityMatchesTexelMassAndJacobian) {
            Float values[8] = { 1, 1, 1, 1, 1, 1, 1, 1 };

            Background background = makeBackground(false, values, 2, 4);

            Vector<Float, 3> direction = normalize(Vector<Float, 3>(1, Float(0.3), Float(0.4)));

            Float v = std::acos(direction[1]) / constants::PI;

            Float weights[4];
            Float total = 0;

            for (int y = 0; y < 4; y++) {
                weights[y] = background.jacobian((Float(y) + Float(0.5)) / 4);
                total += weights[y];
            }

            int row = int(v * 4);

            EXPECT_NEAR(background.probabilityDensity(direction), weights[row] / total / 2 * 8 / background.jacobian(v), tolerances::ACCUMULATED);
        }

        class SceneTest : public InstanceTest {};

        TEST_F(SceneTest, EmptySceneHasNothing) {
            Scene scene;

            EXPECT_EQ(scene.getNumBackgrounds(), 0);
            EXPECT_EQ(scene.getTotalPower(), 0);

            Intersection hit;

            EXPECT_FALSE(scene.hit(hit, makeRay(Vector<Float, 3>(), Vector<Float, 3>(0, 0, 1))));
            EXPECT_EQ(scene.lightProbabilityDensity(nullptr, -1, Vector<Float, 3>(), Vector<Float, 3>(0, 1, 0)), 0);

            const Instance * instance = nullptr;
            int objectIndex = 0;
            Float probabilityDensity = 1;

            Vector<Float, 3> direction = scene.sampleLight(Vector<Float, 3>(), probabilityDensity, instance, objectIndex, state);

            EXPECT_EQ(probabilityDensity, 0);
            EXPECT_EQ(instance, nullptr);
            EXPECT_EQ(objectIndex, -1);

            expectEqual(direction, Vector<Float, 3>(0, 1, 0));
        }

        TEST_F(SceneTest, HitFindsNearestInstance) {
            Object * spheres = makeSpheres(1, diffuse);

            Instance * near = makeInstance(spheres, 1, nullptr, Vector<Float, 3>(0, 0, 0));
            Instance * far = makeInstance(spheres, 1, nullptr, Vector<Float, 3>(0, 0, 5));

            Scene scene = makeScene({ far, near });

            Intersection hit;

            ASSERT_TRUE(scene.hit(hit, makeRay(Vector<Float, 3>(0, 0, -5), Vector<Float, 3>(0, 0, 1))));

            EXPECT_EQ(hit.instance, near);
            EXPECT_NEAR(hit.t, 4, tolerances::ACCUMULATED);

            hit = Intersection();

            ASSERT_TRUE(scene.hit(hit, makeRay(Vector<Float, 3>(0, 0, 10), Vector<Float, 3>(0, 0, -1))));

            EXPECT_EQ(hit.instance, far);
            EXPECT_NEAR(hit.t, 4, tolerances::ACCUMULATED);
        }

        TEST_F(SceneTest, HitMisses) {
            Scene scene = makeScene({ makeInstance(makeSpheres(1, diffuse), 1) });

            Intersection hit;

            EXPECT_FALSE(scene.hit(hit, makeRay(Vector<Float, 3>(5, 0, -5), Vector<Float, 3>(0, 0, 1))));
            EXPECT_FALSE(scene.hit(hit, makeRay(Vector<Float, 3>(0, 0, -5), Vector<Float, 3>(0, 0, -1))));
        }

        TEST_F(SceneTest, OccludedHitDetectsBlockers) {
            Instance * light = makeInstance(makeSpheres(1, emissive), 1, nullptr, Vector<Float, 3>(0, 0, 10));
            Instance * blocker = makeInstance(makeSpheres(1, diffuse), 1, nullptr, Vector<Float, 3>(0, 0, 5));

            Scene blocked = makeScene({ light, blocker }, { light });
            Scene open = makeScene({ light }, { light });

            Ray r = makeRay(Vector<Float, 3>(), Vector<Float, 3>(0, 0, 1));

            Intersection blockedHit;
            Intersection openHit;
            Intersection unrestrictedHit;

            EXPECT_TRUE(blocked.hit(blockedHit, r, light, 0));
            EXPECT_FALSE(open.hit(openHit, r, light, 0));
            EXPECT_TRUE(open.hit(unrestrictedHit, r, nullptr, -1));
        }

        TEST_F(SceneTest, TotalPowerSumsBackgroundsAndLights) {
            Background backgrounds[2] = { makeConstantBackground(2), makeConstantBackground(1) };

            backgrounds[1].setRadius(3);

            Instance * light = makeInstance(makeSpheres(1, emissive), 1);

            Scene scene = makeScene({ light }, { light }, backgrounds, 2);

            EXPECT_EQ(scene.getNumBackgrounds(), 2);

            Float expected = 4 * constants::PI * 2 + 4 * constants::PI * 9 + light->getPower();

            EXPECT_NEAR(scene.getTotalPower(), expected, tolerances::COARSE * expected);
        }

        TEST_F(SceneTest, TotalPowerWithoutLightsIsBackgroundPower) {
            Background backgrounds[1] = { makeConstantBackground(2) };

            Scene scene = makeScene({ makeInstance(makeSpheres(1, diffuse), 1) }, std::vector<Instance *>(), backgrounds, 1);

            EXPECT_NEAR(scene.getTotalPower(), 8 * constants::PI, tolerances::COARSE);
        }

        TEST_F(SceneTest, LightProbabilityDensityWeighsByPower) {
            Background backgrounds[1] = { makeConstantBackground(2) };

            Instance * light = makeInstance(makeSpheres(1, emissive), 1, nullptr, Vector<Float, 3>(0, 0, 5));

            Scene scene = makeScene({ light }, { light }, backgrounds, 1);

            Float total = scene.getTotalPower();

            Vector<Float, 3> point;
            Vector<Float, 3> direction(0, 0, 1);

            EXPECT_NEAR(scene.lightProbabilityDensity(nullptr, -1, point, direction), scene.getBackground(0)->probabilityDensity(direction) * scene.getBackground(0)->getPower() / total, tolerances::ACCUMULATED);
            EXPECT_NEAR(scene.lightProbabilityDensity(light, 0, point, direction), light->probabilityDensity(0, point, direction) * light->getPower(0) / total, tolerances::ACCUMULATED);
        }

        TEST_F(SceneTest, SampledLightsMatchTheirDensityAndPowerShare) {
            Background backgrounds[1] = { makeConstantBackground(2) };

            Instance * light = makeInstance(makeSpheres(1, emissive), 1, nullptr, Vector<Float, 3>(0, 0, 5));

            Scene scene = makeScene({ light }, { light }, backgrounds, 1);

            Float backgroundShare = scene.getBackground(0)->getPower() / scene.getTotalPower();

            Vector<Float, 3> point;

            int fromBackground = 0;

            for (int i = 0; i < 5000; i++) {
                const Instance * instance = nullptr;
                int objectIndex = 0;
                Float probabilityDensity = 0;

                Vector<Float, 3> direction = scene.sampleLight(point, probabilityDensity, instance, objectIndex, state);

                EXPECT_NEAR(direction.length(), 1, tolerances::ACCUMULATED);
                EXPECT_NEAR(scene.lightProbabilityDensity(instance, objectIndex, point, direction) / probabilityDensity, 1, tolerances::COARSE);

                if (!instance) fromBackground++;
                else EXPECT_EQ(instance, light);
            }

            EXPECT_NEAR(Float(fromBackground) / 5000, backgroundShare, 5 * std::sqrt(backgroundShare * (1 - backgroundShare) / 5000));
        }

        TEST_F(SceneTest, SamplingWithOnlyBackgroundsUsesBackground) {
            Background backgrounds[2] = { makeConstantBackground(1), makeConstantBackground(1) };

            Instance * occluder = makeInstance(makeSpheres(1, diffuse), 1);

            Scene scene = makeScene({ occluder }, std::vector<Instance *>(), backgrounds, 2);

            for (int i = 0; i < 100; i++) {
                const Instance * instance = occluder;
                int objectIndex = 0;
                Float probabilityDensity = 0;

                scene.sampleLight(Vector<Float, 3>(), probabilityDensity, instance, objectIndex, state);

                EXPECT_EQ(instance, nullptr);
                EXPECT_EQ(objectIndex, -1);
                EXPECT_GT(probabilityDensity, 0);
            }
        }
    }
}