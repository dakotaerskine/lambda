#pragma once

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "core/allocator.h"
#include "core/builder.h"
#include "core/constants.h"
#include "geometry/background.h"
#include "geometry/bvh.h"
#include "geometry/instance.h"
#include "geometry/object.h"
#include "geometry/scene.h"
#include "math/complex.h"
#include "math/matrix.h"
#include "math/random.h"
#include "math/ray.h"
#include "math/spectrum.h"
#include "math/vector.h"
#include "render/camera.h"
#include "render/film.h"
#include "render/integrator.h"
#include "shading/material.h"
#include "shading/texture.h"
#include "test/tolerances.h"

namespace lambda {
    class GeometryTest : public ::testing::Test {
        protected:
            template <int N>
            static void expectEqual(const Vector<Float, N> & actual, const Vector<Float, N> & expected) {
                for (int i = 0; i < N; i++) EXPECT_EQ(actual[i], expected[i]);
            }

            template <int N>
            static void expectNear(const Vector<Float, N> & actual, const Vector<Float, N> & expected, Float tolerance = tolerances::STANDARD) {
                for (int i = 0; i < N; i++) EXPECT_NEAR(actual[i], expected[i], tolerance);
            }

            template <int N>
            static void expectEqual(const Matrix<Float, N> & actual, const Matrix<Float, N> & expected) {
                for (int i = 0; i < N; i++)
                    for (int j = 0; j < N; j++) EXPECT_EQ(actual(i, j), expected(i, j));
            }

            template <int N>
            static void expectNear(const Matrix<Float, N> & actual, const Matrix<Float, N> & expected, Float tolerance = tolerances::STANDARD) {
                for (int i = 0; i < N; i++)
                    for (int j = 0; j < N; j++) EXPECT_NEAR(actual(i, j), expected(i, j), tolerance);
            }
    };

    class RandomizedTest : public GeometryTest {
        protected:
            Allocator allocator;
            Random state;

            RandomizedTest() : state(12345, 0) {}

            LinearSpectrum<Float> * makeSpectrum(Float value) {
                Float lambdas[2] = { 0, constants::MAX };
                Float values[2] = { value, value };

                return allocator.make(LinearSpectrum<Float>(2, allocator.allocate(lambdas, 2), allocator.allocate(values, 2)));
            }

            LinearSpectrum<Complex> * makeComplexSpectrum(const Complex & value) {
                Float lambdas[2] = { 0, constants::MAX };
                Complex values[2] = { value, value };

                return allocator.make(LinearSpectrum<Complex>(2, allocator.allocate(lambdas, 2), allocator.allocate(values, 2)));
            }

            ScalarTexture * makeScalarTexture(Float value) { return allocator.make(ScalarTexture::makeConstant(value)); }

            SpectrumTexture * makeSpectrumTexture(Float value) { return allocator.make(SpectrumTexture::makeConstant(makeSpectrum(value))); }
    };

    class WorldTest : public RandomizedTest {
        protected:
            Ray makeRay(const Vector<Float, 3> & origin, const Vector<Float, 3> & direction) { return Ray(origin, direction, SampledSpectrum<Float>(550), nullptr); }

            Instance * makeInstance(Object * objects, int count, Material * material = nullptr, const Vector<Float, 3> & translation = Vector<Float, 3>(), const Vector<Float, 3> & rotation = Vector<Float, 3>(), const Vector<Float, 3> & scale = Vector<Float, 3>(1, 1, 1)) {
                Object ** elements = allocator.allocate<Object *>(count);

                std::vector<Object *> lightObjects;

                for (int i = 0; i < count; i++) {
                    elements[i] = objects + i;

                    const Material * current = material ? material : objects[i].getMaterial();

                    if (current && current->isEmissive()) lightObjects.push_back(objects + i);
                }

                std::vector<BVHNode<Object>> nodes = Builder::buildBVH<Object, false>(elements, count);

                BVHNode<Object> * node = allocator.allocate(nodes.data(), int(nodes.size()));
                BVHNode<Object> * lightNode = nullptr;

                if (!lightObjects.empty()) {
                    std::vector<BVHNode<Object>> lightNodes = Builder::buildBVH<Object, true>(allocator.allocate(lightObjects.data(), int(lightObjects.size())), int(lightObjects.size()), material != nullptr);

                    lightNode = allocator.allocate(lightNodes.data(), int(lightNodes.size()));
                }

                return allocator.make(Instance(objects, count, node, lightNode, material, nullptr, nullptr, translation, rotation, scale));
            }

            Background makeBackground(bool octahedral, const Float * values, int width, int height, const Vector<Float, 3> & rotation = Vector<Float, 3>()) {
                SpectrumTexture * emission = allocator.make(SpectrumTexture::makeScalar(allocator.make(ScalarTexture::makeImage(allocator.allocate(values, width * height), width, height, 1, 1, 0, 0))));

                Background background = octahedral ? Background::makeOctahedral(emission, rotation) : Background::makeEquirectangular(emission, rotation);

                std::vector<Float> conditionalCDF, marginalCDF;

                Builder::buildCDF(background, conditionalCDF, marginalCDF);

                background.setCDF(allocator.allocate(conditionalCDF.data(), int(conditionalCDF.size())), allocator.allocate(marginalCDF.data(), int(marginalCDF.size())));

                return background;
            }

            Background makeConstantBackground(Float emission) {
                Background background = Background::makeOctahedral(makeSpectrumTexture(emission), Vector<Float, 3>());

                std::vector<Float> conditionalCDF, marginalCDF;

                Builder::buildCDF(background, conditionalCDF, marginalCDF);

                background.setCDF(allocator.allocate(conditionalCDF.data(), int(conditionalCDF.size())), allocator.allocate(marginalCDF.data(), int(marginalCDF.size())));

                return background;
            }

            Scene makeScene(std::vector<Instance *> instances, std::vector<Instance *> lights = std::vector<Instance *>(), Background * backgrounds = nullptr, int numBackgrounds = 0) {
                std::vector<BVHNode<Instance>> nodes = Builder::buildBVH<Instance, false>(allocator.allocate(instances.data(), int(instances.size())), int(instances.size()));
                std::vector<BVHNode<Instance>> lightNodes = Builder::buildBVH<Instance, true>(allocator.allocate(lights.data(), int(lights.size())), int(lights.size()));

                return Scene(allocator.allocate(nodes.data(), int(nodes.size())), allocator.allocate(lightNodes.data(), int(lightNodes.size())), backgrounds, numBackgrounds);
            }
    };

    class IntegratorTest : public WorldTest {
        protected:
            ColorSpace space;
            Camera camera;

            IntegratorTest() : space(constants::CHROMATICITIES_SRGB), camera(Camera::makePerspective(nullptr, Vector<Float, 3>(0, 0, 5), Vector<Float, 3>(0, 0, 0), Vector<Float, 3>(0, 1, 0), constants::PI / 3, 1)) {}

            Film * makeFilm(int width, int height) { return allocator.make(Film(&space, width, height, constants::CIE_LAMBDA_MIN, constants::CIE_LAMBDA_MAX, allocator.allocate<Float>(3 * width * height))); }

            Integrator makeIntegrator(Film * film, Scene * scene, int samples = 16, int depth = 4, uint64_t seed = 1) {
                Integrator integrator(samples, depth, seed, film);

                integrator.setScene(scene);
                integrator.setCamera(&camera);

                return integrator;
            }

            void render(Integrator & integrator) {
                int completed = 0;

                integrator.renderImage(&completed);

                EXPECT_EQ(completed, integrator.getFilm()->getWidth() * integrator.getFilm()->getHeight());
            }

            void expectMeanLuminance(const Film & film, Float expected) {
                int pixels = film.getWidth() * film.getHeight();

                Float sum = 0;
                Float sumSquared = 0;

                for (int j = 0; j < film.getHeight(); j++)
                    for (int i = 0; i < film.getWidth(); i++) {
                        Float value = film.getPixel(i, j)[1];

                        sum += value;
                        sumSquared += value * value;
                    }

                Float mean = sum / Float(pixels);
                Float variance = std::fmax(sumSquared / Float(pixels) - mean * mean, Float(0));

                EXPECT_NEAR(mean, expected, 5 * std::sqrt(variance / Float(pixels)) + tolerances::COARSE);
            }
    };

    class IOTest : public IntegratorTest {
        protected:
            inline static Allocator colorAllocator;

            Scene emptyScene;
            std::vector<std::string> paths;

            static void SetUpTestSuite() { ColorSpace::initialize(colorAllocator); }

            void TearDown() override {
                for (const std::string & path : paths) std::remove(path.c_str());
            }

            std::string makePath(const std::string & name) {
                std::string path = ::testing::TempDir() + "lambda_" + ::testing::UnitTest::GetInstance()->current_test_info()->name() + "_" + name;

                paths.push_back(path);

                return path;
            }

            std::string writeFile(const std::string & name, const std::string & content) {
                std::string path = makePath(name);

                std::ofstream file(path, std::ios::binary);

                file << content;

                return path;
            }

            Film * makeFilledFilm(int width, int height) {
                Film * film = makeFilm(width, height);

                for (int j = 0; j < height; j++)
                    for (int i = 0; i < width; i++) film->addSample(i, j, SampledSpectrum<Float>(Float(0.2) + Float(0.3) * Float(i + width * j)), film->sampleLambdas(state));

                return film;
            }
    };
}