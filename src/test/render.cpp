#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>

#include <gtest/gtest.h>

#include "core/color.h"
#include "geometry/background.h"
#include "geometry/object.h"
#include "math/ray.h"
#include "math/spectrum.h"
#include "render/camera.h"
#include "render/film.h"
#include "render/integrator.h"
#include "shading/material.h"
#include "shading/medium.h"
#include "shading/texture.h"
#include "test/test.h"
#include "test/tolerances.h"

namespace lambda {
    namespace {
        class CameraTest : public GeometryTest {};

        TEST_F(CameraTest, PerspectiveCenterRayLooksForward) {
            Medium medium;

            Camera camera = Camera::makePerspective(&medium, Vector<Float, 3>(0, 0, 5), Vector<Float, 3>(0, 0, 0), Vector<Float, 3>(0, 1, 0), constants::PI / 2, 2);

            Ray r = camera.getRay(Float(0.5), Float(0.5), SampledSpectrum<Float>(550));

            expectNear(r.getOrigin(), Vector<Float, 3>(0, 0, 5));
            expectNear(r.getDirection(), Vector<Float, 3>(0, 0, -1));

            EXPECT_EQ(r.getLambdas(), SampledSpectrum<Float>(550));
            EXPECT_EQ(r.getMedium(), &medium);
        }

        TEST_F(CameraTest, PerspectiveCornersFollowFieldOfViewAndAspect) {
            Camera camera = Camera::makePerspective(nullptr, Vector<Float, 3>(0, 0, 5), Vector<Float, 3>(0, 0, 0), Vector<Float, 3>(0, 1, 0), constants::PI / 2, 2);

            SampledSpectrum<Float> lambdas(550);

            expectNear(camera.getRay(0, 0, lambdas).getDirection(), normalize(Vector<Float, 3>(-2, 1, -1)));
            expectNear(camera.getRay(1, 0, lambdas).getDirection(), normalize(Vector<Float, 3>(2, 1, -1)));
            expectNear(camera.getRay(0, 1, lambdas).getDirection(), normalize(Vector<Float, 3>(-2, -1, -1)));
            expectNear(camera.getRay(1, 1, lambdas).getDirection(), normalize(Vector<Float, 3>(2, -1, -1)));
        }

        TEST_F(CameraTest, PerspectiveRaysAreNormalized) {
            Camera camera = Camera::makePerspective(nullptr, Vector<Float, 3>(1, 2, 3), Vector<Float, 3>(-2, 0, 1), Vector<Float, 3>(0, 1, 0), Float(0.9), Float(1.5));

            for (Float u = 0; u <= 1; u += Float(0.25))
                for (Float v = 0; v <= 1; v += Float(0.25)) EXPECT_NEAR(camera.getRay(u, v, SampledSpectrum<Float>(550)).getDirection().length(), 1, tolerances::STANDARD);
        }

        TEST_F(CameraTest, OrthographicRaysAreParallel) {
            Camera camera = Camera::makeOrthographic(nullptr, Vector<Float, 3>(0, 0, 5), Vector<Float, 3>(0, 0, 0), Vector<Float, 3>(0, 1, 0), 4, 2);

            SampledSpectrum<Float> lambdas(550);

            for (Float u = 0; u <= 1; u += Float(0.5))
                for (Float v = 0; v <= 1; v += Float(0.5)) expectNear(camera.getRay(u, v, lambdas).getDirection(), Vector<Float, 3>(0, 0, -1));
        }

        TEST_F(CameraTest, OrthographicOriginsSpanImagePlane) {
            Medium medium;

            Camera camera = Camera::makeOrthographic(&medium, Vector<Float, 3>(0, 0, 5), Vector<Float, 3>(0, 0, 0), Vector<Float, 3>(0, 1, 0), 4, 2);

            SampledSpectrum<Float> lambdas(550);

            expectNear(camera.getRay(Float(0.5), Float(0.5), lambdas).getOrigin(), Vector<Float, 3>(0, 0, 5));
            expectNear(camera.getRay(0, 0, lambdas).getOrigin(), Vector<Float, 3>(-4, 2, 5));
            expectNear(camera.getRay(1, 1, lambdas).getOrigin(), Vector<Float, 3>(4, -2, 5));

            EXPECT_EQ(camera.getRay(0, 0, lambdas).getMedium(), &medium);
        }

        TEST_F(CameraTest, SphericalMapsImageToDirections) {
            Camera camera = Camera::makeSpherical(nullptr, Vector<Float, 3>(1, 2, 3), Vector<Float, 3>(1, 2, -2), Vector<Float, 3>(0, 1, 0));

            SampledSpectrum<Float> lambdas(550);

            expectNear(camera.getRay(Float(0.5), Float(0.5), lambdas).getOrigin(), Vector<Float, 3>(1, 2, 3));
            expectNear(camera.getRay(Float(0.5), Float(0.5), lambdas).getDirection(), Vector<Float, 3>(0, 0, -1));
            expectNear(camera.getRay(0, Float(0.5), lambdas).getDirection(), Vector<Float, 3>(0, 0, 1));
            expectNear(camera.getRay(Float(0.25), Float(0.5), lambdas).getDirection(), Vector<Float, 3>(-1, 0, 0));
            expectNear(camera.getRay(Float(0.75), Float(0.5), lambdas).getDirection(), Vector<Float, 3>(1, 0, 0));
            expectNear(camera.getRay(Float(0.5), 0, lambdas).getDirection(), Vector<Float, 3>(0, 1, 0));
            expectNear(camera.getRay(Float(0.5), 1, lambdas).getDirection(), Vector<Float, 3>(0, -1, 0));
        }

        TEST_F(CameraTest, DefaultCameraRayStartsAtOrigin) {
            Camera camera;

            EXPECT_NEAR(camera.getRay(Float(0.5), Float(0.5), SampledSpectrum<Float>(550)).getOrigin().length(), 0, tolerances::STANDARD);
        }

        TEST_F(CameraTest, WorldToCameraAlignsAxesWithView) {
            Vector<Float, 3> position(1, 2, 3);

            Camera cameras[2] = { Camera::makePerspective(nullptr, position, Vector<Float, 3>(1, 2, -2), Vector<Float, 3>(0, 1, 0), Float(0.9), 1), Camera::makeOrthographic(nullptr, position, Vector<Float, 3>(1, 2, -2), Vector<Float, 3>(0, 1, 0), 2, 1) };

            for (const Camera & camera : cameras) {
                Vector<Float, 3> point = position + Vector<Float, 3>(2, 3, -5);

                Vector<Float, 4> local = camera.getWorldToCamera() * Vector<Float, 4>(point, 1);

                expectNear(Vector<Float, 3>(local), Vector<Float, 3>(2, 3, 5), tolerances::ACCUMULATED);

                EXPECT_NEAR(local[3], 1, tolerances::STANDARD);
            }
        }

        TEST_F(CameraTest, WorldToCameraSphericalAlignsAxesWithView) {
            Vector<Float, 3> position(1, 2, 3);

            Camera camera = Camera::makeSpherical(nullptr, position, Vector<Float, 3>(1, 2, -2), Vector<Float, 3>(0, 1, 0));

            Vector<Float, 4> local = camera.getWorldToCamera() * Vector<Float, 4>(position + Vector<Float, 3>(2, 3, -5), 1);

            expectNear(Vector<Float, 3>(local), Vector<Float, 3>(2, 3, 5), tolerances::ACCUMULATED);
        }

        TEST_F(CameraTest, PerspectiveNDCInvertsRayGeneration) {
            Camera camera = Camera::makePerspective(nullptr, Vector<Float, 3>(1, 2, 3), Vector<Float, 3>(-2, 0, 1), Vector<Float, 3>(0, 1, 0), Float(0.9), Float(1.5));

            Matrix<Float, 4> worldToNDC = camera.getWorldToNDC();

            for (Float u = Float(0.1); u < 1; u += Float(0.2))
                for (Float v = Float(0.1); v < 1; v += Float(0.2)) {
                    Vector<Float, 4> ndc = worldToNDC * Vector<Float, 4>(camera.getRay(u, v, SampledSpectrum<Float>(550)).at(3), 1);

                    EXPECT_NEAR(ndc[0] / ndc[3], u, tolerances::ACCUMULATED);
                    EXPECT_NEAR(ndc[1] / ndc[3], v, tolerances::ACCUMULATED);
                }
        }

        TEST_F(CameraTest, OrthographicNDCInvertsRayGeneration) {
            Camera camera = Camera::makeOrthographic(nullptr, Vector<Float, 3>(1, 2, 3), Vector<Float, 3>(-2, 0, 1), Vector<Float, 3>(0, 1, 0), 3, Float(1.5));

            Matrix<Float, 4> worldToNDC = camera.getWorldToNDC();

            for (Float u = Float(0.1); u < 1; u += Float(0.2))
                for (Float v = Float(0.1); v < 1; v += Float(0.2)) {
                    Vector<Float, 4> ndc = worldToNDC * Vector<Float, 4>(camera.getRay(u, v, SampledSpectrum<Float>(550)).at(3), 1);

                    EXPECT_NEAR(ndc[0] / ndc[3], u, tolerances::ACCUMULATED);
                    EXPECT_NEAR(ndc[1] / ndc[3], v, tolerances::ACCUMULATED);
                }
        }

        TEST_F(CameraTest, SphericalNDCIsIdentity) {
            Camera camera = Camera::makeSpherical(nullptr, Vector<Float, 3>(1, 2, 3), Vector<Float, 3>(1, 2, -2), Vector<Float, 3>(0, 1, 0));

            expectEqual(camera.getWorldToNDC(), Matrix<Float, 4>());
        }

        class FilmTest : public RandomizedTest {};

        TEST_F(FilmTest, DefaultFilmIsEmpty) {
            Film film;

            EXPECT_EQ(film.getWidth(), 0);
            EXPECT_EQ(film.getHeight(), 0);
            EXPECT_EQ(film.getSpace(), ColorSpace::sRGB());
        }

        TEST_F(FilmTest, StoresConfiguration) {
            ColorSpace space(constants::CHROMATICITIES_SRGB);

            Film film(&space, 4, 3, 400, 700, allocator.allocate<Float>(36));

            EXPECT_EQ(film.getSpace(), &space);
            EXPECT_EQ(film.getWidth(), 4);
            EXPECT_EQ(film.getHeight(), 3);
            EXPECT_EQ(film.getLambdaMin(), 400);
            EXPECT_EQ(film.getLambdaMax(), 700);
        }

        TEST_F(FilmTest, PixelsStartAtZero) {
            ColorSpace space(constants::CHROMATICITIES_SRGB);

            Film film(&space, 2, 2, 400, 700, allocator.allocate<Float>(12));

            for (int i = 0; i < 2; i++)
                for (int j = 0; j < 2; j++) expectEqual(film.getPixel(i, j), Vector<Float, 3>());
        }

        TEST_F(FilmTest, SampledWavelengthsStayWithinRange) {
            ColorSpace space(constants::CHROMATICITIES_SRGB);

            Float ranges[6][2] = { { 360, 830 }, { 400, 700 }, { 300, 900 }, { 500, 1000 }, { 900, 1000 }, { 200, 300 } };

            for (int r = 0; r < 6; r++) {
                Film film(&space, 1, 1, ranges[r][0], ranges[r][1], allocator.allocate<Float>(3));

                for (int i = 0; i < 1000; i++) {
                    SampledSpectrum<Float> lambdas = film.sampleLambdas(state);

                    for (int j = 0; j < constants::HERO_COUNT; j++) {
                        EXPECT_GE(lambdas[j], ranges[r][0]);
                        EXPECT_LE(lambdas[j], ranges[r][1]);
                    }
                }
            }
        }

        TEST_F(FilmTest, RangeOutsideVisibleSpectrumContributesNothingAndStaysFinite) {
            ColorSpace space(constants::CHROMATICITIES_SRGB);

            Float ranges[2][2] = { { 900, 1000 }, { 200, 300 } };

            for (int r = 0; r < 2; r++) {
                Film film(&space, 1, 1, ranges[r][0], ranges[r][1], allocator.allocate<Float>(3));

                for (int i = 0; i < 100; i++) film.addSample(0, 0, SampledSpectrum<Float>(1), film.sampleLambdas(state));

                Vector<Float, 3> pixel = film.getPixel(0, 0);

                for (int k = 0; k < 3; k++) EXPECT_EQ(pixel[k], 0);
            }
        }

        TEST_F(FilmTest, SampledWavelengthsAreStratified) {
            ColorSpace space(constants::CHROMATICITIES_SRGB);

            Film film(&space, 1, 1, constants::CIE_LAMBDA_MIN, constants::CIE_LAMBDA_MAX, allocator.allocate<Float>(3));

            Float cdfMin = std::tanh(constants::WAVELENGTH_PDF_WIDTH * (constants::CIE_LAMBDA_MIN - constants::WAVELENGTH_PDF_CENTER));
            Float cdfMax = std::tanh(constants::WAVELENGTH_PDF_WIDTH * (constants::CIE_LAMBDA_MAX - constants::WAVELENGTH_PDF_CENTER));

            for (int i = 0; i < 100; i++) {
                SampledSpectrum<Float> lambdas = film.sampleLambdas(state);

                Float quantiles[constants::HERO_COUNT];

                for (int j = 0; j < constants::HERO_COUNT; j++) quantiles[j] = (std::tanh(constants::WAVELENGTH_PDF_WIDTH * (lambdas[j] - constants::WAVELENGTH_PDF_CENTER)) - cdfMin) / (cdfMax - cdfMin);

                std::sort(quantiles, quantiles + constants::HERO_COUNT);

                for (int j = 1; j < constants::HERO_COUNT; j++) EXPECT_NEAR(quantiles[j] - quantiles[j - 1], Float(1) / constants::HERO_COUNT, tolerances::COARSE);
            }
        }

        TEST_F(FilmTest, AddSampleAccumulatesIntoOnePixel) {
            ColorSpace space(constants::CHROMATICITIES_SRGB);

            Film film(&space, 2, 2, 400, 700, allocator.allocate<Float>(12));

            SampledSpectrum<Float> lambdas = film.sampleLambdas(state);

            film.addSample(1, 0, SampledSpectrum<Float>(1), lambdas);

            Vector<Float, 3> once = film.getPixel(1, 0);

            EXPECT_GT(once[1], 0);

            film.addSample(1, 0, SampledSpectrum<Float>(1), lambdas);

            expectNear(film.getPixel(1, 0), 2 * once);
            expectEqual(film.getPixel(0, 0), Vector<Float, 3>());
            expectEqual(film.getPixel(0, 1), Vector<Float, 3>());
            expectEqual(film.getPixel(1, 1), Vector<Float, 3>());
        }

        TEST_F(FilmTest, AddSampleIsLinearInRadiance) {
            ColorSpace space(constants::CHROMATICITIES_SRGB);

            Film film(&space, 2, 1, 400, 700, allocator.allocate<Float>(6));

            SampledSpectrum<Float> lambdas = film.sampleLambdas(state);

            film.addSample(0, 0, SampledSpectrum<Float>(1), lambdas);
            film.addSample(1, 0, SampledSpectrum<Float>(3), lambdas);

            expectNear(film.getPixel(1, 0), 3 * film.getPixel(0, 0));
        }

        TEST_F(FilmTest, AddSampleOfZeroRadianceLeavesPixelBlack) {
            ColorSpace space(constants::CHROMATICITIES_SRGB);

            Film film(&space, 1, 1, 400, 700, allocator.allocate<Float>(3));

            film.addSample(0, 0, SampledSpectrum<Float>(), film.sampleLambdas(state));

            expectEqual(film.getPixel(0, 0), Vector<Float, 3>());
        }

        TEST_F(FilmTest, EqualEnergyRadianceHasUnitTristimulus) {
            ColorSpace space(constants::CHROMATICITIES_SRGB);

            Float ranges[2][2] = { { 360, 830 }, { 300, 900 } };

            for (int r = 0; r < 2; r++) {
                int pixels = 4000;

                Film film(&space, pixels, 1, ranges[r][0], ranges[r][1], allocator.allocate<Float>(3 * pixels));

                for (int i = 0; i < pixels; i++) film.addSample(i, 0, SampledSpectrum<Float>(1), film.sampleLambdas(state));

                for (int k = 0; k < 3; k++) {
                    Float sum = 0;
                    Float sumSquared = 0;

                    for (int i = 0; i < pixels; i++) {
                        Float value = film.getPixel(i, 0)[k];

                        sum += value;
                        sumSquared += value * value;
                    }

                    Float mean = sum / Float(pixels);
                    Float variance = std::fmax(sumSquared / Float(pixels) - mean * mean, Float(0));

                    EXPECT_NEAR(mean, 1, 5 * std::sqrt(variance / Float(pixels)) + tolerances::COARSE);
                }
            }
        }

        TEST_F(FilmTest, NormalizePixelAveragesSamples) {
            ColorSpace space(constants::CHROMATICITIES_SRGB);

            Film film(&space, 2, 1, 400, 700, allocator.allocate<Float>(6));

            SampledSpectrum<Float> lambdas = film.sampleLambdas(state);

            for (int i = 0; i < 4; i++) film.addSample(0, 0, SampledSpectrum<Float>(1), lambdas);

            Vector<Float, 3> total = film.getPixel(0, 0);

            film.normalizePixel(0, 0, 4);

            expectNear(film.getPixel(0, 0), total / 4);
        }

        TEST_F(FilmTest, NormalizePixelIgnoresEmptySampleCount) {
            ColorSpace space(constants::CHROMATICITIES_SRGB);

            Film film(&space, 1, 1, 400, 700, allocator.allocate<Float>(3));

            film.addSample(0, 0, SampledSpectrum<Float>(1), film.sampleLambdas(state));

            Vector<Float, 3> before = film.getPixel(0, 0);

            film.normalizePixel(0, 0, 0);

            expectEqual(film.getPixel(0, 0), before);
        }

        TEST_F(IntegratorTest, DefaultIntegratorIsUnconfigured) {
            Integrator integrator;

            EXPECT_EQ(integrator.getSamples(), 0);
            EXPECT_EQ(integrator.getFilm(), nullptr);
            EXPECT_EQ(integrator.getCamera(), nullptr);
        }

        TEST_F(IntegratorTest, StoresConfiguration) {
            Film * film = makeFilm(2, 2);

            Scene scene;

            Integrator integrator = makeIntegrator(film, &scene, 25);

            EXPECT_EQ(integrator.getSamples(), 25);
            EXPECT_EQ(integrator.getFilm(), film);
            EXPECT_EQ(integrator.getCamera(), &camera);
        }

        TEST_F(IntegratorTest, EmptySceneRendersBlack) {
            Film * film = makeFilm(4, 4);

            Scene scene;

            Integrator integrator = makeIntegrator(film, &scene);

            render(integrator);

            for (int j = 0; j < 4; j++)
                for (int i = 0; i < 4; i++) expectEqual(film->getPixel(i, j), Vector<Float, 3>());
        }

        TEST_F(IntegratorTest, UniformEnvironmentRendersItsRadiance) {
            Film * film = makeFilm(8, 8);

            Background backgrounds[1] = { makeConstantBackground(Float(1.5)) };

            Scene scene = makeScene({}, {}, backgrounds, 1);

            Integrator integrator = makeIntegrator(film, &scene);

            render(integrator);

            expectMeanLuminance(*film, Float(1.5));
        }

        TEST_F(IntegratorTest, EmissiveEnclosureRendersItsEmission) {
            Film * film = makeFilm(8, 8);

            Material * emission = allocator.make(Material::makeEmissive(makeSpectrumTexture(2)));

            Object * sphere = allocator.allocate<Object>(1);

            sphere[0] = Object::makeSphere(emission, nullptr, nullptr, 10);

            Instance * enclosure = makeInstance(sphere, 1);

            Scene scene = makeScene({ enclosure }, { enclosure });

            Integrator integrator = makeIntegrator(film, &scene);

            render(integrator);

            expectMeanLuminance(*film, 2);
        }

        TEST_F(IntegratorTest, ConvexWhiteObjectIsInvisibleInUniformEnvironment) {
            Film * film = makeFilm(8, 8);

            Background backgrounds[1] = { makeConstantBackground(1) };

            Material * white = allocator.make(Material::makeDiffuse(makeSpectrumTexture(1), nullptr));

            Object * sphere = allocator.allocate<Object>(1);

            sphere[0] = Object::makeSphere(white, nullptr, nullptr, 1);

            Scene scene = makeScene({ makeInstance(sphere, 1) }, {}, backgrounds, 1);

            Integrator integrator = makeIntegrator(film, &scene, 64);

            render(integrator);

            expectMeanLuminance(*film, 1);
        }

        TEST_F(IntegratorTest, BlackObjectBlocksEnvironment) {
            Film * film = makeFilm(1, 1);

            Background backgrounds[1] = { makeConstantBackground(1) };

            Material * black = allocator.make(Material::makeDiffuse(makeSpectrumTexture(0), nullptr));

            Object * sphere = allocator.allocate<Object>(1);

            sphere[0] = Object::makeSphere(black, nullptr, nullptr, 3);

            Scene scene = makeScene({ makeInstance(sphere, 1) }, {}, backgrounds, 1);

            Integrator integrator = makeIntegrator(film, &scene, 16);

            render(integrator);

            expectEqual(film->getPixel(0, 0), Vector<Float, 3>());
        }

        TEST_F(IntegratorTest, RenderingIsDeterministicForSeed) {
            Background backgrounds[1] = { makeConstantBackground(1) };

            Material * white = allocator.make(Material::makeDiffuse(makeSpectrumTexture(Float(0.5)), nullptr));

            Object * sphere = allocator.allocate<Object>(1);

            sphere[0] = Object::makeSphere(white, nullptr, nullptr, 1);

            Scene scene = makeScene({ makeInstance(sphere, 1) }, {}, backgrounds, 1);

            Film * first = makeFilm(4, 4);
            Film * second = makeFilm(4, 4);
            Film * other = makeFilm(4, 4);

            Integrator a = makeIntegrator(first, &scene, 4, 4, 7);
            Integrator b = makeIntegrator(second, &scene, 4, 4, 7);
            Integrator c = makeIntegrator(other, &scene, 4, 4, 8);

            render(a);
            render(b);
            render(c);

            int differing = 0;

            for (int j = 0; j < 4; j++)
                for (int i = 0; i < 4; i++) {
                    expectEqual(first->getPixel(i, j), second->getPixel(i, j));

                    if (first->getPixel(i, j) != other->getPixel(i, j)) differing++;
                }

            EXPECT_GT(differing, 0);
        }
    }
}