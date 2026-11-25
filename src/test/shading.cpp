#include <cmath>

#include <gtest/gtest.h>

#include "core/constants.h"
#include "shading/fluorophore.h"
#include "shading/material.h"
#include "shading/medium.h"
#include "shading/texture.h"
#include "test/test.h"
#include "test/tolerances.h"

namespace lambda {
    namespace {
        class FresnelTest : public RandomizedTest {
            protected:
                Intersection intersection;

                Material makeCoating(Float n, Float thickness) { return Material::makeCoated(1, allocator.make(makeComplexSpectrum(Complex(n))), allocator.make(makeScalarTexture(thickness)), nullptr); }
        };

        TEST_F(FresnelTest, AirToGlassAtNormalIncidenceReflectsFourPercent) {
            Float n = Float(1.5);

            Float reflectance = 0;
            Float transmittance = 0;

            Material::fresnelReflectanceTransmittance(nullptr, intersection, Complex(1), Complex(n), 1, 550, reflectance, transmittance);

            EXPECT_NEAR(reflectance, (n - 1) * (n - 1) / ((n + 1) * (n + 1)), tolerances::STANDARD);
            EXPECT_NEAR(reflectance + transmittance, 1, tolerances::STANDARD);
        }

        TEST_F(FresnelTest, NormalIncidenceOnConductor) {
            Float reflectance = 0;
            Float transmittance = 0;

            Material::fresnelReflectanceTransmittance(nullptr, intersection, Complex(1), Complex(Float(0.2), 3), 1, 550, reflectance, transmittance);

            EXPECT_NEAR(reflectance, (Float(0.8) * Float(0.8) + 9) / (Float(1.2) * Float(1.2) + 9), tolerances::STANDARD);
            EXPECT_NEAR(transmittance, 0, tolerances::STANDARD);
        }

        TEST_F(FresnelTest, GlassToAirBeyondCriticalAngleReflectsTotally) {
            Float n = Float(1.5);
            Float cosThetaI = std::cos(std::asin(1 / n) + Float(0.1));

            Float reflectance = 0;
            Float transmittance = 0;

            Material::fresnelReflectanceTransmittance(nullptr, intersection, Complex(n), Complex(1), cosThetaI, 550, reflectance, transmittance);

            EXPECT_NEAR(reflectance, 1, tolerances::STANDARD);
            EXPECT_NEAR(transmittance, 0, tolerances::STANDARD);
        }

        TEST_F(FresnelTest, AirToGlassAtBrewsterAngleReflectsOnlySPolarization) {
            Float n = Float(1.5);
            Float thetaI = std::atan(n);
            Float thetaT = constants::PI / 2 - thetaI;
            Float amplitudeS = std::sin(thetaI - thetaT) / std::sin(thetaI + thetaT);

            Float reflectance = 0;
            Float transmittance = 0;

            Material::fresnelReflectanceTransmittance(nullptr, intersection, Complex(1), Complex(n), std::cos(thetaI), 550, reflectance, transmittance);

            EXPECT_NEAR(reflectance, Float(0.5) * amplitudeS * amplitudeS, tolerances::STANDARD);
        }

        TEST_F(FresnelTest, DielectricConservesEnergy) {
            for (Float cosThetaI = Float(0.05); cosThetaI <= 1; cosThetaI += Float(0.05)) {
                Float reflectance = 0;
                Float transmittance = 0;

                Material::fresnelReflectanceTransmittance(nullptr, intersection, Complex(1), Complex(Float(1.5)), cosThetaI, 550, reflectance, transmittance);

                EXPECT_NEAR(reflectance + transmittance, 1, tolerances::ACCUMULATED);

                Material::fresnelReflectanceTransmittance(nullptr, intersection, Complex(Float(1.5)), Complex(1), cosThetaI, 550, reflectance, transmittance);

                EXPECT_NEAR(reflectance + transmittance, 1, tolerances::ACCUMULATED);
            }
        }

        TEST_F(FresnelTest, ReflectanceGrowsTowardGrazingIncidence) {
            Float previous = 0;

            for (Float cosThetaI = 1; cosThetaI > Float(0.02); cosThetaI -= Float(0.02)) {
                Float reflectance = 0;
                Float transmittance = 0;

                Material::fresnelReflectanceTransmittance(nullptr, intersection, Complex(1), Complex(Float(1.5)), cosThetaI, 550, reflectance, transmittance);

                EXPECT_GE(reflectance, previous);

                previous = reflectance;
            }

            EXPECT_GT(previous, Float(0.5));
        }

        TEST_F(FresnelTest, ReflectanceIsSymmetricAcrossInterface) {
            Float cosThetaI = std::cos(constants::PI / 4);
            Float cosThetaT = std::sqrt(1 - (1 - cosThetaI * cosThetaI) / (Float(1.5) * Float(1.5)));

            Float entering = 0;
            Float leaving = 0;
            Float transmittance = 0;

            Material::fresnelReflectanceTransmittance(nullptr, intersection, Complex(1), Complex(Float(1.5)), cosThetaI, 550, entering, transmittance);
            Material::fresnelReflectanceTransmittance(nullptr, intersection, Complex(Float(1.5)), Complex(1), cosThetaT, 550, leaving, transmittance);

            EXPECT_NEAR(entering, leaving, tolerances::STANDARD);
        }

        TEST_F(FresnelTest, QuarterWaveCoatingCancelsReflectionAtNormalIncidence) {
            Float n = std::sqrt(Float(1.5));
            Float lambda = 550;

            Material coating = makeCoating(n, lambda / (4 * n));

            Float reflectance = 0;
            Float transmittance = 0;

            Material::fresnelReflectanceTransmittance(&coating, intersection, Complex(1), Complex(Float(1.5)), 1, lambda, reflectance, transmittance);

            EXPECT_NEAR(reflectance, 0, tolerances::STANDARD);
            EXPECT_NEAR(transmittance, 1, tolerances::STANDARD);
        }

        TEST_F(FresnelTest, ThinFilmMatchesAiryFormula) {
            Float n0 = 1;
            Float n1 = Float(1.38);
            Float n2 = Float(1.5);
            Float lambda = 550;

            for (Float thickness = 20; thickness <= 300; thickness += 40) {
                Material coating = makeCoating(n1, thickness);

                Float reflectance = 0;
                Float transmittance = 0;

                Material::fresnelReflectanceTransmittance(&coating, intersection, Complex(n0), Complex(n2), 1, lambda, reflectance, transmittance);

                Complex r01((n0 - n1) / (n0 + n1));
                Complex r12((n1 - n2) / (n1 + n2));
                Complex phase = exp(Complex(0, 4 * constants::PI * n1 * thickness / lambda));

                Float expected = abs((r01 + r12 * phase) / (Complex(1) + r01 * r12 * phase));

                EXPECT_NEAR(reflectance, expected * expected, tolerances::ACCUMULATED);
                EXPECT_NEAR(reflectance + transmittance, 1, tolerances::ACCUMULATED);
            }
        }

        TEST_F(FresnelTest, ThinFilmRepeatsWithHalfWavelengthThickness) {
            Float n = Float(1.38);
            Float lambda = 550;

            Material thin = makeCoating(n, 100);
            Material thick = makeCoating(n, 100 + lambda / (2 * n));

            Float thinReflectance = 0;
            Float thickReflectance = 0;
            Float transmittance = 0;

            Material::fresnelReflectanceTransmittance(&thin, intersection, Complex(1), Complex(Float(1.5)), 1, lambda, thinReflectance, transmittance);
            Material::fresnelReflectanceTransmittance(&thick, intersection, Complex(1), Complex(Float(1.5)), 1, lambda, thickReflectance, transmittance);

            EXPECT_NEAR(thinReflectance, thickReflectance, tolerances::ACCUMULATED);
        }

        TEST_F(FresnelTest, VanishingThinFilmMatchesBareInterface) {
            Material coating = makeCoating(Float(1.38), Float(1e-3));

            Float coated = 0;
            Float bare = 0;
            Float transmittance = 0;

            Material::fresnelReflectanceTransmittance(&coating, intersection, Complex(1), Complex(Float(1.5)), 1, 550, coated, transmittance);
            Material::fresnelReflectanceTransmittance(nullptr, intersection, Complex(1), Complex(Float(1.5)), 1, 550, bare, transmittance);

            EXPECT_NEAR(coated, bare, tolerances::ACCUMULATED);
        }

        class FluorophoreTest : public RandomizedTest {
            protected:
                Fluorophore fluorophore;

                void SetUp() override { fluorophore = Fluorophore(1, 1, allocator.make(Float(400)), allocator.make(Float(500)), allocator.make(Float(400)), allocator.make(Float(400)), allocator.make(Float(0)), allocator.make(Float(1))); }
        };

        TEST_F(FluorophoreTest, DefaultIsInert) {
            Fluorophore inert;

            EXPECT_EQ(inert.inShift(500), 0);
            EXPECT_EQ(inert.outShift(400), 0);
            EXPECT_EQ(inert.getMaxOutShift(), 0);
            EXPECT_EQ(inert.evaluate(400, 500, Float(0.3)), 0);

            Float probabilityDensity = 0;

            EXPECT_EQ(inert.sample(500, Float(0.3), probabilityDensity, state), 500);
            EXPECT_EQ(probabilityDensity, 1);
        }

        TEST_F(FluorophoreTest, MaxOutShiftBoundsNumericMaximum) {
            Float numericMax = 0;

            for (Float lambda = 300; lambda <= 500; lambda += Float(0.1)) numericMax = std::fmax(numericMax, fluorophore.outShift(lambda));

            Float maxOutShift = fluorophore.getMaxOutShift();

            EXPECT_LE(numericMax, maxOutShift);
            EXPECT_LE(maxOutShift, numericMax * Float(1.01));
        }

        TEST_F(FluorophoreTest, MaxOutShiftBoundsMultipleComponents) {
            Float lambdasExcitation[2] = { 400, 450 };
            Float lambdasEmission[2] = { 500, 550 };
            Float variancesExcitation[2] = { 400, 200 };
            Float variancesEmission[2] = { 400, 300 };
            Float covariances[2] = { 0, 0 };
            Float weights[2] = { Float(0.6), Float(0.4) };

            Fluorophore mixture(1, 2, lambdasExcitation, lambdasEmission, variancesExcitation, variancesEmission, covariances, weights);

            Float numericMax = 0;

            for (Float lambda = 300; lambda <= 550; lambda += Float(0.1)) numericMax = std::fmax(numericMax, mixture.outShift(lambda));

            EXPECT_LE(numericMax, mixture.getMaxOutShift());
            EXPECT_LE(mixture.getMaxOutShift(), numericMax * Float(1.01));
        }

        TEST_F(FluorophoreTest, InShiftAndOutShiftConserveEnergy) {
            Float inSum = 0;
            Float outSum = 0;

            for (Float lambda = 200; lambda <= 800; lambda += Float(0.5)) {
                inSum += fluorophore.inShift(lambda);
                outSum += fluorophore.outShift(lambda);
            }

            EXPECT_NEAR(inSum, outSum, tolerances::STANDARD);
        }

        TEST_F(FluorophoreTest, ReflectancePlusOutShiftStaysBelowOne) {
            Float reflectance = Float(0.3);

            for (Float lambda = 200; lambda <= 800; lambda += 10) EXPECT_LE(reflectance + fluorophore.outShift(lambda), 1);

            EXPECT_LE(reflectance + fluorophore.getMaxOutShift(), 1);
        }

        TEST_F(FluorophoreTest, ScaleMultipliesShifts) {
            Fluorophore doubled(2, 1, allocator.make(Float(400)), allocator.make(Float(500)), allocator.make(Float(400)), allocator.make(Float(400)), allocator.make(Float(0)), allocator.make(Float(1)));

            for (Float lambda = 300; lambda <= 600; lambda += 25) {
                EXPECT_NEAR(doubled.inShift(lambda), 2 * fluorophore.inShift(lambda), tolerances::STANDARD);
                EXPECT_NEAR(doubled.outShift(lambda), 2 * fluorophore.outShift(lambda), tolerances::STANDARD);
            }

            EXPECT_NEAR(doubled.getMaxOutShift(), 2 * fluorophore.getMaxOutShift(), tolerances::STANDARD);
        }

        TEST_F(FluorophoreTest, SplitWeightsMatchSingleComponent) {
            Float lambdasExcitation[2] = { 400, 400 };
            Float lambdasEmission[2] = { 500, 500 };
            Float variances[2] = { 400, 400 };
            Float covariances[2] = { 0, 0 };
            Float weights[2] = { Float(0.5), Float(0.5) };

            Fluorophore split(1, 2, lambdasExcitation, lambdasEmission, variances, variances, covariances, weights);

            for (Float lambda = 300; lambda <= 600; lambda += 25) {
                EXPECT_NEAR(split.inShift(lambda), fluorophore.inShift(lambda), tolerances::STANDARD);
                EXPECT_NEAR(split.outShift(lambda), fluorophore.outShift(lambda), tolerances::STANDARD);
                EXPECT_NEAR(split.evaluate(lambda, lambda + 50, Float(0.3)), fluorophore.evaluate(lambda, lambda + 50, Float(0.3)), tolerances::STANDARD);
            }
        }

        TEST_F(FluorophoreTest, EvaluateMatchesAnalyticJointDensity) {
            Float lambdaExcitation = 410;
            Float lambdaEmission = 510;
            Float reflectance = Float(0.3);

            Float expected = std::exp(Float(-0.5) * 100 / 400) / std::sqrt(2 * constants::PI * 400) * std::exp(Float(-0.5) * 100 / 400) / std::sqrt(2 * constants::PI * 400);

            EXPECT_NEAR(fluorophore.evaluate(lambdaExcitation, lambdaEmission, reflectance), expected, tolerances::FINE);
        }

        TEST_F(FluorophoreTest, EvaluateMatchesCorrelatedJointDensity) {
            Float covariance = 100;

            Fluorophore correlated(1, 1, allocator.make(Float(400)), allocator.make(Float(500)), allocator.make(Float(400)), allocator.make(Float(400)), allocator.make(covariance), allocator.make(Float(1)));

            Float dx = 10;
            Float de = 10;

            Float determinant = 400 * 400 - covariance * covariance;
            Float quadratic = (dx * dx * 400 - 2 * dx * de * covariance + de * de * 400) / determinant;

            Float expected = std::exp(Float(-0.5) * quadratic) / (2 * constants::PI * std::sqrt(determinant));

            EXPECT_NEAR(correlated.evaluate(410, 510, Float(0.3)), expected, tolerances::FINE);
        }

        TEST_F(FluorophoreTest, EvaluateAtEqualWavelengthsIsReflectance) {
            EXPECT_EQ(fluorophore.evaluate(500, 500, Float(0.3)), Float(0.3));
        }

        TEST_F(FluorophoreTest, EvaluateRejectsBlueShiftedAndNonPositiveExcitation) {
            EXPECT_EQ(fluorophore.evaluate(520, 500, Float(0.3)), 0);
            EXPECT_EQ(fluorophore.evaluate(0, 500, Float(0.3)), 0);
            EXPECT_EQ(fluorophore.evaluate(-10, 500, Float(0.3)), 0);
        }

        TEST_F(FluorophoreTest, SampleMatchesEvaluatedDensity) {
            Float lambdaEmission = 500;
            Float reflectance = Float(0.3);

            Float expected = reflectance + fluorophore.inShift(lambdaEmission);

            for (int i = 0; i < 1000; i++) {
                Float probabilityDensity, lambdaExcitation = fluorophore.sample(lambdaEmission, reflectance, probabilityDensity, state);

                if (probabilityDensity > 0) {
                    Float evaluatedDensity = fluorophore.evaluate(lambdaExcitation, lambdaEmission, reflectance);

                    EXPECT_NEAR(evaluatedDensity / probabilityDensity, expected, tolerances::STANDARD);
                }
            }
        }

        TEST_F(FluorophoreTest, SampleNeverBlueShifts) {
            for (int i = 0; i < 1000; i++) {
                Float probabilityDensity, lambdaExcitation = fluorophore.sample(500, Float(0.01), probabilityDensity, state);

                if (probabilityDensity > 0) {
                    EXPECT_GT(lambdaExcitation, 0);
                    EXPECT_LE(lambdaExcitation, 500);
                }
            }
        }

        TEST_F(FluorophoreTest, SampleKeepsWavelengthWithReflectanceProbability) {
            Float reflectance = Float(0.01);
            Float lambdaEmission = 500;

            Float expected = reflectance / (reflectance + fluorophore.inShift(lambdaEmission));

            int unshifted = 0;

            for (int i = 0; i < 5000; i++) {
                Float probabilityDensity;

                if (fluorophore.sample(lambdaEmission, reflectance, probabilityDensity, state) == lambdaEmission) unshifted++;
            }

            EXPECT_NEAR(Float(unshifted) / 5000, expected, 5 * std::sqrt(expected * (1 - expected) / 5000));
        }

        class SurfaceTest : public RandomizedTest {
            protected:
                Intersection intersection;

                void SetUp() override {
                    intersection.transformedNormal = Vector<Float, 3>(0, 1, 0);
                    intersection.transformedTangent = Vector<Float, 3>(1, 0, 0);
                    intersection.geometricNormal = intersection.transformedNormal;
                    intersection.frontFacing = true;
                    intersection.useTransformed = true;
                    intersection.isSurface = true;
                }
        };

        class TextureTest : public SurfaceTest {};

        TEST_F(TextureTest, ScalarConstant) {
            ScalarTexture texture = ScalarTexture::makeConstant(Float(0.7));

            EXPECT_EQ(texture.evaluate(intersection), Float(0.7));
            EXPECT_EQ(texture.getMin(), Float(0.7));
            EXPECT_EQ(texture.getMax(), Float(0.7));
            EXPECT_EQ(texture.getAverage(), Float(0.7));
            EXPECT_EQ(texture.getAverage(3, 5), Float(0.7));
            EXPECT_EQ(texture.getWidth(), 1);
            EXPECT_EQ(texture.getHeight(), 1);
        }

        TEST_F(TextureTest, ScalarScaleMultipliesOperands) {
            ScalarTexture texture = ScalarTexture::makeScale(makeScalarTexture(2), makeScalarTexture(3));

            EXPECT_NEAR(texture.evaluate(intersection), 6, tolerances::STANDARD);
        }

        TEST_F(TextureTest, ScalarMixInterpolatesByFactor) {
            ScalarTexture texture = ScalarTexture::makeMix(makeScalarTexture(2), makeScalarTexture(6), makeScalarTexture(Float(0.25)));

            EXPECT_NEAR(texture.evaluate(intersection), 3, tolerances::STANDARD);
        }

        TEST_F(TextureTest, ScalarCheckerAlternatesByCell) {
            ScalarTexture texture = ScalarTexture::makeChecker(1, 2, 2, 2, 0, 0);

            Float coordinates[4][2] = { { Float(0.1), Float(0.1) }, { Float(0.6), Float(0.1) }, { Float(0.6), Float(0.6) }, { Float(-0.1), Float(0.1) } };
            Float expected[4] = { 1, 2, 1, 2 };

            for (int i = 0; i < 4; i++) {
                intersection.textureCoordinate = Vector<Float, 2>(coordinates[i][0], coordinates[i][1]);

                EXPECT_EQ(texture.evaluate(intersection), expected[i]);
            }
        }

        TEST_F(TextureTest, ScalarCheckerAppliesOffset) {
            ScalarTexture texture = ScalarTexture::makeChecker(1, 2, 1, 1, Float(0.5), 0);

            intersection.textureCoordinate = Vector<Float, 2>(Float(0.25), Float(0.25));

            EXPECT_EQ(texture.evaluate(intersection), 1);

            intersection.textureCoordinate = Vector<Float, 2>(Float(0.75), Float(0.25));

            EXPECT_EQ(texture.evaluate(intersection), 2);
        }

        TEST_F(TextureTest, ScalarCheckerStatistics) {
            ScalarTexture texture = ScalarTexture::makeChecker(4, 2, 1, 1, 0, 0);

            EXPECT_EQ(texture.getMin(), 2);
            EXPECT_EQ(texture.getMax(), 4);
            EXPECT_EQ(texture.getAverage(), 3);
            EXPECT_EQ(texture.getWidth(), 1);
            EXPECT_EQ(texture.getHeight(), 1);
        }

        TEST_F(TextureTest, ScalarPerlinSingleOctaveMatchesNoise) {
            ScalarTexture texture = ScalarTexture::makePerlin(2, Float(0.5), 1);

            intersection.transformedPoint = Vector<Float, 3>(Float(0.3), Float(0.6), Float(0.9));

            EXPECT_EQ(texture.evaluate(intersection), utils::perlin(2 * intersection.transformedPoint));
        }

        TEST_F(TextureTest, ScalarPerlinNormalizesOctaves) {
            ScalarTexture texture = ScalarTexture::makePerlin(1, Float(0.5), 4);

            EXPECT_EQ(texture.getMin(), 0);
            EXPECT_EQ(texture.getMax(), 1);
            EXPECT_EQ(texture.getAverage(), Float(0.5));

            for (Float x = 0; x < 3; x += Float(0.37)) {
                intersection.transformedPoint = Vector<Float, 3>(x, Float(0.4), Float(0.2));

                EXPECT_GE(texture.evaluate(intersection), 0);
                EXPECT_LE(texture.evaluate(intersection), 1);
            }
        }

        TEST_F(TextureTest, ScalarPerlinHonorsPointSelection) {
            ScalarTexture texture = ScalarTexture::makePerlin(1, Float(0.5), 1);

            intersection.point = Vector<Float, 3>(Float(0.3), Float(0.6), Float(0.9));
            intersection.transformedPoint = Vector<Float, 3>(Float(1.7), Float(2.2), Float(0.4));

            intersection.useTransformed = true;

            EXPECT_EQ(texture.evaluate(intersection), utils::perlin(intersection.transformedPoint));

            intersection.useTransformed = false;

            EXPECT_EQ(texture.evaluate(intersection), utils::perlin(intersection.point));
        }

        TEST_F(TextureTest, ScalarWorleySingleOctaveMatchesNoise) {
            ScalarTexture texture = ScalarTexture::makeWorley(3, Float(0.5), 1);

            intersection.transformedPoint = Vector<Float, 3>(Float(0.3), Float(0.6), Float(0.9));

            EXPECT_EQ(texture.evaluate(intersection), utils::worley(3 * intersection.transformedPoint));
        }

        TEST_F(TextureTest, ScalarWorleyHonorsPointSelection) {
            ScalarTexture texture = ScalarTexture::makeWorley(1, Float(0.5), 1);

            intersection.point = Vector<Float, 3>(Float(0.3), Float(0.6), Float(0.9));
            intersection.transformedPoint = Vector<Float, 3>(Float(1.7), Float(2.2), Float(0.4));

            intersection.useTransformed = false;

            EXPECT_EQ(texture.evaluate(intersection), utils::worley(intersection.point));
        }

        TEST_F(TextureTest, ScalarImageStatistics) {
            Float values[4] = { 0, 1, 2, 3 };

            ScalarTexture texture = ScalarTexture::makeImage(values, 2, 2, 1, 1, 0, 0);

            EXPECT_EQ(texture.getMin(), 0);
            EXPECT_EQ(texture.getMax(), 3);
            EXPECT_EQ(texture.getAverage(), Float(1.5));
            EXPECT_EQ(texture.getWidth(), 2);
            EXPECT_EQ(texture.getHeight(), 2);
        }

        TEST_F(TextureTest, ScalarImageReproducesTexelCenters) {
            Float values[4] = { 0, 1, 2, 3 };

            ScalarTexture texture = ScalarTexture::makeImage(values, 2, 2, 1, 1, 0, 0);

            for (int y = 0; y < 2; y++)
                for (int x = 0; x < 2; x++) {
                    intersection.textureCoordinate = Vector<Float, 2>((Float(x) + Float(0.5)) / 2, (Float(y) + Float(0.5)) / 2);

                    EXPECT_NEAR(texture.evaluate(intersection), values[y * 2 + x], tolerances::STANDARD);
                }
        }

        TEST_F(TextureTest, ScalarImageInterpolatesBilinearly) {
            Float values[4] = { 0, 1, 2, 3 };

            ScalarTexture texture = ScalarTexture::makeImage(values, 2, 2, 1, 1, 0, 0);

            intersection.textureCoordinate = Vector<Float, 2>(Float(0.5), Float(0.5));

            EXPECT_NEAR(texture.evaluate(intersection), Float(1.5), tolerances::STANDARD);
        }

        TEST_F(TextureTest, ScalarImageWrapsHorizontallyAndClampsVertically) {
            Float values[4] = { 0, 1, 2, 3 };

            ScalarTexture texture = ScalarTexture::makeImage(values, 2, 2, 1, 1, 0, 0);

            intersection.textureCoordinate = Vector<Float, 2>(Float(1.25), Float(0.25));

            EXPECT_NEAR(texture.evaluate(intersection), 0, tolerances::STANDARD);

            intersection.textureCoordinate = Vector<Float, 2>(Float(-0.25), Float(0.25));

            EXPECT_NEAR(texture.evaluate(intersection), 1, tolerances::STANDARD);

            intersection.textureCoordinate = Vector<Float, 2>(Float(0.25), -5);

            EXPECT_NEAR(texture.evaluate(intersection), 0, tolerances::STANDARD);

            intersection.textureCoordinate = Vector<Float, 2>(Float(0.25), 5);

            EXPECT_NEAR(texture.evaluate(intersection), 2, tolerances::STANDARD);
        }

        TEST_F(TextureTest, ScalarImageTransformsCoordinates) {
            Float values[4] = { 0, 1, 2, 3 };

            ScalarTexture texture = ScalarTexture::makeImage(values, 2, 2, 2, 1, 0, 0);

            intersection.textureCoordinate = Vector<Float, 2>(Float(0.125), Float(0.25));

            EXPECT_NEAR(texture.evaluate(intersection), 0, tolerances::STANDARD);

            EXPECT_EQ(texture.getWidth(), 1);
            EXPECT_EQ(texture.getHeight(), 1);
            EXPECT_EQ(texture.getAverage(1, 1), Float(1.5));
        }

        TEST_F(TextureTest, ScalarImageTexelAverageWraps) {
            Float values[4] = { 0, 1, 2, 3 };

            ScalarTexture texture = ScalarTexture::makeImage(values, 2, 2, 1, 1, 0, 0);

            EXPECT_EQ(texture.getAverage(1, 0), 1);
            EXPECT_EQ(texture.getAverage(-1, 0), 1);
            EXPECT_EQ(texture.getAverage(2, 1), 2);
            EXPECT_EQ(texture.getAverage(0, -1), 2);
        }

        TEST_F(TextureTest, SpectrumConstantEvaluatesSpectrum) {
            Float lambdas[2] = { 400, 600 };
            Float values[2] = { 1, 3 };

            SpectrumTexture texture = SpectrumTexture::makeConstant(allocator.make(LinearSpectrum<Float>(2, lambdas, values)));

            EXPECT_NEAR(texture.evaluate(intersection, 500), 2, tolerances::STANDARD);
            EXPECT_EQ(texture.evaluate(intersection, 700), 0);
            EXPECT_EQ(texture.getMin(), 0);
            EXPECT_EQ(texture.getMax(), 3);
            EXPECT_NEAR(texture.getAverage(), 2, tolerances::STANDARD);
            EXPECT_EQ(texture.getWidth(), 1);
            EXPECT_EQ(texture.getHeight(), 1);
        }

        TEST_F(TextureTest, SpectrumConstantExtremaIncludeZero) {
            SpectrumTexture positive = SpectrumTexture::makeConstant(makeSpectrum(2));
            SpectrumTexture negative = SpectrumTexture::makeConstant(makeSpectrum(-2));

            EXPECT_EQ(positive.getMin(), 0);
            EXPECT_EQ(positive.getMax(), 2);
            EXPECT_EQ(negative.getMin(), -2);
            EXPECT_EQ(negative.getMax(), 0);
        }

        TEST_F(TextureTest, SpectrumScalarIgnoresWavelength) {
            SpectrumTexture texture = SpectrumTexture::makeScalar(makeScalarTexture(Float(0.4)));

            EXPECT_EQ(texture.evaluate(intersection, 400), Float(0.4));
            EXPECT_EQ(texture.evaluate(intersection, 700), Float(0.4));
            EXPECT_EQ(texture.getAverage(), Float(0.4));
        }

        TEST_F(TextureTest, SpectrumScaleMultipliesOperands) {
            SpectrumTexture texture = SpectrumTexture::makeScale(makeSpectrumTexture(2), makeSpectrumTexture(Float(1.5)));

            EXPECT_NEAR(texture.evaluate(intersection, 550), 3, tolerances::STANDARD);
        }

        TEST_F(TextureTest, SpectrumMixInterpolatesByFactor) {
            SpectrumTexture texture = SpectrumTexture::makeMix(makeSpectrumTexture(2), makeSpectrumTexture(6), makeScalarTexture(Float(0.75)));

            EXPECT_NEAR(texture.evaluate(intersection, 550), 5, tolerances::STANDARD);
        }

        TEST_F(TextureTest, SpectrumCheckerAlternatesByCell) {
            SpectrumTexture texture = SpectrumTexture::makeChecker(makeSpectrum(1), makeSpectrum(2), 2, 2, 0, 0);

            intersection.textureCoordinate = Vector<Float, 2>(Float(0.1), Float(0.1));

            EXPECT_EQ(texture.evaluate(intersection, 550), 1);

            intersection.textureCoordinate = Vector<Float, 2>(Float(0.6), Float(0.1));

            EXPECT_EQ(texture.evaluate(intersection, 550), 2);

            intersection.textureCoordinate = Vector<Float, 2>(Float(-0.1), Float(0.1));

            EXPECT_EQ(texture.evaluate(intersection, 550), 2);
        }

        TEST_F(TextureTest, SpectrumImageWithoutUpsamplingIsHalfScale) {
            ColorSpace space(constants::CHROMATICITIES_SRGB);

            Float values[3] = { Float(0.4), Float(0.4), Float(0.4) };

            SpectrumTexture texture = SpectrumTexture::makeImage(values, &space, 1, 1, 1, 1, 0, 0);

            EXPECT_NEAR(texture.evaluate(intersection, 550), Float(0.5), tolerances::STANDARD);
        }

        TEST_F(TextureTest, SpectrumImagePreservesHighDynamicRange) {
            ColorSpace space(constants::CHROMATICITIES_SRGB);

            Float values[3] = { 4, 2, 1 };

            SpectrumTexture texture = SpectrumTexture::makeImage(values, &space, 1, 1, 1, 1, 0, 0);

            EXPECT_NEAR(texture.evaluate(intersection, 550), 2, tolerances::STANDARD);
        }

        TEST_F(TextureTest, SpectrumImageStatistics) {
            ColorSpace space(constants::CHROMATICITIES_SRGB);

            Float values[6] = { 0, 1, 2, 3, 4, -1 };

            SpectrumTexture texture = SpectrumTexture::makeImage(values, &space, 2, 1, 1, 1, 0, 0);

            EXPECT_EQ(texture.getMin(), 0);
            EXPECT_EQ(texture.getMax(), 4);
            EXPECT_NEAR(texture.getAverage(), Float(9) / 6, tolerances::STANDARD);
            EXPECT_EQ(texture.getWidth(), 2);
            EXPECT_EQ(texture.getHeight(), 1);
            EXPECT_NEAR(texture.getAverage(0, 0), 1, tolerances::STANDARD);
            EXPECT_NEAR(texture.getAverage(1, 0), Float(2) , tolerances::STANDARD);
        }

        class MaterialTest : public SurfaceTest {
            protected:
                Ray rIn;

                void SetUp() override {
                    SurfaceTest::SetUp();

                    rIn = Ray(Vector<Float, 3>(0, 1, 1), normalize(Vector<Float, 3>(0, -1, -1)), SampledSpectrum<Float>(550), nullptr);
                }

                Fluorophore makeFluorophore(Float lambdaExcitation, Float lambdaEmission, Float fullWidthHalfMaximum, Float strength = Float(0.6)) {
                    Float variance = fullWidthHalfMaximum * fullWidthHalfMaximum / (8 * std::log(Float(2)));
                    Float shiftProbability = Float(0.5) * std::erfc((lambdaExcitation - lambdaEmission) / std::sqrt(4 * variance));

                    return Fluorophore(strength * std::sqrt(2 * constants::PI * variance) / shiftProbability, 1, allocator.make(lambdaExcitation), allocator.make(lambdaEmission), allocator.make(variance), allocator.make(variance), allocator.make(Float(0)), allocator.make(Float(1)));
                }

                Material makeDiffuse(Float reflectance, Fluorophore * fluorophore = nullptr) { return Material::makeDiffuse(makeSpectrumTexture(reflectance), fluorophore); }

                Material makeFresnel(const Complex & n0, const Complex & n1, Float alpha) { return Material::makeFresnel(makeComplexSpectrum(n0), makeComplexSpectrum(n1), makeScalarTexture(alpha), makeScalarTexture(alpha)); }
        };

        TEST_F(MaterialTest, DiffuseTypeQueries) {
            Material material = makeDiffuse(Float(0.5));

            EXPECT_FALSE(material.isIdealFresnel());
            EXPECT_FALSE(material.isEmissive());
            EXPECT_FALSE(material.isSpecular(intersection));
            EXPECT_EQ(material.averageEmission(), 0);
        }

        TEST_F(MaterialTest, FresnelTypeQueries) {
            Material smooth = makeFresnel(Complex(1), Complex(Float(1.5)), 0);
            Material rough = makeFresnel(Complex(1), Complex(Float(1.5)), Float(0.3));

            EXPECT_TRUE(smooth.isIdealFresnel());
            EXPECT_FALSE(smooth.isEmissive());
            EXPECT_EQ(smooth.averageEmission(), 0);

            EXPECT_TRUE(smooth.isSpecular(intersection));
            EXPECT_FALSE(rough.isSpecular(intersection));
        }

        TEST_F(MaterialTest, FresnelIsSpecularWhenEitherRoughnessIsSmooth) {
            Material material = Material::makeFresnel(makeComplexSpectrum(Complex(1)), makeComplexSpectrum(Complex(Float(1.5))), makeScalarTexture(Float(0.3)), makeScalarTexture(0));

            EXPECT_TRUE(material.isSpecular(intersection));
        }

        TEST_F(MaterialTest, CoatedTypeQueries) {
            Material smooth = makeFresnel(Complex(1), Complex(Float(1.5)), 0);
            Material rough = makeFresnel(Complex(1), Complex(Float(1.5)), Float(0.3));

            Material smoothCoated = Material::makeCoated(1, allocator.make(makeComplexSpectrum(Complex(Float(1.3)))), allocator.make(makeScalarTexture(100)), &smooth);
            Material roughCoated = Material::makeCoated(1, allocator.make(makeComplexSpectrum(Complex(Float(1.3)))), allocator.make(makeScalarTexture(100)), &rough);

            EXPECT_FALSE(smoothCoated.isIdealFresnel());
            EXPECT_FALSE(smoothCoated.isEmissive());
            EXPECT_EQ(smoothCoated.averageEmission(), 0);

            EXPECT_TRUE(smoothCoated.isSpecular(intersection));
            EXPECT_FALSE(roughCoated.isSpecular(intersection));
        }

        TEST_F(MaterialTest, EmissiveTypeQueries) {
            Material material = Material::makeEmissive(makeSpectrumTexture(3));

            EXPECT_TRUE(material.isEmissive());
            EXPECT_FALSE(material.isIdealFresnel());
            EXPECT_FALSE(material.isSpecular(intersection));
            EXPECT_NEAR(material.averageEmission(), 3, tolerances::STANDARD);
        }

        TEST_F(MaterialTest, EmissiveEmitsEvaluatedSpectrum) {
            Material material = Material::makeEmissive(makeSpectrumTexture(3));

            SampledSpectrum<Float> emission = material.emission(intersection, rIn, false);

            for (int i = 0; i < constants::HERO_COUNT; i++) EXPECT_EQ(emission[i], 3);
        }

        TEST_F(MaterialTest, EmissiveCollapsedEmitsOnlyFirstLane) {
            Material material = Material::makeEmissive(makeSpectrumTexture(3));

            SampledSpectrum<Float> emission = material.emission(intersection, rIn, true);

            EXPECT_EQ(emission[0], 3);

            for (int i = 1; i < constants::HERO_COUNT; i++) EXPECT_EQ(emission[i], 0);
        }

        TEST_F(MaterialTest, NonEmissiveMaterialsDoNotEmit) {
            Material diffuse = makeDiffuse(Float(0.5));
            Material fresnel = makeFresnel(Complex(1), Complex(Float(1.5)), 0);

            EXPECT_EQ(diffuse.emission(intersection, rIn, false), SampledSpectrum<Float>());
            EXPECT_EQ(fresnel.emission(intersection, rIn, false), SampledSpectrum<Float>());
        }

        TEST_F(MaterialTest, EmissiveDoesNotScatter) {
            Material material = Material::makeEmissive(makeSpectrumTexture(3));

            Ray rOut = rIn;

            Float radianceScale = 1;
            bool collapsed = false;

            SampledSpectrum<Float> probabilityDensity, attenuation = material.sample(intersection, rOut, nullptr, probabilityDensity, radianceScale, collapsed, state);

            EXPECT_EQ(attenuation, SampledSpectrum<Float>());

            SampledSpectrum<Float> evaluated = material.evaluate(intersection, rIn, rIn, probabilityDensity, collapsed);

            EXPECT_EQ(evaluated, SampledSpectrum<Float>());
        }

        TEST_F(MaterialTest, SampleLambdasWithoutFluorophoreIsIdentity) {
            Material materials[2] = { makeDiffuse(Float(0.5)), makeFresnel(Complex(1), Complex(Float(1.5)), 0) };

            SampledSpectrum<Float> lambdas;

            for (int i = 0; i < constants::HERO_COUNT; i++) lambdas[i] = Float(450 + 50 * i);

            for (const Material & material : materials) {
                SampledSpectrum<Float> probabilityDensity(1);

                EXPECT_EQ(material.sampleLambdas(intersection, lambdas, probabilityDensity, false, state), lambdas);
            }
        }

        TEST_F(MaterialTest, DiffuseEvaluateUsesLambertianDensity) {
            Material material = makeDiffuse(Float(0.8));

            Ray rOut(Vector<Float, 3>(), normalize(Vector<Float, 3>(0, 1, 1)), SampledSpectrum<Float>(550), nullptr);

            bool collapsed = false;

            SampledSpectrum<Float> probabilityDensity, attenuation = material.evaluate(intersection, rIn, rOut, probabilityDensity, collapsed);

            Float cosTheta = dot(intersection.transformedNormal, rOut.getDirection());

            for (int i = 0; i < constants::HERO_COUNT; i++) {
                EXPECT_NEAR(probabilityDensity[i], cosTheta / constants::PI, tolerances::STANDARD);
                EXPECT_NEAR(attenuation[i], Float(0.8) / constants::PI, tolerances::STANDARD);
            }
        }

        TEST_F(MaterialTest, DiffuseEvaluateBelowSurfaceIsZero) {
            Material material = makeDiffuse(Float(0.8));

            Ray rOut(Vector<Float, 3>(), Vector<Float, 3>(0, -1, 0), SampledSpectrum<Float>(550), nullptr);

            bool collapsed = false;

            SampledSpectrum<Float> probabilityDensity(1), attenuation = material.evaluate(intersection, rIn, rOut, probabilityDensity, collapsed);

            EXPECT_EQ(attenuation, SampledSpectrum<Float>());
            EXPECT_EQ(probabilityDensity, SampledSpectrum<Float>());
        }

        TEST_F(MaterialTest, DiffuseEvaluateCollapsedFillsFirstLaneOnly) {
            Material material = makeDiffuse(Float(0.8));

            Ray rOut(Vector<Float, 3>(), Vector<Float, 3>(0, 1, 0), SampledSpectrum<Float>(550), nullptr);

            SampledSpectrum<Float> probabilityDensity, attenuation = material.evaluate(intersection, rIn, rOut, probabilityDensity, true);

            EXPECT_GT(attenuation[0], 0);

            for (int i = 1; i < constants::HERO_COUNT; i++) EXPECT_EQ(attenuation[i], 0);
        }

        TEST_F(MaterialTest, DiffuseSamplingDoesNotCollapseOrRescaleRadiance) {
            Material material = makeDiffuse(Float(0.8));

            for (int i = 0; i < 1000; i++) {
                Ray rOut = rIn;

                Float radianceScale = 1;
                bool collapsed = false;

                SampledSpectrum<Float> probabilityDensity;

                material.sample(intersection, rOut, nullptr, probabilityDensity, radianceScale, collapsed, state);

                EXPECT_FALSE(collapsed);
                EXPECT_EQ(radianceScale, 1);
            }
        }

        TEST_F(MaterialTest, DiffuseSampleWeightsAreFiniteAndNonNegative) {
            Material material = makeDiffuse(Float(0.8));

            for (int i = 0; i < 1000; i++) {
                Ray rOut = rIn;

                Float radianceScale = 1;
                bool collapsed = false;

                SampledSpectrum<Float> probabilityDensity, attenuation = material.sample(intersection, rOut, nullptr, probabilityDensity, radianceScale, collapsed, state);

                for (int j = 0; j < constants::HERO_COUNT; j++) {
                    EXPECT_TRUE(std::isfinite(attenuation[j]));
                    EXPECT_GE(attenuation[j], 0);
                }
            }
        }

        TEST_F(MaterialTest, DiffuseSampleDensityMatchesEvaluatedDensity) {
            Material material = makeDiffuse(Float(0.8));

            for (int i = 0; i < 1000; i++) {
                Ray rOut = rIn;

                Float radianceScale = 1;
                bool collapsed = false;

                SampledSpectrum<Float> probabilityDensity, evaluatedDensity;

                material.sample(intersection, rOut, nullptr, probabilityDensity, radianceScale, collapsed, state);

                ASSERT_GT(probabilityDensity[0], 0);

                material.evaluate(intersection, rIn, rOut, evaluatedDensity, collapsed);

                EXPECT_NEAR(probabilityDensity[0], evaluatedDensity[0], tolerances::STANDARD);
            }
        }

        TEST_F(MaterialTest, DiffuseSamplingIsCosineWeightedAboveSurface) {
            Material material = makeDiffuse(Float(0.8));

            Float sum = 0;

            for (int i = 0; i < 5000; i++) {
                Ray rOut = rIn;

                Float radianceScale = 1;
                bool collapsed = false;

                SampledSpectrum<Float> probabilityDensity;

                material.sample(intersection, rOut, nullptr, probabilityDensity, radianceScale, collapsed, state);

                Float cosTheta = dot(intersection.transformedNormal, rOut.getDirection());

                ASSERT_GT(cosTheta, 0);

                sum += cosTheta;
            }

            EXPECT_NEAR(sum / 5000, Float(2) / 3, tolerances::STATISTICAL);
        }

        TEST_F(MaterialTest, DiffuseSamplingStartsAtSurfacePointWithSameLambdas) {
            Material material = makeDiffuse(Float(0.8));

            intersection.transformedPoint = Vector<Float, 3>(1, 2, 3);

            Ray rOut = rIn;

            Float radianceScale = 1;
            bool collapsed = false;

            SampledSpectrum<Float> probabilityDensity;

            material.sample(intersection, rOut, nullptr, probabilityDensity, radianceScale, collapsed, state);

            expectNear(rOut.getOrigin(), intersection.transformedPoint, tolerances::COARSE);

            EXPECT_EQ(rOut.getLambdas(), rIn.getLambdas());
        }

        TEST_F(MaterialTest, DiffuseSampleWeightEqualsReflectance) {
            Material material = makeDiffuse(Float(0.25));

            for (int i = 0; i < 1000; i++) {
                Ray rOut = rIn;

                Float radianceScale = 1;
                bool collapsed = false;

                SampledSpectrum<Float> probabilityDensity, attenuation = material.sample(intersection, rOut, nullptr, probabilityDensity, radianceScale, collapsed, state);

                ASSERT_GT(probabilityDensity[0], 0);

                EXPECT_NEAR(attenuation[0], Float(0.25), tolerances::STANDARD);
            }
        }

        TEST_F(MaterialTest, FluorescentSampleWeightIsReflectancePlusInShift) {
            Fluorophore fluorophore = makeFluorophore(400, 500, 40);

            Material material = makeDiffuse(Float(0.3), &fluorophore);

            int weighted = 0;

            for (int i = 0; i < 3; i++) {
                Ray rLambda(Vector<Float, 3>(0, 1, 1), normalize(Vector<Float, 3>(0, -1, -1)), SampledSpectrum<Float>(Float(400 + i * 50)), nullptr);

                Float expected = Float(0.3) + fluorophore.inShift(rLambda.getLambdas()[0]);

                for (int j = 0; j < 1000; j++) {
                    Ray rOut = rLambda;

                    Float radianceScale = 1;
                    bool collapsed = false;

                    SampledSpectrum<Float> probabilityDensity, attenuation = material.sample(intersection, rOut, nullptr, probabilityDensity, radianceScale, collapsed, state);

                    if (probabilityDensity[0] > 0) {
                        EXPECT_NEAR(attenuation[0], expected, tolerances::STANDARD);

                        weighted++;
                    }
                }
            }

            EXPECT_GT(weighted, 0);
        }

        TEST_F(MaterialTest, FluorescentSampleWithoutReflectanceGivesOtherLanesZeroWeight) {
            Fluorophore fluorophore = makeFluorophore(330, 420, 15);

            Material material = makeDiffuse(0, &fluorophore);

            SampledSpectrum<Float> lambdas;

            for (int i = 0; i < constants::HERO_COUNT; i++) lambdas[i] = Float(420 + i * 110);

            Ray rLambda(Vector<Float, 3>(0, 1, 1), normalize(Vector<Float, 3>(0, -1, -1)), lambdas, nullptr);

            Ray rOut = rLambda;

            Float radianceScale = 1;
            bool collapsed = false;

            SampledSpectrum<Float> probabilityDensity, attenuation = material.sample(intersection, rOut, nullptr, probabilityDensity, radianceScale, collapsed, state);

            ASSERT_GT(fluorophore.inShift(lambdas[0]), Float(0.1));

            EXPECT_GT(attenuation[0], 0);

            for (int i = 1; i < constants::HERO_COUNT; i++) EXPECT_EQ(attenuation[i], 0);
        }

        TEST_F(MaterialTest, FluorescentSampleLambdasNeverIncreasesWavelength) {
            Float varianceExcitation = 15 * 15 / (8 * std::log(Float(2)));

            Fluorophore fluorophore(Float(0.6) * std::sqrt(2 * constants::PI * varianceExcitation) / (std::erfc((330 - 420) / std::sqrt(2 * (varianceExcitation + varianceExcitation)))), 1, allocator.make(Float(330)), allocator.make(Float(420)), allocator.make(varianceExcitation), allocator.make(varianceExcitation), allocator.make(Float(0)), allocator.make(Float(1)));

            Material material = makeDiffuse(Float(0.1), &fluorophore);

            SampledSpectrum<Float> lambdas(420);

            for (int i = 0; i < 100; i++) {
                SampledSpectrum<Float> probabilityDensity(1);

                SampledSpectrum<Float> excitation = material.sampleLambdas(intersection, lambdas, probabilityDensity, false, state);

                for (int j = 0; j < constants::HERO_COUNT; j++) EXPECT_LE(excitation[j], lambdas[j]);
            }
        }

        TEST_F(MaterialTest, SmoothDielectricReflectsOrRefractsBySnellsLaw) {
            Material material = makeFresnel(Complex(1), Complex(Float(1.5)), 0);

            Medium medium = Medium::makeHomogeneous(makeSpectrum(0), makeSpectrum(0), 0, nullptr);

            Float sinThetaI = std::sin(constants::PI / 4);
            Float ratio = Float(1) / Float(1.5);

            int reflected = 0;
            int refracted = 0;

            for (int i = 0; i < 1000; i++) {
                Ray rOut = rIn;

                Float radianceScale = 1;
                bool collapsed = false;

                SampledSpectrum<Float> probabilityDensity, attenuation = material.sample(intersection, rOut, &medium, probabilityDensity, radianceScale, collapsed, state);

                Vector<Float, 3> direction = rOut.getDirection();

                if (direction[1] > 0) {
                    reflected++;

                    expectNear(direction, utils::reflected(rIn.getDirection(), intersection.transformedNormal), tolerances::ACCUMULATED);

                    EXPECT_EQ(radianceScale, 1);
                    EXPECT_EQ(rOut.getMedium(), rIn.getMedium());
                    EXPECT_NEAR(attenuation[0], 1, tolerances::STANDARD);
                }
                else {
                    refracted++;

                    EXPECT_NEAR(std::sqrt(direction[0] * direction[0] + direction[2] * direction[2]), ratio * sinThetaI, tolerances::STANDARD);
                    EXPECT_NEAR(radianceScale, ratio * ratio, tolerances::STANDARD);
                    EXPECT_EQ(rOut.getMedium(), &medium);
                    EXPECT_NEAR(attenuation[0], ratio * ratio, tolerances::STANDARD);
                    EXPECT_FALSE(collapsed);
                }
            }

            Float reflectance = 0;
            Float transmittance = 0;

            Material::fresnelReflectanceTransmittance(nullptr, intersection, Complex(1), Complex(Float(1.5)), std::cos(constants::PI / 4), 550, reflectance, transmittance);

            Float expected = reflectance / (reflectance + transmittance);

            EXPECT_NEAR(Float(reflected) / 1000, expected, 5 * std::sqrt(expected * (1 - expected) / 1000));
            EXPECT_GT(refracted, 0);
        }

        TEST_F(MaterialTest, SmoothDielectricReflectsTotallyInsideBeyondCriticalAngle) {
            Material material = makeFresnel(Complex(1), Complex(Float(1.5)), 0);

            intersection.frontFacing = false;

            Ray rInside(Vector<Float, 3>(), Vector<Float, 3>(std::sin(constants::PI / 3), -std::cos(constants::PI / 3), 0), SampledSpectrum<Float>(550), nullptr);

            for (int i = 0; i < 100; i++) {
                Ray rOut = rInside;

                Float radianceScale = 1;
                bool collapsed = false;

                SampledSpectrum<Float> probabilityDensity, attenuation = material.sample(intersection, rOut, nullptr, probabilityDensity, radianceScale, collapsed, state);

                EXPECT_GT(rOut.getDirection()[1], 0);
                EXPECT_NEAR(attenuation[0], 1, tolerances::STANDARD);
            }
        }

        TEST_F(MaterialTest, SmoothConductorAlwaysReflects) {
            Material material = makeFresnel(Complex(1), Complex(Float(0.2), 3), 0);

            Float reflectance = 0;
            Float transmittance = 0;

            Material::fresnelReflectanceTransmittance(nullptr, intersection, Complex(1), Complex(Float(0.2), 3), std::cos(constants::PI / 4), 550, reflectance, transmittance);

            for (int i = 0; i < 100; i++) {
                Ray rOut = rIn;

                Float radianceScale = 1;
                bool collapsed = false;

                SampledSpectrum<Float> probabilityDensity, attenuation = material.sample(intersection, rOut, nullptr, probabilityDensity, radianceScale, collapsed, state);

                EXPECT_GT(rOut.getDirection()[1], 0);
                EXPECT_NEAR(attenuation[0], reflectance, tolerances::STANDARD);
                EXPECT_NEAR(probabilityDensity[0], 1, tolerances::STANDARD);
            }
        }

        TEST_F(MaterialTest, DispersiveRefractionCollapsesWavelengths) {
            Float lambdas[2] = { 400, 700 };
            Complex n1[2] = { Complex(Float(1.6)), Complex(Float(1.5)) };

            Material material = Material::makeFresnel(makeComplexSpectrum(Complex(1)), allocator.make(LinearSpectrum<Complex>(2, lambdas, n1)), makeScalarTexture(0), makeScalarTexture(0));

            SampledSpectrum<Float> hero;

            for (int i = 0; i < constants::HERO_COUNT; i++) hero[i] = Float(450 + 60 * i);

            Ray rDispersive(rIn.getOrigin(), rIn.getDirection(), hero, nullptr);

            int collapsedRefractions = 0;

            for (int i = 0; i < 200; i++) {
                Ray rOut = rDispersive;

                Float radianceScale = 1;
                bool collapsed = false;

                SampledSpectrum<Float> probabilityDensity;

                material.sample(intersection, rOut, nullptr, probabilityDensity, radianceScale, collapsed, state);

                EXPECT_EQ(collapsed, rOut.getDirection()[1] < 0);

                if (collapsed) collapsedRefractions++;
            }

            EXPECT_GT(collapsedRefractions, 0);
        }

        TEST_F(MaterialTest, NonDispersiveRefractionKeepsAllWavelengths) {
            Material material = makeFresnel(Complex(1), Complex(Float(1.5)), 0);

            for (int i = 0; i < 200; i++) {
                Ray rOut = rIn;

                Float radianceScale = 1;
                bool collapsed = false;

                SampledSpectrum<Float> probabilityDensity;

                material.sample(intersection, rOut, nullptr, probabilityDensity, radianceScale, collapsed, state);

                EXPECT_FALSE(collapsed);
            }
        }

        TEST_F(MaterialTest, RoughConductorSampling) {
            Material material = makeFresnel(Complex(1), Complex(Float(0.2), 3), Float(0.3));

            for (int i = 0; i < 1000; i++) {
                Ray rOut = rIn;

                Float radianceScale = 1;
                bool collapsed = false;

                SampledSpectrum<Float> probabilityDensity, attenuation = material.sample(intersection, rOut, nullptr, probabilityDensity, radianceScale, collapsed, state);

                for (int j = 0; j < constants::HERO_COUNT; j++) {
                    EXPECT_FALSE(std::isnan(attenuation[j]));
                    EXPECT_FALSE(std::isinf(attenuation[j]));

                    EXPECT_GE(attenuation[j], 0);
                }

                if (attenuation[0] > 0) {
                    EXPECT_GT(probabilityDensity[0], 0);
                    EXPECT_GT(rOut.getDirection()[1], 0);
                }
            }
        }

        TEST_F(MaterialTest, RoughConductorAlbedoConsistency) {
            Material material = makeFresnel(Complex(1), Complex(Float(0.2), 3), Float(0.3));

            Float sum = 0;

            for (int i = 0; i < 5000; i++) {
                Ray rOut = rIn;

                Float radianceScale = 1;
                bool collapsed = false;

                SampledSpectrum<Float> probabilityDensity, attenuation = material.sample(intersection, rOut, nullptr, probabilityDensity, radianceScale, collapsed, state);

                if (probabilityDensity[0] > 0) sum += attenuation[0];
            }

            Float evaluatedSum = 0;

            for (int i = 0; i < 5000; i++) {
                Float z = 1 - 2 * utils::randomFloat(state);
                Float phi = 2 * constants::PI * utils::randomFloat(state);
                Float r = std::fmax(std::sqrt(1 - z * z), Float(0));

                Ray rOut(Vector<Float, 3>(), Vector<Float, 3>(r * std::cos(phi), z, r * std::sin(phi)), SampledSpectrum<Float>(550), nullptr);

                bool collapsed = false;

                SampledSpectrum<Float> probabilityDensity, attenuation = material.evaluate(intersection, rIn, rOut, probabilityDensity, collapsed);

                evaluatedSum += attenuation[0] * std::fabs(z) * 4 * constants::PI;
            }

            EXPECT_NEAR(sum / 5000, evaluatedSum / 5000, tolerances::STATISTICAL);
        }

        TEST_F(MaterialTest, RoughConductorConservesEnergy) {
            Material material = makeFresnel(Complex(1), Complex(Float(0.2), 3), Float(0.3));

            Float sum = 0;

            for (int i = 0; i < 5000; i++) {
                Ray rOut = rIn;

                Float radianceScale = 1;
                bool collapsed = false;

                SampledSpectrum<Float> probabilityDensity, attenuation = material.sample(intersection, rOut, nullptr, probabilityDensity, radianceScale, collapsed, state);

                if (probabilityDensity[0] > 0) sum += attenuation[0];
            }

            EXPECT_LE(sum / 5000, Float(1.02));
            EXPECT_GT(sum / 5000, Float(0.5));
        }

        TEST_F(MaterialTest, RoughDielectricConservesEnergy) {
            Material material = makeFresnel(Complex(1), Complex(Float(1.5)), Float(0.3));

            Float sum = 0;

            for (int i = 0; i < 5000; i++) {
                Ray rOut = rIn;

                Float radianceScale = 1;
                bool collapsed = false;

                SampledSpectrum<Float> probabilityDensity, attenuation = material.sample(intersection, rOut, nullptr, probabilityDensity, radianceScale, collapsed, state);

                if (probabilityDensity[0] > 0) sum += attenuation[0] / radianceScale;
            }

            EXPECT_LE(sum / 5000, Float(1.03));
            EXPECT_GT(sum / 5000, Float(0.85));
        }

        TEST_F(MaterialTest, CoatedSamplingProducesFiniteNonNegativeWeights) {
            Material substrate = makeFresnel(Complex(1), Complex(Float(1.5)), Float(0.3));

            Material material = Material::makeCoated(1, allocator.make(makeComplexSpectrum(Complex(Float(1.3)))), allocator.make(makeScalarTexture(100)), &substrate);

            for (int i = 0; i < 1000; i++) {
                Ray rOut = rIn;

                Float radianceScale = 1;
                bool collapsed = false;

                SampledSpectrum<Float> probabilityDensity, attenuation = material.sample(intersection, rOut, nullptr, probabilityDensity, radianceScale, collapsed, state);

                for (int j = 0; j < constants::HERO_COUNT; j++) {
                    EXPECT_FALSE(std::isnan(attenuation[j]));
                    EXPECT_FALSE(std::isinf(attenuation[j]));

                    EXPECT_GE(attenuation[j], 0);
                }
            }
        }

        class MediumTest : public SurfaceTest {
            protected:
                Ray makeRay(const Medium & medium) { return Ray(Vector<Float, 3>(0, 0, 0), Vector<Float, 3>(0, 0, 1), SampledSpectrum<Float>(550), &medium); }

                Medium makeHomogeneous(Float sigmaA, Float sigmaS, Float g) { return Medium::makeHomogeneous(makeSpectrum(sigmaA), makeSpectrum(sigmaS), g, nullptr); }
        };

        TEST_F(MediumTest, HomogeneousTransmittanceFollowsBeerLambert) {
            Medium medium = makeHomogeneous(Float(0.1), Float(0.4), 0);

            Ray rIn = makeRay(medium);

            intersection.t = 3;

            SampledSpectrum<Float> transmittance = medium.transmittance(intersection, rIn, false);

            for (int i = 0; i < constants::HERO_COUNT; i++) EXPECT_NEAR(transmittance[i], std::exp(Float(-1.5)), tolerances::STANDARD);
        }

        TEST_F(MediumTest, HomogeneousTransmittanceCollapsedFillsFirstLane) {
            Medium medium = makeHomogeneous(Float(0.1), Float(0.4), 0);

            Ray rIn = makeRay(medium);

            intersection.t = 3;

            SampledSpectrum<Float> transmittance = medium.transmittance(intersection, rIn, true);

            EXPECT_GT(transmittance[0], 0);

            for (int i = 1; i < constants::HERO_COUNT; i++) EXPECT_EQ(transmittance[i], 0);
        }

        TEST_F(MediumTest, HomogeneousTransmittanceToInfinity) {
            Medium attenuating = makeHomogeneous(Float(0.1), Float(0.4), 0);
            Medium vacuum = makeHomogeneous(0, 0, 0);

            intersection.t = constants::MAX;

            EXPECT_EQ(attenuating.transmittance(intersection, makeRay(attenuating), false), SampledSpectrum<Float>(0));
            EXPECT_EQ(vacuum.transmittance(intersection, makeRay(vacuum), false), SampledSpectrum<Float>(1));
        }

        TEST_F(MediumTest, HomogeneousSampleLambdasWithoutFluorophoreIsIdentity) {
            Medium medium = makeHomogeneous(Float(0.1), Float(0.4), 0);

            SampledSpectrum<Float> lambdas(550);
            SampledSpectrum<Float> probabilityDensity(1);

            EXPECT_EQ(medium.sampleLambdas(lambdas, probabilityDensity, false, state), lambdas);
        }

        TEST_F(MediumTest, HomogeneousSamplingDoesNotCollapse) {
            Medium medium = makeHomogeneous(Float(0.1), Float(0.9), Float(0.5));

            Ray rIn = makeRay(medium);

            for (int i = 0; i < 1000; i++) {
                Ray rOut = rIn;

                bool collapsed = false;

                SampledSpectrum<Float> probabilityDensity;

                medium.sample(intersection, rOut, probabilityDensity, collapsed, state);

                EXPECT_FALSE(collapsed);
            }
        }

        TEST_F(MediumTest, HomogeneousSampleWeightsAreFiniteAndNonNegative) {
            Medium medium = makeHomogeneous(Float(0.1), Float(0.9), Float(0.5));

            Ray rIn = makeRay(medium);

            for (int i = 0; i < 1000; i++) {
                Ray rOut = rIn;

                bool collapsed = false;

                SampledSpectrum<Float> probabilityDensity, attenuation = medium.sample(intersection, rOut, probabilityDensity, collapsed, state);

                for (int j = 0; j < constants::HERO_COUNT; j++) {
                    EXPECT_TRUE(std::isfinite(attenuation[j]));
                    EXPECT_GE(attenuation[j], 0);
                }
            }
        }

        TEST_F(MediumTest, HomogeneousSampleDensityMatchesPhaseFunction) {
            Medium medium = makeHomogeneous(Float(0.1), Float(0.9), Float(0.5));

            Ray rIn = makeRay(medium);

            for (int i = 0; i < 1000; i++) {
                Ray rOut = rIn;

                bool collapsed = false;

                SampledSpectrum<Float> probabilityDensity, evaluatedDensity;

                medium.sample(intersection, rOut, probabilityDensity, collapsed, state);

                ASSERT_GT(probabilityDensity[0], 0);

                medium.evaluate(rIn, rOut, evaluatedDensity, collapsed);

                EXPECT_NEAR(probabilityDensity[0], evaluatedDensity[0], tolerances::STANDARD);
            }
        }

        TEST_F(MediumTest, HomogeneousSamplingWeightIsScatteringCoefficient) {
            Medium medium = makeHomogeneous(Float(0.1), Float(0.9), Float(0.5));

            Ray rIn = makeRay(medium);

            for (int i = 0; i < 100; i++) {
                Ray rOut = rIn;

                bool collapsed = false;

                SampledSpectrum<Float> probabilityDensity, attenuation = medium.sample(intersection, rOut, probabilityDensity, collapsed, state);

                EXPECT_NEAR(attenuation[0], Float(0.9), tolerances::STANDARD);
            }
        }

        TEST_F(MediumTest, HomogeneousSamplingStartsAtScatterPoint) {
            Medium medium = makeHomogeneous(Float(0.1), Float(0.9), Float(0.5));

            Ray rIn = makeRay(medium);

            intersection.transformedPoint = Vector<Float, 3>(1, 2, 3);

            Ray rOut = rIn;

            bool collapsed = false;

            SampledSpectrum<Float> probabilityDensity;

            medium.sample(intersection, rOut, probabilityDensity, collapsed, state);

            expectEqual(rOut.getOrigin(), intersection.transformedPoint);

            EXPECT_NEAR(rOut.getDirection().length(), 1, tolerances::STANDARD);
            EXPECT_EQ(rOut.getMedium(), &medium);
        }

        TEST_F(MediumTest, HomogeneousCollapsedSamplingFillsFirstLane) {
            Medium medium = makeHomogeneous(Float(0.1), Float(0.9), Float(0.5));

            Ray rIn = makeRay(medium);

            Ray rOut = rIn;

            SampledSpectrum<Float> probabilityDensity, attenuation = medium.sample(intersection, rOut, probabilityDensity, true, state);

            EXPECT_GT(attenuation[0], 0);

            for (int i = 1; i < constants::HERO_COUNT; i++) EXPECT_EQ(attenuation[i], 0);
        }

        TEST_F(MediumTest, IsotropicPhaseFunctionIsUniform) {
            Medium medium = makeHomogeneous(Float(0.1), Float(0.9), 0);

            Ray rIn = makeRay(medium);

            Ray rOut(Vector<Float, 3>(), Vector<Float, 3>(1, 0, 0), SampledSpectrum<Float>(550), &medium);

            SampledSpectrum<Float> probabilityDensity;

            medium.evaluate(rIn, rOut, probabilityDensity, false);

            for (int i = 0; i < constants::HERO_COUNT; i++) EXPECT_NEAR(probabilityDensity[i], 1 / (4 * constants::PI), tolerances::STANDARD);
        }

        TEST_F(MediumTest, PhaseFunctionPeaksAlongScatteringDirection) {
            Medium forward = makeHomogeneous(Float(0.1), Float(0.9), Float(0.6));
            Medium backward = makeHomogeneous(Float(0.1), Float(0.9), Float(-0.6));

            Ray rIn = makeRay(forward);

            Ray straight(Vector<Float, 3>(), Vector<Float, 3>(0, 0, 1), SampledSpectrum<Float>(550), nullptr);
            Ray reversed(Vector<Float, 3>(), Vector<Float, 3>(0, 0, -1), SampledSpectrum<Float>(550), nullptr);

            SampledSpectrum<Float> forwardStraight, forwardReversed, backwardStraight, backwardReversed;

            forward.evaluate(rIn, straight, forwardStraight, false);
            forward.evaluate(rIn, reversed, forwardReversed, false);
            backward.evaluate(rIn, straight, backwardStraight, false);
            backward.evaluate(rIn, reversed, backwardReversed, false);

            EXPECT_GT(forwardStraight[0], forwardReversed[0]);
            EXPECT_GT(backwardReversed[0], backwardStraight[0]);
        }

        TEST_F(MediumTest, PhaseFunctionIntegratesToOne) {
            Medium medium = makeHomogeneous(Float(0.1), Float(0.9), Float(0.5));

            Ray rIn = makeRay(medium);

            Float sum = 0;

            for (int i = 0; i < 20000; i++) {
                Ray rOut(Vector<Float, 3>(), utils::randomUnitVector(state), SampledSpectrum<Float>(550), &medium);

                SampledSpectrum<Float> probabilityDensity;

                medium.evaluate(rIn, rOut, probabilityDensity, false);

                sum += probabilityDensity[0] * 4 * constants::PI;
            }

            EXPECT_NEAR(sum / 20000, 1, tolerances::STATISTICAL);
        }

        TEST_F(MediumTest, SampledScatteringHasAsymmetryParameterAsMeanCosine) {
            Float asymmetries[2] = { Float(0.5), Float(-0.5) };

            for (Float g : asymmetries) {
                Medium medium = makeHomogeneous(Float(0.1), Float(0.9), g);

                Ray rIn = makeRay(medium);

                Float sum = 0;

                for (int i = 0; i < 5000; i++) {
                    Ray rOut = rIn;

                    bool collapsed = false;

                    SampledSpectrum<Float> probabilityDensity;

                    medium.sample(intersection, rOut, probabilityDensity, collapsed, state);

                    sum += dot(rIn.getDirection(), rOut.getDirection());
                }

                EXPECT_NEAR(sum / 5000, g, tolerances::STATISTICAL);
            }
        }

        TEST_F(MediumTest, DistanceSampleTransmittancesAreFiniteAndNonNegative) {
            Medium medium = makeHomogeneous(Float(0.1), Float(0.9), Float(0.5));

            Ray rIn = makeRay(medium);

            for (int i = 0; i < 1000; i++) {
                Intersection surfaceIntersection;

                surfaceIntersection.t = 10;
                surfaceIntersection.isSurface = true;

                bool collapsed = false;

                SampledSpectrum<Float> probabilityDensity, transmittance = medium.sampleDistance(surfaceIntersection, rIn, probabilityDensity, collapsed, state);

                for (int j = 0; j < constants::HERO_COUNT; j++) {
                    EXPECT_TRUE(std::isfinite(transmittance[j]));
                    EXPECT_GE(transmittance[j], 0);
                }
            }
        }

        TEST_F(MediumTest, DistanceSampleBeforeSurfaceMovesIntersectionOntoRay) {
            Medium medium = makeHomogeneous(Float(0.1), Float(0.9), Float(0.5));

            Ray rIn = makeRay(medium);

            int events = 0;

            for (int i = 0; i < 1000; i++) {
                Intersection surfaceIntersection;

                surfaceIntersection.t = 10;
                surfaceIntersection.isSurface = true;

                bool collapsed = false;

                SampledSpectrum<Float> probabilityDensity;

                medium.sampleDistance(surfaceIntersection, rIn, probabilityDensity, collapsed, state);

                if (surfaceIntersection.t < 10) {
                    EXPECT_FALSE(surfaceIntersection.isSurface);

                    expectNear(surfaceIntersection.transformedPoint, rIn.at(surfaceIntersection.t), tolerances::ACCUMULATED);

                    events++;
                }
            }

            EXPECT_GT(events, 0);
        }

        TEST_F(MediumTest, DistanceSampleBeyondSurfaceKeepsSurfaceIntersection) {
            Medium medium = makeHomogeneous(Float(0.01), Float(0.01), Float(0.5));

            Ray rIn = makeRay(medium);

            int passes = 0;

            for (int i = 0; i < 1000; i++) {
                Intersection surfaceIntersection;

                surfaceIntersection.t = 10;
                surfaceIntersection.isSurface = true;

                bool collapsed = false;

                SampledSpectrum<Float> probabilityDensity;

                medium.sampleDistance(surfaceIntersection, rIn, probabilityDensity, collapsed, state);

                if (surfaceIntersection.isSurface) {
                    EXPECT_EQ(surfaceIntersection.t, 10);

                    passes++;
                }
            }

            EXPECT_GT(passes, 0);
        }

        TEST_F(MediumTest, DistanceSamplingCollapsedFillsFirstLane) {
            Medium medium = makeHomogeneous(Float(0.1), Float(0.9), Float(0.5));

            Ray rIn = makeRay(medium);

            Intersection surfaceIntersection;

            surfaceIntersection.t = 10;

            SampledSpectrum<Float> probabilityDensity, transmittance = medium.sampleDistance(surfaceIntersection, rIn, probabilityDensity, true, state);

            EXPECT_GT(transmittance[0], 0);

            for (int i = 1; i < constants::HERO_COUNT; i++) EXPECT_EQ(transmittance[i], 0);
        }

        TEST_F(MediumTest, DistanceSamplingThroughVacuumReachesSurface) {
            Medium medium = makeHomogeneous(0, 0, 0);

            Ray rIn = makeRay(medium);

            Intersection surfaceIntersection;

            surfaceIntersection.t = 5;

            bool collapsed = false;

            SampledSpectrum<Float> probabilityDensity, transmittance = medium.sampleDistance(surfaceIntersection, rIn, probabilityDensity, collapsed, state);

            EXPECT_TRUE(surfaceIntersection.isSurface);
            EXPECT_EQ(surfaceIntersection.t, 5);
            EXPECT_NEAR(transmittance[0], 1, tolerances::STANDARD);
        }

        TEST_F(MediumTest, DistanceSamplingToInfinityInVacuumStaysUnbounded) {
            Medium medium = makeHomogeneous(0, 0, 0);

            Ray rIn = makeRay(medium);

            Intersection surfaceIntersection;

            surfaceIntersection.t = constants::MAX;

            bool collapsed = false;

            SampledSpectrum<Float> probabilityDensity, transmittance = medium.sampleDistance(surfaceIntersection, rIn, probabilityDensity, collapsed, state);

            EXPECT_TRUE(surfaceIntersection.isSurface);
            EXPECT_EQ(surfaceIntersection.t, constants::MAX);
            EXPECT_NEAR(transmittance[0], 1, tolerances::STANDARD);
        }

        TEST_F(MediumTest, DistanceSamplingThroughAbsorberWeightsByTransmittance) {
            Medium medium = makeHomogeneous(Float(0.3), 0, 0);

            Ray rIn = makeRay(medium);

            for (int i = 0; i < 20; i++) {
                Intersection surfaceIntersection;

                surfaceIntersection.t = 4;

                bool collapsed = false;

                SampledSpectrum<Float> probabilityDensity, transmittance = medium.sampleDistance(surfaceIntersection, rIn, probabilityDensity, collapsed, state);

                EXPECT_TRUE(surfaceIntersection.isSurface);
                EXPECT_NEAR(transmittance[0], std::exp(Float(-1.2)), tolerances::STANDARD);
            }
        }

        TEST_F(MediumTest, UnboundedDistanceSamplingEstimatesScatteringAlbedo) {
            Float sigmaA = Float(0.28);
            Float sigmaS = Float(0.02);

            Medium medium = makeHomogeneous(sigmaA, sigmaS, Float(0.5));

            Ray rIn = makeRay(medium);

            Float sum = 0;
            Float sumSquared = 0;

            for (int i = 0; i < 5000; i++) {
                Intersection surfaceIntersection;

                surfaceIntersection.t = constants::MAX;
                surfaceIntersection.isSurface = true;

                bool collapsed = false;

                SampledSpectrum<Float> probabilityDensity, transmittance = medium.sampleDistance(surfaceIntersection, rIn, probabilityDensity, collapsed, state);

                ASSERT_GT(probabilityDensity[0], 0);

                Float value = transmittance[0] * sigmaS;

                sum += value;
                sumSquared += value * value;
            }

            Float mean = sum / 5000;
            Float variance = std::fmax(sumSquared / 5000 - mean * mean, Float(0));

            EXPECT_NEAR(mean, sigmaS / (sigmaA + sigmaS), 5 * std::sqrt(variance / 5000) + tolerances::STANDARD);
        }

        TEST_F(MediumTest, DistanceSamplingEstimatesAnalyticScatteringIntegral) {
            Float sigmaA = Float(0.28);
            Float sigmaS = Float(0.02);
            Float sigmaT = sigmaA + sigmaS;
            Float distance = 10;

            Medium medium = makeHomogeneous(sigmaA, sigmaS, Float(0.5));

            Ray rIn = makeRay(medium);

            Float sum = 0;
            Float sumSquared = 0;

            for (int i = 0; i < 5000; i++) {
                Intersection surfaceIntersection;

                surfaceIntersection.t = distance;
                surfaceIntersection.isSurface = true;

                bool collapsed = false;

                SampledSpectrum<Float> probabilityDensity, transmittance = medium.sampleDistance(surfaceIntersection, rIn, probabilityDensity, collapsed, state);

                ASSERT_GT(probabilityDensity[0], 0);

                Float value = transmittance[0] * (surfaceIntersection.isSurface ? 1 : sigmaS * surfaceIntersection.t / distance);

                sum += value;
                sumSquared += value * value;
            }

            Float attenuation = std::exp(-sigmaT * distance);
            Float expected = sigmaS * (1 - attenuation * (1 + sigmaT * distance)) / (sigmaT * sigmaT * distance) + attenuation;

            Float mean = sum / 5000;
            Float variance = std::fmax(sumSquared / 5000 - mean * mean, Float(0));

            EXPECT_NEAR(mean, expected, 5 * std::sqrt(variance / 5000) + tolerances::STANDARD);
        }
    }
}