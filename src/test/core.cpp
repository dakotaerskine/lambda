#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "core/allocator.h"
#include "core/builder.h"
#include "core/color.h"
#include "core/constants.h"
#include "core/dispatcher.h"
#include "core/error.h"
#include "core/parser.h"
#include "core/platform.h"
#include "core/utils.h"
#include "core/writer.h"
#include "test/test.h"
#include "test/tolerances.h"

namespace lambda {
    namespace {
        TEST(AllocatorTest, MakeStoresValue) {
            Allocator allocator;

            Float * value = allocator.make(Float(3));

            ASSERT_NE(value, nullptr);

            EXPECT_EQ(*value, 3);
        }

        TEST(AllocatorTest, MakeReturnsDistinctStorage) {
            Allocator allocator;

            Float * a = allocator.make(Float(1));
            Float * b = allocator.make(Float(2));

            EXPECT_NE(a, b);

            EXPECT_EQ(*a, 1);
            EXPECT_EQ(*b, 2);
        }

        TEST(AllocatorTest, MakeHostStoresValue) {
            Allocator allocator;

            Vector<Float, 3> expected(1, 2, 3);

            Vector<Float, 3> * value = allocator.makeHost(expected);

            ASSERT_NE(value, nullptr);

            EXPECT_EQ(*value, expected);
        }

        TEST(AllocatorTest, AllocateValueInitializes) {
            Allocator allocator;

            Float * values = allocator.allocate<Float>(4);

            ASSERT_NE(values, nullptr);

            for (int i = 0; i < 4; i++) EXPECT_EQ(values[i], 0);
        }

        TEST(AllocatorTest, AllocateCopiesSource) {
            Allocator allocator;

            Float source[3] = { 1, 2, 3 };

            Float * values = allocator.allocate(source, 3);

            ASSERT_NE(values, nullptr);

            EXPECT_NE(values, source);

            source[0] = 4;

            EXPECT_EQ(values[0], 1);
            EXPECT_EQ(values[1], 2);
            EXPECT_EQ(values[2], 3);
        }

        TEST(AllocatorTest, AllocateEmptyReturnsNull) {
            Allocator allocator;

            Float source[1] = { 1 };

            EXPECT_EQ(allocator.allocate<Float>(0), nullptr);
            EXPECT_EQ(allocator.allocate(source, 0), nullptr);
        }

        TEST(AllocatorTest, AllocateRejectsNegativeLength) {
            Allocator allocator;

            Float source[1] = { 1 };

            EXPECT_THROW(allocator.allocate<Float>(-1), Error);
            EXPECT_THROW(allocator.allocate(source, -1), Error);
        }

        TEST(ConstantsTest, NextAfterIsLargestFloatBelowOne) {
            EXPECT_EQ(constants::NEXT_AFTER, std::nextafter(Float(1), Float(0)));
            EXPECT_LT(constants::NEXT_AFTER, 1);
        }

        TEST(ConstantsTest, PermutationIsBijective) {
            bool seen[constants::PERMUTATION_SIZE] = {};

            for (int i = 0; i < constants::PERMUTATION_SIZE; i++) {
                int value = constants::PERMUTATION[i];

                ASSERT_GE(value, 0);
                ASSERT_LT(value, constants::PERMUTATION_SIZE);

                EXPECT_FALSE(seen[value]);

                seen[value] = true;
            }
        }

        TEST(ConstantsTest, LuminanceTableIntegratesToYIntegral) {
            double sum = 0;

            for (int i = 0; i < constants::CIE_LAMBDA_BINS; i++) sum += constants::CIE_XYZ_1931[i][1];

            EXPECT_NEAR(sum, constants::CIE_Y_INTEGRAL, tolerances::COARSE);
        }

        TEST(ConstantsTest, LuminanceTablePeaksAtPhotopicMaximum) {
            int peak = 0;

            for (int i = 1; i < constants::CIE_LAMBDA_BINS; i++)
                if (constants::CIE_XYZ_1931[i][1] > constants::CIE_XYZ_1931[peak][1]) peak = i;

            EXPECT_EQ(constants::CIE_LAMBDA_MIN + peak, 555);
            EXPECT_NEAR(constants::CIE_XYZ_1931[peak][1], 1, tolerances::COARSE);
        }

        TEST(ConstantsTest, IlluminantsAreNormalizedAt560) {
            EXPECT_NEAR(constants::CIE_D65[560 - constants::CIE_LAMBDA_MIN], 100, tolerances::STATISTICAL);
            EXPECT_NEAR(constants::CIE_D60[560 - constants::CIE_LAMBDA_MIN], 100, 1);
        }

        TEST(ErrorTest, LengthMessage) {
            EXPECT_STREQ(Error::length(5).what(), "length 5 is invalid");
            EXPECT_STREQ(Error::length(-1).what(), "length -1 is invalid");
        }

        TEST(ErrorTest, FormatPrependsProgram) {
            EXPECT_EQ(Error("failed").format("lambda"), "lambda: failed");
        }

        TEST(ErrorTest, FileErrorFormatWithLocation) {
            EXPECT_EQ(FileError("scene.lrd", 3, 7, "unexpected token").format("lambda"), "scene.lrd:3:7: unexpected token");
        }

        TEST(ErrorTest, FileErrorFormatWithoutLocation) {
            EXPECT_EQ(FileError("scene.lrd", "invalid file extension").format("lambda"), "scene.lrd: invalid file extension");
        }

        TEST(ErrorTest, FailedToOpenIncludesReason) {
            errno = ENOENT;

            EXPECT_STREQ(FileError::failedToOpen("scene.lrd").what(), "failed to open file (no such file or directory)");
        }

        class ErrorHandlerTest : public ::testing::Test {
            protected:
                std::ostringstream output;
                std::streambuf * original;

                void SetUp() override { original = std::cerr.rdbuf(output.rdbuf()); }

                void TearDown() override { std::cerr.rdbuf(original); }
        };

        TEST_F(ErrorHandlerTest, ErrorUsesFormat) {
            ErrorHandler::handle("lambda", Error("failed"));

            EXPECT_EQ(output.str(), "lambda: failed\n");
        }

        TEST_F(ErrorHandlerTest, FileErrorUsesFileLocation) {
            ErrorHandler::handle("lambda", FileError("scene.lrd", 2, 5, "bad value"));

            EXPECT_EQ(output.str(), "scene.lrd:2:5: bad value\n");
        }

        TEST_F(ErrorHandlerTest, StandardExceptionPrependsProgram) {
            ErrorHandler::handle("lambda", std::runtime_error("failed"));

            EXPECT_EQ(output.str(), "lambda: failed\n");
        }

        TEST_F(ErrorHandlerTest, EmptyProgramPrintsMessageOnly) {
            ErrorHandler::handle("", std::runtime_error("failed"));

            EXPECT_EQ(output.str(), "failed\n");
        }

        TEST(UtilsTest, HasExtensionMatches) {
            EXPECT_TRUE(utils::hasExtension("scene.lrd", ".lrd"));
            EXPECT_TRUE(utils::hasExtension("a/b/image.exr", ".exr"));
            EXPECT_TRUE(utils::hasExtension(".exr", ".exr"));
        }

        TEST(UtilsTest, HasExtensionIgnoresCase) {
            EXPECT_TRUE(utils::hasExtension("scene.LRD", ".lrd"));
            EXPECT_TRUE(utils::hasExtension("scene.lrd", ".LRD"));
            EXPECT_TRUE(utils::hasExtension("scene.LrD", ".lRd"));
        }

        TEST(UtilsTest, HasExtensionRejectsMismatch) {
            EXPECT_FALSE(utils::hasExtension("scene.lrd", ".exr"));
            EXPECT_FALSE(utils::hasExtension("scene.lrd.bak", ".lrd"));
            EXPECT_FALSE(utils::hasExtension("lrd", ".lrd"));
            EXPECT_FALSE(utils::hasExtension("", ".lrd"));
        }

        TEST(UtilsTest, sRGBEncodeKnownValues) {
            EXPECT_NEAR(utils::sRGBEncode(0), 0, tolerances::STANDARD);
            EXPECT_NEAR(utils::sRGBEncode(1), 1, tolerances::STANDARD);
            EXPECT_NEAR(utils::sRGBEncode(Float(0.5)), Float(0.735357), tolerances::STANDARD);
        }

        TEST(UtilsTest, sRGBEncodeLinearSegment) {
            EXPECT_NEAR(utils::sRGBEncode(Float(0.002)), Float(12.92) * Float(0.002), tolerances::STANDARD);
            EXPECT_NEAR(utils::sRGBDecode(Float(0.02)), Float(0.02) / Float(12.92), tolerances::STANDARD);
        }

        TEST(UtilsTest, sRGBRoundTrip) {
            for (Float value = 0; value <= 1; value += Float(0.05)) EXPECT_NEAR(utils::sRGBDecode(utils::sRGBEncode(value)), value, tolerances::STANDARD);
        }

        TEST(UtilsTest, sRGBEncodeIsMonotonic) {
            Float previous = utils::sRGBEncode(0);

            for (Float value = Float(0.01); value <= 1; value += Float(0.01)) {
                Float encoded = utils::sRGBEncode(value);

                EXPECT_GT(encoded, previous);

                previous = encoded;
            }
        }

        TEST(UtilsTest, ClampInsideRangeIsIdentity) {
            EXPECT_EQ(utils::clamp(Float(0.5), Float(0), Float(1)), Float(0.5));
            EXPECT_EQ(utils::clamp(3, 1, 5), 3);
        }

        TEST(UtilsTest, ClampBoundsValue) {
            EXPECT_EQ(utils::clamp(Float(-1), Float(0), Float(1)), 0);
            EXPECT_EQ(utils::clamp(Float(2), Float(0), Float(1)), 1);
            EXPECT_EQ(utils::clamp(-3, 1, 5), 1);
            EXPECT_EQ(utils::clamp(9, 1, 5), 5);
        }

        TEST(UtilsTest, ClampMapsNaNToLowerBound) {
            EXPECT_EQ(utils::clamp(std::numeric_limits<Float>::quiet_NaN(), Float(0), Float(1)), 0);
        }

        TEST(UtilsTest, InterpolateEndpointsAndMidpoint) {
            EXPECT_EQ(utils::interpolate(Float(2), Float(6), Float(0)), 2);
            EXPECT_EQ(utils::interpolate(Float(2), Float(6), Float(1)), 6);
            EXPECT_NEAR(utils::interpolate(Float(2), Float(6), Float(0.25)), 3, tolerances::STANDARD);
        }

        TEST(UtilsTest, InterpolateVectors) {
            Vector<Float, 3> result = utils::interpolate(Vector<Float, 3>(0, 2, 4), Vector<Float, 3>(2, 4, 8), Float(0.5));

            EXPECT_NEAR(result[0], 1, tolerances::STANDARD);
            EXPECT_NEAR(result[1], 3, tolerances::STANDARD);
            EXPECT_NEAR(result[2], 6, tolerances::STANDARD);
        }

        TEST(UtilsTest, SmoothStepEndpointsAndMidpoint) {
            EXPECT_EQ(utils::smoothStep(Float(0)), 0);
            EXPECT_EQ(utils::smoothStep(Float(1)), 1);
            EXPECT_NEAR(utils::smoothStep(Float(0.5)), Float(0.5), tolerances::STANDARD);
        }

        TEST(UtilsTest, SigmoidKnownValues) {
            EXPECT_EQ(utils::sigmoid(Float(0)), Float(0.5));
            EXPECT_NEAR(utils::sigmoid(Float(1)), Float(0.5) + Float(0.5) / std::sqrt(Float(2)), tolerances::STANDARD);
        }

        TEST(UtilsTest, SigmoidIsSymmetricAndBounded) {
            for (Float x = Float(0.1); x < 100; x *= 2) {
                EXPECT_NEAR(utils::sigmoid(x) + utils::sigmoid(-x), 1, tolerances::STANDARD);

                EXPECT_GT(utils::sigmoid(x), Float(0.5));
                EXPECT_LE(utils::sigmoid(x), 1);
            }
        }

        TEST(UtilsTest, SigmoidIsMonotonic) {
            Float previous = utils::sigmoid(Float(-50));

            for (Float x = -49; x <= 50; x += 1) {
                Float value = utils::sigmoid(x);

                EXPECT_GT(value, previous);

                previous = value;
            }
        }

        TEST(UtilsTest, ReflectedFlipsNormalComponent) {
            Vector<Float, 3> result = utils::reflected(Vector<Float, 3>(1, -1, 0), Vector<Float, 3>(0, 1, 0));

            EXPECT_NEAR(result[0], 1, tolerances::STANDARD);
            EXPECT_NEAR(result[1], 1, tolerances::STANDARD);
            EXPECT_NEAR(result[2], 0, tolerances::STANDARD);
        }

        TEST(UtilsTest, ReflectedIsInvolutionAndPreservesLength) {
            Vector<Float, 3> v = normalize(Vector<Float, 3>(1, -2, Float(0.5)));
            Vector<Float, 3> n = normalize(Vector<Float, 3>(Float(0.2), 1, Float(0.3)));

            Vector<Float, 3> r = utils::reflected(v, n);

            EXPECT_NEAR(r.length(), 1, tolerances::STANDARD);
            EXPECT_NEAR(dot(r, n), -dot(v, n), tolerances::STANDARD);

            Vector<Float, 3> back = utils::reflected(r, n);

            for (int i = 0; i < 3; i++) EXPECT_NEAR(back[i], v[i], tolerances::STANDARD);
        }

        TEST(UtilsTest, ReflectedKeepsTangentialDirection) {
            Vector<Float, 3> v(Float(0.6), 0, Float(0.8));

            Vector<Float, 3> r = utils::reflected(v, Vector<Float, 3>(0, 1, 0));

            for (int i = 0; i < 3; i++) EXPECT_NEAR(r[i], v[i], tolerances::STANDARD);
        }

        TEST(UtilsTest, RefractedWithUnitRatioIsIdentity) {
            Vector<Float, 3> v = normalize(Vector<Float, 3>(1, -2, Float(0.5)));

            Vector<Float, 3> r = utils::refracted(v, Vector<Float, 3>(0, 1, 0), 1);

            for (int i = 0; i < 3; i++) EXPECT_NEAR(r[i], v[i], tolerances::STANDARD);
        }

        TEST(UtilsTest, RefractedAtNormalIncidenceIsUnbent) {
            Vector<Float, 3> r = utils::refracted(Vector<Float, 3>(0, -1, 0), Vector<Float, 3>(0, 1, 0), Float(1) / Float(1.5));

            EXPECT_NEAR(r[0], 0, tolerances::STANDARD);
            EXPECT_NEAR(r[1], -1, tolerances::STANDARD);
            EXPECT_NEAR(r[2], 0, tolerances::STANDARD);
        }

        TEST(UtilsTest, RefractedObeysSnellsLaw) {
            Float ratio = Float(1) / Float(1.5);
            Float sinThetaI = Float(0.5);

            Vector<Float, 3> v(sinThetaI, -std::sqrt(1 - sinThetaI * sinThetaI), 0);

            Vector<Float, 3> r = utils::refracted(v, Vector<Float, 3>(0, 1, 0), ratio);

            EXPECT_NEAR(r.length(), 1, tolerances::STANDARD);
            EXPECT_NEAR(r[0], ratio * sinThetaI, tolerances::STANDARD);
            EXPECT_LT(r[1], 0);
        }

        TEST(UtilsTest, RefractedTotalInternalReflectionReflects) {
            Float sinThetaI = std::sin(constants::PI / 3);

            Vector<Float, 3> v(sinThetaI, -std::cos(constants::PI / 3), 0);
            Vector<Float, 3> n(0, 1, 0);

            Vector<Float, 3> r = utils::refracted(v, n, Float(1.5));
            Vector<Float, 3> expected = utils::reflected(v, n);

            for (int i = 0; i < 3; i++) EXPECT_NEAR(r[i], expected[i], tolerances::STANDARD);
        }

        TEST(UtilsTest, FadeEndpointsAndMidpoint) {
            EXPECT_EQ(utils::fade(0), 0);
            EXPECT_EQ(utils::fade(1), 1);
            EXPECT_NEAR(utils::fade(Float(0.5)), Float(0.5), tolerances::STANDARD);
        }

        TEST(UtilsTest, FadeIsMonotonic) {
            Float previous = utils::fade(0);

            for (Float t = Float(0.01); t <= 1; t += Float(0.01)) {
                Float value = utils::fade(t);

                EXPECT_GE(value, previous);

                previous = value;
            }
        }

        TEST(UtilsTest, PermutationWrapsAround) {
            for (int i = 0; i < 256; i++) {
                EXPECT_EQ(utils::permutation(i), constants::PERMUTATION[i]);
                EXPECT_EQ(utils::permutation(i + 256), utils::permutation(i));
                EXPECT_EQ(utils::permutation(i + 512), utils::permutation(i));
            }
        }

        TEST(UtilsTest, GradientSelectsAxisPairs) {
            Float expected[12] = { 3, 1, -1, -3, 4, 2, -2, -4, 5, 1, -1, -5 };

            for (int hash = 0; hash < 12; hash++) EXPECT_EQ(utils::gradient(hash, 1, 2, 3), expected[hash]);
        }

        TEST(UtilsTest, GradientRepeatsEveryTwelveHashes) {
            for (int hash = 0; hash < 12; hash++) EXPECT_EQ(utils::gradient(hash + 12, 1, 2, 3), utils::gradient(hash, 1, 2, 3));
        }

        TEST(UtilsTest, HashIsNormalizedAndDeterministic) {
            for (int x = -3; x < 4; x++)
                for (int y = -3; y < 4; y++) {
                    Float value = utils::hash(x, y, 1, 0);

                    EXPECT_GE(value, 0);
                    EXPECT_LE(value, 1);

                    EXPECT_EQ(value, utils::hash(x, y, 1, 0));
                }
        }

        TEST(UtilsTest, HashRepeatsEvery256Cells) {
            EXPECT_EQ(utils::hash(5, 6, 7, 0), utils::hash(5 + 256, 6, 7, 0));
            EXPECT_EQ(utils::hash(5, 6, 7, 0), utils::hash(5, 6 + 512, 7 - 256, 0));
        }

        TEST(UtilsTest, HashDependsOnSalt) {
            int differing = 0;

            for (int x = 0; x < 16; x++)
                if (utils::hash(x, 1, 2, 0) != utils::hash(x, 1, 2, 97)) differing++;

            EXPECT_GT(differing, 8);
        }

        TEST(UtilsTest, PerlinIsHalfAtLatticePoints) {
            for (int i = -2; i <= 2; i++)
                for (int j = -2; j <= 2; j++) {
                    Vector<Float, 3> point(Float(i), Float(j), 3);

                    EXPECT_EQ(utils::perlin(point), Float(0.5));
                }
        }

        TEST(UtilsTest, PerlinIsNormalizedAndDeterministic) {
            for (Float x = -3; x < 3; x += Float(0.37))
                for (Float y = -3; y < 3; y += Float(0.41)) {
                    Vector<Float, 3> point(x, y, Float(0.3));

                    Float value = utils::perlin(point);

                    EXPECT_GE(value, 0);
                    EXPECT_LE(value, 1);

                    EXPECT_EQ(value, utils::perlin(point));
                }
        }

        TEST(UtilsTest, PerlinIsContinuous) {
            for (Float x = -2; x < 2; x += Float(0.31)) {
                Vector<Float, 3> point(x, Float(0.7), Float(1.3));
                Vector<Float, 3> shifted = point + Vector<Float, 3>(Float(1e-3), 0, 0);

                EXPECT_NEAR(utils::perlin(point), utils::perlin(shifted), tolerances::STATISTICAL);
            }
        }

        TEST(UtilsTest, WorleyIsNormalizedAndDeterministic) {
            for (Float x = -3; x < 3; x += Float(0.37))
                for (Float y = -3; y < 3; y += Float(0.41)) {
                    Vector<Float, 3> point(x, y, Float(0.3));

                    Float value = utils::worley(point);

                    EXPECT_GE(value, 0);
                    EXPECT_LE(value, 1);

                    EXPECT_EQ(value, utils::worley(point));
                }
        }

        TEST(UtilsTest, WorleyVanishesAtFeaturePoints) {
            for (int x = 0; x < 3; x++)
                for (int y = 0; y < 3; y++) {
                    Vector<Float, 3> feature(Float(x) + utils::hash(x, y, 0, 0), Float(y) + utils::hash(x, y, 0, 97), utils::hash(x, y, 0, 193));

                    EXPECT_NEAR(utils::worley(feature), 0, tolerances::STANDARD);
                }
        }

        TEST(UtilsTest, WorleyIsContinuous) {
            for (Float x = -2; x < 2; x += Float(0.31)) {
                Vector<Float, 3> point(x, Float(0.7), Float(1.3));
                Vector<Float, 3> shifted = point + Vector<Float, 3>(Float(1e-3), 0, 0);

                EXPECT_NEAR(utils::worley(point), utils::worley(shifted), tolerances::STATISTICAL);
            }
        }

        TEST(UtilsTest, RandomInLinearStaysInUnitInterval) {
            Random state(1, 0);

            for (int i = 0; i < 1000; i++) {
                Float x = utils::randomInLinear(Float(0.2), Float(1.5), state);

                EXPECT_GE(x, 0);
                EXPECT_LT(x, 1);
            }
        }

        TEST(UtilsTest, RandomInLinearEqualEndpointsIsUniform) {
            Random state(1, 0);

            Float sum = 0;

            for (int i = 0; i < 5000; i++) sum += utils::randomInLinear(1, 1, state);

            EXPECT_NEAR(sum / 5000, Float(0.5), tolerances::STATISTICAL);
        }

        TEST(UtilsTest, RandomInLinearFollowsLinearDensity) {
            Random state(1, 0);

            Float sum = 0;

            for (int i = 0; i < 5000; i++) sum += utils::randomInLinear(0, 1, state);

            EXPECT_NEAR(sum / 5000, Float(2) / 3, tolerances::STATISTICAL);
        }

        TEST(UtilsTest, RandomUnitVectorIsNormalizedAndCentered) {
            Random state(1, 0);

            Vector<Float, 3> sum;

            for (int i = 0; i < 5000; i++) {
                Vector<Float, 3> v = utils::randomUnitVector(state);

                EXPECT_NEAR(v.length(), 1, tolerances::STANDARD);

                sum += v;
            }

            for (int i = 0; i < 3; i++) EXPECT_NEAR(sum[i] / 5000, 0, tolerances::STATISTICAL);
        }

        TEST(UtilsTest, RandomInHemisphereIsCosineWeighted) {
            Random state(1, 0);

            Vector<Float, 3> normals[3] = { Vector<Float, 3>(0, 1, 0), Vector<Float, 3>(2, Float(0.5), -1), Vector<Float, 3>(Float(0.1), -1, Float(0.2)) };

            for (const Vector<Float, 3> & normal : normals) {
                Vector<Float, 3> w = normalize(normal);

                Float sum = 0;

                for (int i = 0; i < 5000; i++) {
                    Vector<Float, 3> v = utils::randomInHemisphere(normal, state);

                    ASSERT_NEAR(v.length(), 1, tolerances::STANDARD);

                    ASSERT_GE(dot(v, w), -tolerances::STANDARD);

                    sum += dot(v, w);
                }

                EXPECT_NEAR(sum / 5000, Float(2) / 3, tolerances::STATISTICAL);
            }
        }

        TEST(UtilsTest, RandomInConeStaysInsideCone) {
            Random state(1, 0);

            Vector<Float, 3> w = normalize(Vector<Float, 3>(1, 2, -1));

            Float sum = 0;

            for (int i = 0; i < 5000; i++) {
                Vector<Float, 3> v = utils::randomInCone(w, Float(0.5), state);

                ASSERT_NEAR(v.length(), 1, tolerances::STANDARD);

                ASSERT_GE(dot(v, w), Float(0.5) - tolerances::STANDARD);

                sum += dot(v, w);
            }

            EXPECT_NEAR(sum / 5000, Float(0.75), tolerances::STATISTICAL);
        }

        TEST(UtilsTest, RandomInConeWithUnitCosineIsAxis) {
            Random state(1, 0);

            Vector<Float, 3> w = normalize(Vector<Float, 3>(1, 2, -1));

            for (int i = 0; i < 10; i++) {
                Vector<Float, 3> v = utils::randomInCone(w, 1, state);

                for (int j = 0; j < 3; j++) EXPECT_NEAR(v[j], w[j], tolerances::STANDARD);
            }
        }

        TEST(UtilsTest, RandomInConeWithOppositeCosineCoversSphere) {
            Random state(1, 0);

            Vector<Float, 3> w(0, 0, 1);

            Float sum = 0;

            for (int i = 0; i < 5000; i++) sum += dot(utils::randomInCone(w, -1, state), w);

            EXPECT_NEAR(sum / 5000, 0, tolerances::STATISTICAL);
        }

        class ColorSpaceTest : public GeometryTest {
            protected:
                inline static Allocator allocator;

                inline static float * upsamplingScale;
                inline static float (* upsamplingLUT)[constants::UPSAMPLING_RESOLUTION][constants::UPSAMPLING_RESOLUTION][constants::UPSAMPLING_RESOLUTION][3];

                void SetUp() override {
                    ASSERT_NE(ColorSpace::sRGB(), nullptr);
                    ASSERT_NE(ColorSpace::Rec2020(), nullptr);
                    ASSERT_NE(ColorSpace::ACES2065(), nullptr);
                }

                static void SetUpTestSuite() {
                    ColorSpace::initialize(allocator);

                    upsamplingScale = allocator.allocate<float>(constants::UPSAMPLING_RESOLUTION);
                    upsamplingLUT = allocator.allocate<float[constants::UPSAMPLING_RESOLUTION][constants::UPSAMPLING_RESOLUTION][constants::UPSAMPLING_RESOLUTION][3]>(3);

                    for (int i = 0; i < constants::UPSAMPLING_RESOLUTION; i++) upsamplingScale[i] = float(i) / float(constants::UPSAMPLING_RESOLUTION - 1);

                    for (int c = 0; c < 3; c++)
                        for (int i = 0; i < constants::UPSAMPLING_RESOLUTION; i++)
                            for (int j = 0; j < constants::UPSAMPLING_RESOLUTION; j++)
                                for (int k = 0; k < constants::UPSAMPLING_RESOLUTION; k++) {
                                    upsamplingLUT[c][i][j][k][0] = float(i + 100 * c);
                                    upsamplingLUT[c][i][j][k][1] = float(j);
                                    upsamplingLUT[c][i][j][k][2] = float(k);
                                }
                }

                ColorSpace makeUpsamplingSpace() const { return ColorSpace(constants::CHROMATICITIES_SRGB, nullptr, upsamplingScale, upsamplingLUT); }
        };

        TEST_F(ColorSpaceTest, GetReturnsSpaceByName) {
            EXPECT_EQ(ColorSpace::get("srgb"), ColorSpace::sRGB());
            EXPECT_EQ(ColorSpace::get("rec2020"), ColorSpace::Rec2020());
            EXPECT_EQ(ColorSpace::get("aces2065-1"), ColorSpace::ACES2065());
        }

        TEST_F(ColorSpaceTest, GetRejectsUnknownName) {
            EXPECT_EQ(ColorSpace::get("unknown"), nullptr);
            EXPECT_EQ(ColorSpace::get("sRGB"), nullptr);
            EXPECT_EQ(ColorSpace::get(""), nullptr);
        }

        TEST_F(ColorSpaceTest, InitializeIsIdempotent) {
            const ColorSpace * srgb = ColorSpace::sRGB();

            ColorSpace::initialize(allocator);

            EXPECT_EQ(ColorSpace::sRGB(), srgb);
        }

        TEST_F(ColorSpaceTest, ChromaticitiesMatchDefinition) {

            for (int i = 0; i < 8; i++) {
                EXPECT_EQ(ColorSpace::sRGB()->chromaticity(i), constants::CHROMATICITIES_SRGB[i]);
                EXPECT_EQ(ColorSpace::Rec2020()->chromaticity(i), constants::CHROMATICITIES_REC2020[i]);
                EXPECT_EQ(ColorSpace::ACES2065()->chromaticity(i), constants::CHROMATICITIES_ACES2065[i]);
            }
        }

        TEST_F(ColorSpaceTest, Rec2020ContainsSRGB) {
            EXPECT_TRUE(ColorSpace::Rec2020()->contains(*ColorSpace::sRGB()));
        }

        TEST_F(ColorSpaceTest, SRGBDoesNotContainRec2020) {
            EXPECT_FALSE(ColorSpace::sRGB()->contains(*ColorSpace::Rec2020()));
        }

        TEST_F(ColorSpaceTest, ACES2065ContainsSRGBAndRec2020) {
            EXPECT_TRUE(ColorSpace::ACES2065()->contains(*ColorSpace::sRGB()));
            EXPECT_TRUE(ColorSpace::ACES2065()->contains(*ColorSpace::Rec2020()));
        }

        TEST_F(ColorSpaceTest, ContainsItself) {

            EXPECT_TRUE(ColorSpace::sRGB()->contains(*ColorSpace::sRGB()));
            EXPECT_TRUE(ColorSpace::Rec2020()->contains(*ColorSpace::Rec2020()));
            EXPECT_TRUE(ColorSpace::ACES2065()->contains(*ColorSpace::ACES2065()));
        }

        TEST_F(ColorSpaceTest, WhiteMapsToWhitepoint) {
            const ColorSpace * spaces[3] = { ColorSpace::sRGB(), ColorSpace::Rec2020(), ColorSpace::ACES2065() };

            for (const ColorSpace * space : spaces) {
                ASSERT_NE(space, nullptr);

                Vector<double, 3> xyz = space->toXYZ(Vector<double, 3>(1, 1, 1));

                EXPECT_NEAR(xyz[0], space->chromaticity(6) / space->chromaticity(7), tolerances::STANDARD);
                EXPECT_NEAR(xyz[1], 1, tolerances::STANDARD);
                EXPECT_NEAR(xyz[2], (1 - space->chromaticity(6) - space->chromaticity(7)) / space->chromaticity(7), tolerances::STANDARD);
            }
        }

        TEST_F(ColorSpaceTest, SRGBPrimariesMatchStandardMatrix) {

            Vector<double, 3> red = ColorSpace::sRGB()->toXYZ(Vector<double, 3>(1, 0, 0));
            Vector<double, 3> green = ColorSpace::sRGB()->toXYZ(Vector<double, 3>(0, 1, 0));
            Vector<double, 3> blue = ColorSpace::sRGB()->toXYZ(Vector<double, 3>(0, 0, 1));

            EXPECT_NEAR(red[0], 0.4124, tolerances::COARSE);
            EXPECT_NEAR(red[1], 0.2126, tolerances::COARSE);
            EXPECT_NEAR(red[2], 0.0193, tolerances::COARSE);

            EXPECT_NEAR(green[0], 0.3576, tolerances::COARSE);
            EXPECT_NEAR(green[1], 0.7152, tolerances::COARSE);
            EXPECT_NEAR(green[2], 0.1192, tolerances::COARSE);

            EXPECT_NEAR(blue[0], 0.1805, tolerances::COARSE);
            EXPECT_NEAR(blue[1], 0.0722, tolerances::COARSE);
            EXPECT_NEAR(blue[2], 0.9505, tolerances::COARSE);
        }

        TEST_F(ColorSpaceTest, ToRGBInvertsToXYZ) {
            const ColorSpace * spaces[3] = { ColorSpace::sRGB(), ColorSpace::Rec2020(), ColorSpace::ACES2065() };

            for (const ColorSpace * space : spaces) {
                ASSERT_NE(space, nullptr);

                Vector<double, 3> color(0.2, 0.6, 0.9);

                Vector<double, 3> roundTrip = space->toRGB(space->toXYZ(color));

                for (int i = 0; i < 3; i++) EXPECT_NEAR(roundTrip[i], color[i], tolerances::STANDARD);
            }
        }

        TEST_F(ColorSpaceTest, ConversionIsLinear) {

            Vector<double, 3> a(0.1, 0.4, 0.2);
            Vector<double, 3> b(0.3, 0.1, 0.5);

            Vector<double, 3> sum = ColorSpace::Rec2020()->toXYZ(a + b);
            Vector<double, 3> separate = ColorSpace::Rec2020()->toXYZ(a) + ColorSpace::Rec2020()->toXYZ(b);

            for (int i = 0; i < 3; i++) EXPECT_NEAR(sum[i], separate[i], tolerances::STANDARD);
        }

        TEST_F(ColorSpaceTest, LabOfWhitepointIsNeutralWhite) {

            Vector<double, 3> whitepoint = ColorSpace::sRGB()->toXYZ(Vector<double, 3>(1, 1, 1));

            Vector<double, 3> lab = ColorSpace::sRGB()->toLab(Vector<double, 3>(1, 1, 1), whitepoint);

            EXPECT_NEAR(lab[0], 100, tolerances::COARSE);
            EXPECT_NEAR(lab[1], 0, tolerances::COARSE);
            EXPECT_NEAR(lab[2], 0, tolerances::COARSE);
        }

        TEST_F(ColorSpaceTest, LabOfBlackIsZero) {

            Vector<double, 3> whitepoint = ColorSpace::sRGB()->toXYZ(Vector<double, 3>(1, 1, 1));

            Vector<double, 3> lab = ColorSpace::sRGB()->toLab(Vector<double, 3>(0, 0, 0), whitepoint);

            for (int i = 0; i < 3; i++) EXPECT_NEAR(lab[i], 0, tolerances::COARSE);
        }

        TEST_F(ColorSpaceTest, LabOfNeutralGrayHasNoChroma) {

            Vector<double, 3> whitepoint = ColorSpace::sRGB()->toXYZ(Vector<double, 3>(1, 1, 1));

            Vector<double, 3> lab = ColorSpace::sRGB()->toLab(Vector<double, 3>(0.18, 0.18, 0.18), whitepoint);

            EXPECT_NEAR(lab[0], 116 * std::cbrt(0.18) - 16, tolerances::COARSE);
            EXPECT_NEAR(lab[1], 0, tolerances::COARSE);
            EXPECT_NEAR(lab[2], 0, tolerances::COARSE);
        }

        TEST_F(ColorSpaceTest, IlluminantMatchesReferenceTable) {

            for (int lambda = constants::CIE_LAMBDA_MIN; lambda <= constants::CIE_LAMBDA_MAX; lambda += 10) {
                EXPECT_EQ(ColorSpace::sRGB()->illuminant(lambda), constants::CIE_D65[lambda - constants::CIE_LAMBDA_MIN]);
                EXPECT_EQ(ColorSpace::Rec2020()->illuminant(lambda), constants::CIE_D65[lambda - constants::CIE_LAMBDA_MIN]);
                EXPECT_EQ(ColorSpace::ACES2065()->illuminant(lambda), constants::CIE_D60[lambda - constants::CIE_LAMBDA_MIN]);
            }
        }

        TEST_F(ColorSpaceTest, IlluminantIsZeroOutsideVisibleRange) {

            EXPECT_EQ(ColorSpace::sRGB()->illuminant(constants::CIE_LAMBDA_MIN - 1), 0);
            EXPECT_EQ(ColorSpace::sRGB()->illuminant(constants::CIE_LAMBDA_MAX + 1), 0);

            EXPECT_NE(ColorSpace::sRGB()->illuminant(constants::CIE_LAMBDA_MIN), 0);
            EXPECT_NE(ColorSpace::sRGB()->illuminant(constants::CIE_LAMBDA_MAX), 0);
        }

        TEST_F(ColorSpaceTest, IlluminantIsZeroWithoutTable) {
            ColorSpace space(constants::CHROMATICITIES_SRGB);

            EXPECT_EQ(space.illuminant(560), 0);
        }

        TEST_F(ColorSpaceTest, BradfordAdaptationBetweenIdenticalWhitepointsIsIdentity) {

            Matrix<double, 3> adaptation = ColorSpace::bradfordAdaptation(*ColorSpace::sRGB(), *ColorSpace::Rec2020());

            for (int i = 0; i < 3; i++)
                for (int j = 0; j < 3; j++) EXPECT_NEAR(adaptation(i, j), i == j ? 1 : 0, tolerances::STANDARD);
        }

        TEST_F(ColorSpaceTest, BradfordAdaptationMapsWhitepointToWhitepoint) {

            Vector<double, 3> adapted = ColorSpace::bradfordAdaptation(*ColorSpace::sRGB(), *ColorSpace::ACES2065()) * ColorSpace::sRGB()->toXYZ(Vector<double, 3>(1, 1, 1));
            Vector<double, 3> expected = ColorSpace::ACES2065()->toXYZ(Vector<double, 3>(1, 1, 1));

            for (int i = 0; i < 3; i++) EXPECT_NEAR(adapted[i], expected[i], tolerances::STANDARD);
        }

        TEST_F(ColorSpaceTest, BradfordAdaptationIsInvertible) {

            Matrix<double, 3> product = ColorSpace::bradfordAdaptation(*ColorSpace::sRGB(), *ColorSpace::ACES2065()) * ColorSpace::bradfordAdaptation(*ColorSpace::ACES2065(), *ColorSpace::sRGB());

            for (int i = 0; i < 3; i++)
                for (int j = 0; j < 3; j++) EXPECT_NEAR(product(i, j), i == j ? 1 : 0, tolerances::STANDARD);
        }

        TEST_F(ColorSpaceTest, ConvertingToACES2065AndBackRestoresSRGBColor) {
            Vector<double, 3> color(0.5, 0.25, 0.75);

            Vector<double, 3> converted = ColorSpace::ACES2065()->toRGB(ColorSpace::bradfordAdaptation(*ColorSpace::sRGB(), *ColorSpace::ACES2065()) * ColorSpace::sRGB()->toXYZ(color));
            Vector<double, 3> restored = ColorSpace::sRGB()->toRGB(ColorSpace::bradfordAdaptation(*ColorSpace::ACES2065(), *ColorSpace::sRGB()) * ColorSpace::ACES2065()->toXYZ(converted));

            EXPECT_NEAR(restored[0], color[0], tolerances::STANDARD);
            EXPECT_NEAR(restored[1], color[1], tolerances::STANDARD);
            EXPECT_NEAR(restored[2], color[2], tolerances::STANDARD);
        }

        TEST_F(ColorSpaceTest, WithoutTablesHasNoUpsampling) {
            ColorSpace space(constants::CHROMATICITIES_SRGB);

            EXPECT_FALSE(space.hasUpsampling());
        }

        TEST_F(ColorSpaceTest, UpsampleWithoutTablesIsZero) {
            ColorSpace space(constants::CHROMATICITIES_SRGB);

            expectEqual(space.upsample(Vector<Float, 3>(Float(0.5), Float(0.25), Float(0.125))), Vector<Float, 3>());
        }

        TEST_F(ColorSpaceTest, UpsampleInterpolatesTrilinearlyInEachCase) {
            ColorSpace space = makeUpsamplingSpace();

            ASSERT_TRUE(space.hasUpsampling());

            Vector<Float, 3> red = space.upsample(Vector<Float, 3>(Float(0.5), Float(0.25), Float(0.125)));
            Vector<Float, 3> green = space.upsample(Vector<Float, 3>(Float(0.25), Float(0.5), Float(0.125)));
            Vector<Float, 3> blue = space.upsample(Vector<Float, 3>(Float(0.125), Float(0.25), Float(0.5)));

            EXPECT_NEAR(red[0], Float(31.5), tolerances::COARSE);
            EXPECT_NEAR(red[1], Float(15.75), tolerances::COARSE);
            EXPECT_NEAR(red[2], Float(31.5), tolerances::COARSE);

            EXPECT_NEAR(green[0], Float(131.5), tolerances::COARSE);
            EXPECT_NEAR(green[1], Float(31.5), tolerances::COARSE);
            EXPECT_NEAR(green[2], Float(15.75), tolerances::COARSE);

            EXPECT_NEAR(blue[0], Float(231.5), tolerances::COARSE);
            EXPECT_NEAR(blue[1], Float(31.5), tolerances::COARSE);
            EXPECT_NEAR(blue[2], Float(15.75), tolerances::COARSE);
        }

        TEST_F(ColorSpaceTest, UpsampleClampsInputToUnitCube) {
            ColorSpace space = makeUpsamplingSpace();

            Vector<Float, 3> bright = space.upsample(Vector<Float, 3>(2, 0, 0));

            EXPECT_NEAR(bright[0], 63, tolerances::COARSE);
            EXPECT_NEAR(bright[1], 0, tolerances::COARSE);
            EXPECT_NEAR(bright[2], 0, tolerances::COARSE);

            Vector<Float, 3> dark = space.upsample(Vector<Float, 3>(-1, -1, -1));

            EXPECT_NEAR(dark[0], 0, tolerances::COARSE);
            EXPECT_NEAR(dark[1], 0, tolerances::COARSE);
            EXPECT_NEAR(dark[2], 0, tolerances::COARSE);
        }

        TEST_F(ColorSpaceTest, UpsampleOfGrayReachesLastGridCell) {
            ColorSpace space = makeUpsamplingSpace();

            Vector<Float, 3> gray = space.upsample(Vector<Float, 3>(Float(0.5), Float(0.5), Float(0.5)));

            EXPECT_NEAR(gray[0], Float(31.5), tolerances::COARSE);
            EXPECT_NEAR(gray[1], 63, tolerances::COARSE);
            EXPECT_NEAR(gray[2], 63, tolerances::COARSE);
        }

        class BuilderTest : public WorldTest {
            protected:
                Mesh mesh;
                Object * objects;
                int count;

                void SetUp() override {
                    count = 8;

                    Vector<Float, 3> * vertices = allocator.allocate<Vector<Float, 3>>(3 * count);
                    Vector<int, 3> * faces = allocator.allocate<Vector<int, 3>>(count);

                    for (int i = 0; i < count; i++) {
                        vertices[3 * i] = Vector<Float, 3>(Float(3 * i), 0, 0);
                        vertices[3 * i + 1] = Vector<Float, 3>(Float(3 * i + 1), 0, 0);
                        vertices[3 * i + 2] = Vector<Float, 3>(Float(3 * i), Float(1 + i % 3), 0);

                        faces[i] = Vector<int, 3>(3 * i, 3 * i + 1, 3 * i + 2);
                    }

                    mesh = Mesh::makeTri(vertices, nullptr, nullptr, faces);

                    objects = allocator.allocate<Object>(count);

                    for (int i = 0; i < count; i++) objects[i] = Object::makeTri(nullptr, nullptr, nullptr, &mesh, i);
                }

                Object ** makeElements() {
                    Object ** elements = allocator.allocate<Object *>(count);

                    for (int i = 0; i < count; i++) elements[i] = objects + i;

                    return elements;
                }

                void collectLeafElements(const std::vector<BVHNode<Object>> & nodes, int index, std::vector<const Object *> & leafElements) {
                    const BVHNode<Object> & node = nodes[index];

                    if (node.isLeaf()) {
                        for (int i = 0; i < node.getCount(); i++) {
                            leafElements.push_back(node.getElement(i));

                            for (int j = 0; j < 3; j++) {
                                EXPECT_GE(node.getElement(i)->getMin()[j], node.getMin()[j]);
                                EXPECT_LE(node.getElement(i)->getMax()[j], node.getMax()[j]);
                            }
                        }

                        return;
                    }

                    for (int child : { index + 1, index + node.getCount() }) {
                        for (int j = 0; j < 3; j++) {
                            EXPECT_GE(nodes[child].getMin()[j], node.getMin()[j]);
                            EXPECT_LE(nodes[child].getMax()[j], node.getMax()[j]);
                        }

                        collectLeafElements(nodes, child, leafElements);
                    }
                }
        };

        TEST_F(BuilderTest, EmptyInputProducesNoNodes) {
            EXPECT_TRUE((Builder::buildBVH<Object, false>(nullptr, 0)).empty());
            EXPECT_TRUE((Builder::buildBVH<Object, false>(makeElements(), -1)).empty());
        }

        TEST_F(BuilderTest, SingleElementProducesLeaf) {
            Object ** elements = makeElements();

            std::vector<BVHNode<Object>> nodes = Builder::buildBVH<Object, false>(elements, 1);

            ASSERT_EQ(nodes.size(), 1u);

            EXPECT_TRUE(nodes[0].isLeaf());
            EXPECT_EQ(nodes[0].getCount(), 1);
            EXPECT_EQ(nodes[0].getElement(0), objects);

            expectEqual(nodes[0].getMin(), objects[0].getMin());
            expectEqual(nodes[0].getMax(), objects[0].getMax());
        }

        TEST_F(BuilderTest, RootBoundsEncloseAllElements) {
            std::vector<BVHNode<Object>> nodes = Builder::buildBVH<Object, false>(makeElements(), count);

            for (int i = 0; i < count; i++)
                for (int j = 0; j < 3; j++) {
                    EXPECT_LE(nodes[0].getMin()[j], objects[i].getMin()[j]);
                    EXPECT_GE(nodes[0].getMax()[j], objects[i].getMax()[j]);
                }
        }

        TEST_F(BuilderTest, EveryElementAppearsInExactlyOneLeaf) {
            std::vector<BVHNode<Object>> nodes = Builder::buildBVH<Object, false>(makeElements(), count);

            EXPECT_GT(nodes.size(), 1u);

            std::vector<const Object *> leafElements;

            collectLeafElements(nodes, 0, leafElements);

            ASSERT_EQ(int(leafElements.size()), count);

            for (int i = 0; i < count; i++) EXPECT_EQ(std::count(leafElements.begin(), leafElements.end(), objects + i), 1);
        }

        TEST_F(BuilderTest, CoincidentCentersCollapseIntoOneLeaf) {
            Object ** elements = allocator.allocate<Object *>(5);

            for (int i = 0; i < 5; i++) elements[i] = objects;

            std::vector<BVHNode<Object>> nodes = Builder::buildBVH<Object, false>(elements, 5);

            ASSERT_EQ(nodes.size(), 1u);

            EXPECT_TRUE(nodes[0].isLeaf());
            EXPECT_EQ(nodes[0].getCount(), 5);
        }

        TEST_F(BuilderTest, LightBVHPowerFollowsArea) {
            std::vector<BVHNode<Object>> nodes = Builder::buildBVH<Object, true>(makeElements(), count, true);

            Float totalArea = 0;

            for (int i = 0; i < count; i++) totalArea += objects[i].getArea();

            EXPECT_NEAR(nodes[0].getPower(), totalArea, tolerances::ACCUMULATED);
        }

        TEST_F(BuilderTest, LightBVHChildPowersSumToParentPower) {
            std::vector<BVHNode<Object>> nodes = Builder::buildBVH<Object, true>(makeElements(), count, true);

            for (int i = 0; i < int(nodes.size()); i++) {
                if (!nodes[i].isLeaf()) {
                    EXPECT_NEAR(nodes[i].getPower(), nodes[i + 1].getPower() + nodes[i + nodes[i].getCount()].getPower(), tolerances::ACCUMULATED);
                }
            }
        }

        TEST_F(BuilderTest, LightBVHLeafPowerSumsElementPowers) {
            std::vector<BVHNode<Object>> nodes = Builder::buildBVH<Object, true>(makeElements(), count, true);

            for (const BVHNode<Object> & node : nodes) {
                if (!node.isLeaf()) continue;

                Float power = 0;

                for (int i = 0; i < node.getCount(); i++) power += node.getElement(i)->getArea();

                EXPECT_NEAR(node.getPower(), power, tolerances::ACCUMULATED);
            }
        }

        TEST_F(BuilderTest, LightBVHWeighsElementsByEmission) {
            Material * materials[3] = { nullptr, nullptr, nullptr };

            for (int i = 0; i < 3; i++) materials[i] = allocator.make(Material::makeEmissive(makeSpectrumTexture(Float(i + 1))));

            Object * emitters = allocator.allocate<Object>(3);
            Object ** elements = allocator.allocate<Object *>(3);

            for (int i = 0; i < 3; i++) {
                emitters[i] = Object::makeTri(materials[i], nullptr, nullptr, &mesh, i);

                elements[i] = emitters + i;
            }

            std::vector<BVHNode<Object>> nodes = Builder::buildBVH<Object, true>(elements, 3);

            Float expected = 0;

            for (int i = 0; i < 3; i++) expected += emitters[i].getArea() * Float(i + 1);

            EXPECT_NEAR(nodes[0].getPower(), expected, tolerances::ACCUMULATED);
        }

        TEST_F(BuilderTest, InstanceBVHBoundsEncloseInstances) {
            Object * sphere = allocator.allocate<Object>(1);

            sphere[0] = Object::makeSphere(nullptr, nullptr, nullptr, 1);

            Instance * first = makeInstance(sphere, 1, nullptr, Vector<Float, 3>(-5, 0, 0));
            Instance * second = makeInstance(sphere, 1, nullptr, Vector<Float, 3>(5, 2, 0), Vector<Float, 3>(), Vector<Float, 3>(2, 2, 2));

            Instance ** elements = allocator.allocate<Instance *>(2);

            elements[0] = first;
            elements[1] = second;

            std::vector<BVHNode<Instance>> nodes = Builder::buildBVH<Instance, false>(elements, 2);

            ASSERT_EQ(nodes.size(), 3u);

            EXPECT_FALSE(nodes[0].isLeaf());
            EXPECT_TRUE(nodes[1].isLeaf());
            EXPECT_TRUE(nodes[2].isLeaf());

            for (int j = 0; j < 3; j++) {
                EXPECT_LE(nodes[0].getMin()[j], std::fmin(first->getMin()[j], second->getMin()[j]));
                EXPECT_GE(nodes[0].getMax()[j], std::fmax(first->getMax()[j], second->getMax()[j]));
            }
        }

        TEST_F(BuilderTest, SceneRadiusOfNothingIsOne) {
            EXPECT_EQ(Builder::computeSceneRadius(std::vector<Instance *>()), 1);
        }

        TEST_F(BuilderTest, SceneRadiusOfSingleObjectIsItsRadius) {
            Object * sphere = allocator.allocate<Object>(1);

            sphere[0] = Object::makeSphere(nullptr, nullptr, nullptr, 2);

            std::vector<Instance *> instances = { makeInstance(sphere, 1, nullptr, Vector<Float, 3>(1, 2, 3), Vector<Float, 3>(), Vector<Float, 3>(3, 3, 3)) };

            EXPECT_NEAR(Builder::computeSceneRadius(instances), 6, tolerances::STANDARD);
        }

        TEST_F(BuilderTest, SceneRadiusOfSeparatedObjectsSpansBoth) {
            Object * sphere = allocator.allocate<Object>(1);

            sphere[0] = Object::makeSphere(nullptr, nullptr, nullptr, 1);

            std::vector<Instance *> instances = { makeInstance(sphere, 1, nullptr, Vector<Float, 3>(-5, 0, 0)), makeInstance(sphere, 1, nullptr, Vector<Float, 3>(5, 0, 0)) };

            EXPECT_NEAR(Builder::computeSceneRadius(instances), 6, tolerances::ACCUMULATED);
        }

        TEST_F(BuilderTest, SceneRadiusIgnoresContainedObjects) {
            Object * spheres = allocator.allocate<Object>(2);

            spheres[0] = Object::makeSphere(nullptr, nullptr, nullptr, 1);
            spheres[1] = Object::makeSphere(nullptr, nullptr, nullptr, 10);

            std::vector<Instance *> small = { makeInstance(spheres, 1, nullptr, Vector<Float, 3>(1, 0, 0)), makeInstance(spheres + 1, 1) };
            std::vector<Instance *> large = { makeInstance(spheres + 1, 1), makeInstance(spheres, 1, nullptr, Vector<Float, 3>(1, 0, 0)) };

            EXPECT_NEAR(Builder::computeSceneRadius(small), 10, tolerances::ACCUMULATED);
            EXPECT_NEAR(Builder::computeSceneRadius(large), 10, tolerances::ACCUMULATED);
        }

        TEST_F(BuilderTest, OctahedralWeightsFollowTexelAverages) {
            Float values[4] = { 1, 1, 2, 4 };

            Background background = makeBackground(true, values, 2, 2);

            std::vector<Float> conditionalCDF, marginalCDF;

            Builder::buildCDF(background, conditionalCDF, marginalCDF);

            ASSERT_EQ(conditionalCDF.size(), 4u);
            ASSERT_EQ(marginalCDF.size(), 2u);

            EXPECT_NEAR(conditionalCDF[0], Float(0.5), tolerances::STANDARD);
            EXPECT_NEAR(conditionalCDF[1], 1, tolerances::STANDARD);
            EXPECT_NEAR(conditionalCDF[2], Float(1) / 3, tolerances::STANDARD);
            EXPECT_NEAR(conditionalCDF[3], 1, tolerances::STANDARD);

            EXPECT_NEAR(marginalCDF[0], Float(0.25), tolerances::STANDARD);
            EXPECT_NEAR(marginalCDF[1], 1, tolerances::STANDARD);
        }

        TEST_F(BuilderTest, EquirectangularWeightsRowsByJacobian) {
            Float values[8] = { 1, 1, 1, 1, 1, 1, 1, 1 };

            Background background = makeBackground(false, values, 2, 4);

            std::vector<Float> conditionalCDF, marginalCDF;

            Builder::buildCDF(background, conditionalCDF, marginalCDF);

            ASSERT_EQ(marginalCDF.size(), 4u);

            EXPECT_NEAR(marginalCDF[3], 1, tolerances::STANDARD);

            Float weights[4];
            Float total = 0;

            for (int y = 0; y < 4; y++) {
                weights[y] = 2 * background.jacobian((Float(y) + Float(0.5)) / 4);
                total += weights[y];
            }

            Float cumulative = 0;

            for (int y = 0; y < 4; y++) {
                cumulative += weights[y];

                EXPECT_NEAR(marginalCDF[y], cumulative / total, tolerances::STANDARD);
            }
        }

        TEST_F(BuilderTest, ZeroRowsFallBackToUniformConditional) {
            Float values[4] = { 0, 0, 1, 1 };

            Background background = makeBackground(true, values, 2, 2);

            std::vector<Float> conditionalCDF, marginalCDF;

            Builder::buildCDF(background, conditionalCDF, marginalCDF);

            EXPECT_NEAR(conditionalCDF[0], Float(0.5), tolerances::STANDARD);
            EXPECT_NEAR(conditionalCDF[1], 1, tolerances::STANDARD);
            EXPECT_EQ(marginalCDF[0], 0);
            EXPECT_NEAR(marginalCDF[1], 1, tolerances::STANDARD);
        }

        TEST_F(BuilderTest, BlackBackgroundFallsBackToUniform) {
            Float values[4] = { 0, 0, 0, 0 };

            Background background = makeBackground(true, values, 2, 2);

            std::vector<Float> conditionalCDF, marginalCDF;

            Builder::buildCDF(background, conditionalCDF, marginalCDF);

            EXPECT_NEAR(marginalCDF[0], Float(0.5), tolerances::STANDARD);
            EXPECT_NEAR(marginalCDF[1], 1, tolerances::STANDARD);
        }

        TEST_F(BuilderTest, ConstantBackgroundHasSingleEntry) {
            Background background = Background::makeOctahedral(makeSpectrumTexture(3), Vector<Float, 3>());

            std::vector<Float> conditionalCDF, marginalCDF;

            Builder::buildCDF(background, conditionalCDF, marginalCDF);

            ASSERT_EQ(conditionalCDF.size(), 1u);
            ASSERT_EQ(marginalCDF.size(), 1u);

            EXPECT_NEAR(conditionalCDF[0], 1, tolerances::STANDARD);
            EXPECT_NEAR(marginalCDF[0], 1, tolerances::STANDARD);
        }

        class DispatcherTest : public IntegratorTest {
            protected:
                std::ostringstream output;
                std::streambuf * original;

                void SetUp() override { original = std::cout.rdbuf(output.rdbuf()); }

                void TearDown() override { std::cout.rdbuf(original); }
        };

        TEST_F(DispatcherTest, RendersEveryPixelAndReportsProgress) {
            Film * film = makeFilm(4, 4);

            Background backgrounds[1] = { makeConstantBackground(1) };

            Scene scene = makeScene({}, {}, backgrounds, 1);

            Integrator integrator = makeIntegrator(film, &scene, 4);

            Float duration = Dispatcher::dispatch("lambda", allocator, &integrator);

            EXPECT_GE(duration, 0);

            EXPECT_NE(output.str().find("lambda: 0% complete"), std::string::npos);
            EXPECT_NE(output.str().find("lambda: 100% complete"), std::string::npos);

            for (int j = 0; j < 4; j++)
                for (int i = 0; i < 4; i++) EXPECT_GT(film->getPixel(i, j)[1], 0);
        }

        class WriterTest : public IOTest {
            protected:
                template <typename T>
                static T readEXRAttribute(const std::string & path, const std::string & name) {
                    EXRVersion version;
                    EXRHeader header;

                    T value = T();

                    InitEXRHeader(&header);

                    if (ParseEXRVersionFromFile(&version, path.c_str()) != TINYEXR_SUCCESS) return value;

                    const char * error = nullptr;

                    if (ParseEXRHeaderFromFile(&header, &version, path.c_str(), &error) != TINYEXR_SUCCESS) return value;

                    for (int i = 0; i < header.num_custom_attributes; i++)
                        if (name == header.custom_attributes[i].name) std::memcpy(&value, header.custom_attributes[i].value, sizeof(T));

                    FreeEXRHeader(&header);

                    return value;
                }
        };

        class ParserTest : public IOTest {
            protected:
                Integrator * parse(const std::string & scene) { return Parser::parseLRD(writeFile("scene.lrd", scene), allocator); }

                std::string parseFailure(const std::string & scene) {
                    std::string path = writeFile("scene.lrd", scene);

                    try {
                        Parser::parseLRD(path, allocator);
                    }
                    catch (const FileError & error) {
                        std::string message = error.format("");

                        return message.substr(message.find(path) + path.size() + 1);
                    }

                    return "no error";
                }
        };

        TEST_F(WriterTest, RejectsUnknownExtension) {
            Integrator integrator;

            EXPECT_THROW(Writer::writeImage("image.txt", &integrator, 0), FileError);
            EXPECT_THROW(Writer::writeImage("image", &integrator, 0), FileError);
        }

        TEST_F(WriterTest, ReportsInvalidExtensionAgainstOutputName) {
            Integrator integrator;

            try {
                Writer::writeImage("image.txt", &integrator, 0);

                FAIL();
            }
            catch (const FileError & error) {
                EXPECT_EQ(error.format("lambda"), "image.txt: invalid file extension");
            }
        }

        TEST_F(ParserTest, DefaultsOutputToExrBesideInput) {
            char program[] = "lambda";
            char scene[] = "scenes/box.lrd";

            char * argv[] = { program, scene };

            std::string parsedProgram, input, output;

            Parser::parseArguments(2, argv, parsedProgram, input, output);

            EXPECT_EQ(parsedProgram, "lambda");
            EXPECT_EQ(input, "scenes/box.lrd");
            EXPECT_EQ(output, "scenes/box.exr");
        }

        TEST_F(ParserTest, DefaultOutputKeepsInputStemCase) {
            char program[] = "lambda";
            char scene[] = "BOX.LRD";

            char * argv[] = { program, scene };

            std::string parsedProgram, input, output;

            Parser::parseArguments(2, argv, parsedProgram, input, output);

            EXPECT_EQ(output, "BOX.exr");
        }

        TEST_F(ParserTest, AcceptsExplicitOutputFormats) {
            char program[] = "lambda";
            char scene[] = "box.lrd";
            char outputs[4][16] = { "image.exr", "image.pfm", "image.png", "IMAGE.PNG" };

            for (int i = 0; i < 4; i++) {
                char * argv[] = { program, scene, outputs[i] };

                std::string parsedProgram, input, output;

                Parser::parseArguments(3, argv, parsedProgram, input, output);

                EXPECT_EQ(input, "box.lrd");
                EXPECT_EQ(output, outputs[i]);
            }
        }

        TEST_F(ParserTest, RejectsWrongArgumentCount) {
            char program[] = "lambda";
            char scene[] = "box.lrd";
            char image[] = "image.exr";
            char extra[] = "extra";

            std::string parsedProgram, input, output;

            char * none[] = { program };
            char * excessive[] = { program, scene, image, extra };

            EXPECT_THROW(Parser::parseArguments(0, none, parsedProgram, input, output), Error);
            EXPECT_THROW(Parser::parseArguments(1, none, parsedProgram, input, output), Error);
            EXPECT_THROW(Parser::parseArguments(4, excessive, parsedProgram, input, output), Error);
        }

        TEST_F(ParserTest, RecordsProgramEvenWhenArgumentsAreInvalid) {
            char program[] = "lambda";

            char * argv[] = { program };

            std::string parsedProgram, input, output;

            EXPECT_THROW(Parser::parseArguments(1, argv, parsedProgram, input, output), Error);

            EXPECT_EQ(parsedProgram, "lambda");
        }

        TEST_F(ParserTest, RejectsInputWithWrongExtension) {
            char program[] = "lambda";
            char scene[] = "box.txt";

            char * argv[] = { program, scene };

            std::string parsedProgram, input, output;

            EXPECT_THROW(Parser::parseArguments(2, argv, parsedProgram, input, output), FileError);
        }

        TEST_F(ParserTest, RejectsOutputWithWrongExtension) {
            char program[] = "lambda";
            char scene[] = "box.lrd";
            char image[] = "image.jpg";

            char * argv[] = { program, scene, image };

            std::string parsedProgram, input, output;

            EXPECT_THROW(Parser::parseArguments(3, argv, parsedProgram, input, output), FileError);
        }

        TEST_F(ParserTest, MissingSceneFileIsReported) {
            EXPECT_THROW(Parser::parseLRD("missing-scene-file.lrd", allocator), FileError);
        }

        TEST_F(WriterTest, PFMHasHeaderAndLinearRGBRowsFromBottomToTop) {
            Film * film = makeFilledFilm(3, 2);

            Integrator integrator = makeIntegrator(film, &emptyScene);

            std::string path = makePath("image.pfm");

            Writer::writeImage(path, &integrator, 0);

            std::ifstream file(path, std::ios::binary);

            std::string magic;
            int width, height;
            float scale;

            file >> magic >> width >> height >> scale;
            file.get();

            EXPECT_EQ(magic, "PF");
            EXPECT_EQ(width, 3);
            EXPECT_EQ(height, 2);
            EXPECT_EQ(scale, -1);

            for (int j = 1; j >= 0; j--)
                for (int i = 0; i < 3; i++) {
                    float stored[3];

                    file.read(reinterpret_cast<char *>(stored), sizeof(stored));

                    ASSERT_TRUE(file.good());

                    Vector<float, 3> expected = film->getSpace()->toRGB(film->getPixel(i, j));

                    for (int k = 0; k < 3; k++) EXPECT_EQ(stored[k], expected[k]);
                }

            file.get();

            EXPECT_TRUE(file.eof());
        }

        TEST_F(WriterTest, PFMKeepsValuesAboveOne) {
            Film * film = makeFilledFilm(3, 2);

            Integrator integrator = makeIntegrator(film, &emptyScene);

            std::string path = makePath("image.pfm");

            Writer::writeImage(path, &integrator, 0);

            std::ifstream file(path, std::ios::binary);

            std::string header;

            std::getline(file, header);
            std::getline(file, header);
            std::getline(file, header);

            float largest = 0;

            for (int i = 0; i < 18; i++) {
                float value;

                file.read(reinterpret_cast<char *>(&value), sizeof(value));

                largest = std::fmax(largest, value);
            }

            EXPECT_GT(largest, 1);
        }

        TEST_F(WriterTest, PNGStoresClampedSRGBEncodedPixels) {
            Film * film = makeFilledFilm(3, 2);

            Integrator integrator = makeIntegrator(film, &emptyScene);

            std::string path = makePath("image.png");

            Writer::writeImage(path, &integrator, 0);

            int width, height, channels;

            unsigned char * pixels = stbi_load(path.c_str(), &width, &height, &channels, 3);

            ASSERT_NE(pixels, nullptr);

            EXPECT_EQ(width, 3);
            EXPECT_EQ(height, 2);

            for (int j = 0; j < 2; j++)
                for (int i = 0; i < 3; i++) {
                    Vector<Float, 3> color = ColorSpace::sRGB()->toRGB(film->getPixel(i, j));

                    for (int k = 0; k < 3; k++) EXPECT_NEAR(Float(pixels[(j * 3 + i) * 3 + k]), utils::sRGBEncode(utils::clamp(color[k], Float(0), Float(1))) * 255, 1);
                }

            stbi_image_free(pixels);
        }

        TEST_F(WriterTest, EXRStoresHalfPrecisionRGBRowsFromTopToBottom) {
            Film * film = makeFilledFilm(3, 2);

            Integrator integrator = makeIntegrator(film, &emptyScene);

            std::string path = makePath("image.exr");

            Writer::writeImage(path, &integrator, 0);

            float * rgba = nullptr;
            int width, height;
            const char * error = nullptr;

            ASSERT_EQ(LoadEXR(&rgba, &width, &height, path.c_str(), &error), TINYEXR_SUCCESS);

            EXPECT_EQ(width, 3);
            EXPECT_EQ(height, 2);

            for (int j = 0; j < 2; j++)
                for (int i = 0; i < 3; i++) {
                    Vector<float, 3> expected = film->getSpace()->toRGB(film->getPixel(i, j));

                    for (int k = 0; k < 3; k++) EXPECT_NEAR(rgba[(j * 3 + i) * 4 + k], expected[k], tolerances::COARSE * std::fmax(Float(1), expected[k]));
                }

            free(rgba);
        }

        TEST_F(WriterTest, EXRRecordsRenderMetadata) {
            Film * film = makeFilledFilm(3, 2);

            Integrator integrator = makeIntegrator(film, &emptyScene, 25);

            std::string path = makePath("image.exr");

            Writer::writeImage(path, &integrator, Float(1.5));

            EXPECT_EQ(readEXRAttribute<int>(path, "samplesPerPixel"), 25);
            EXPECT_EQ(readEXRAttribute<float>(path, "renderTimeSeconds"), Float(1.5));
            EXPECT_EQ(readEXRAttribute<float>(path, "lambdaMin"), film->getLambdaMin());
            EXPECT_EQ(readEXRAttribute<float>(path, "lambdaMax"), film->getLambdaMax());
        }

        TEST_F(WriterTest, UnwritableLocationThrows) {
            Film * film = makeFilledFilm(3, 2);

            Integrator integrator = makeIntegrator(film, &emptyScene);

            EXPECT_THROW(Writer::writeImage("missing-directory/image.pfm", &integrator, 0), FileError);
            EXPECT_THROW(Writer::writeImage("missing-directory/image.png", &integrator, 0), FileError);
            EXPECT_THROW(Writer::writeImage("missing-directory/image.exr", &integrator, 0), FileError);
        }

        TEST_F(ParserTest, ParsesRenderConfiguration) {
            Integrator * integrator = parse("Render srgb 4 2 16 3 400 700 7\nCamera perspective none (0 0 5) (0 0 0) (0 1 0) 1.0\n");

            ASSERT_NE(integrator, nullptr);

            EXPECT_EQ(integrator->getFilm()->getWidth(), 4);
            EXPECT_EQ(integrator->getFilm()->getHeight(), 2);
            EXPECT_EQ(integrator->getFilm()->getSpace(), ColorSpace::sRGB());
            EXPECT_EQ(integrator->getFilm()->getLambdaMin(), 400);
            EXPECT_EQ(integrator->getFilm()->getLambdaMax(), 700);
            EXPECT_EQ(integrator->getSamples(), 16);
        }

        TEST_F(ParserTest, ParsesColorSpaceByName) {
            EXPECT_EQ(parse("Render rec2020 1 1 1 0 400 700 0\nCamera spherical none (0 0 0) (0 0 -1) (0 1 0)\n")->getFilm()->getSpace(), ColorSpace::Rec2020());
            EXPECT_EQ(parse("Render aces2065-1 1 1 1 0 400 700 0\nCamera spherical none (0 0 0) (0 0 -1) (0 1 0)\n")->getFilm()->getSpace(), ColorSpace::ACES2065());
        }

        TEST_F(ParserTest, ParsesCameraTypes) {
            SampledSpectrum<Float> lambdas(550);

            Integrator * perspective = parse("Render srgb 2 2 1 0 400 700 0\nCamera perspective none (0 0 5) (0 0 0) (0 1 0) 1.0\n");
            Integrator * orthographic = parse("Render srgb 2 2 1 0 400 700 0\nCamera orthographic none (0 0 5) (0 0 0) (0 1 0) 4\n");
            Integrator * spherical = parse("Render srgb 2 2 1 0 400 700 0\nCamera spherical none (0 0 5) (0 0 0) (0 1 0)\n");

            expectNear(perspective->getCamera()->getRay(Float(0.5), Float(0.5), lambdas).getDirection(), Vector<Float, 3>(0, 0, -1));
            expectNear(orthographic->getCamera()->getRay(0, 0, lambdas).getOrigin(), Vector<Float, 3>(-2, 2, 5));
            expectNear(spherical->getCamera()->getRay(Float(0.5), 0, lambdas).getDirection(), Vector<Float, 3>(0, 1, 0));
        }

        TEST_F(ParserTest, IgnoresBlankLinesAndComments) {
            Integrator * integrator = parse("\n# a leading comment\nRender srgb 1 1 1 0 400 700 0 # trailing comment\n\n   \nCamera spherical none (0 0 0) (0 0 -1) (0 1 0)\n");

            EXPECT_NE(integrator, nullptr);
        }

        TEST_F(ParserTest, SceneWithoutBackgroundRendersBlack) {
            Integrator * integrator = parse("Render srgb 4 4 4 1 400 700 0\nCamera perspective none (0 0 5) (0 0 0) (0 1 0) 1.0\n");

            render(*integrator);

            for (int j = 0; j < 4; j++)
                for (int i = 0; i < 4; i++) expectEqual(integrator->getFilm()->getPixel(i, j), Vector<Float, 3>());
        }

        TEST_F(ParserTest, UniformBackgroundRendersItsRadiance) {
            Integrator * integrator = parse("Render srgb 8 8 16 1 360 830 0\nCamera perspective none (0 0 5) (0 0 0) (0 1 0) 1.0\nBackground octahedral 1.5 (0 0 0)\n");

            render(*integrator);

            expectMeanLuminance(*integrator->getFilm(), Float(1.5));
        }

        TEST_F(ParserTest, EmissiveInstanceRendersItsEmission) {
            Integrator * integrator = parse("Render srgb 8 8 16 1 360 830 0\nCamera perspective none (0 0 0) (0 0 -1) (0 1 0) 1.0\nMaterial lamp emissive 2.0\nObject shell sphere lamp none none 10.0\nInstance shell none none none (0 0 0) (0 0 0) (1 1 1)\n");

            render(*integrator);

            expectMeanLuminance(*integrator->getFilm(), 2);
        }

        TEST_F(ParserTest, InstanceMaterialOverridesObjectMaterial) {
            Integrator * integrator = parse("Render srgb 8 8 16 1 360 830 0\nCamera perspective none (0 0 0) (0 0 -1) (0 1 0) 1.0\nMaterial lamp emissive 2.0\nObject shell sphere none none none 10.0\nInstance shell lamp none none (0 0 0) (0 0 0) (1 1 1)\n");

            render(*integrator);

            expectMeanLuminance(*integrator->getFilm(), 2);
        }

        TEST_F(ParserTest, ObjectGroupCanBeInstancedRepeatedly) {
            Integrator * integrator = parse("Render srgb 2 2 1 1 360 830 0\nCamera perspective none (0 0 5) (0 0 0) (0 1 0) 1.0\nMaterial white diffuse 0.5 [] [] [] [] [] []\nObject ball sphere white none none 1.0\nInstance ball none none none (0 0 0) (0 0 0) (1 1 1)\nInstance ball none none none (3 0 0) (0 0 0) (1 1 1)\n");

            EXPECT_NE(integrator, nullptr);
        }

        TEST_F(ParserTest, NamedTexturesAreReusable) {
            Integrator * integrator = parse("Render srgb 2 2 1 1 360 830 0\nCamera perspective none (0 0 5) (0 0 0) (0 1 0) 1.0\nTexture bands scalar checker 0.2 0.8 4 4 0 0\nMaterial painted diffuse bands [] [] [] [] [] []\nObject ball sphere painted none none 1.0\nInstance ball none none none (0 0 0) (0 0 0) (1 1 1)\n");

            EXPECT_NE(integrator, nullptr);
        }

        TEST_F(ParserTest, ParsesFluorescentMaterial) {
            Integrator * integrator = parse("Render srgb 2 2 1 1 360 830 0\nCamera perspective none (0 0 5) (0 0 0) (0 1 0) 1.0\nMaterial dye diffuse 0.3 [400] [100] [40] [40] [0] [0.5]\nObject ball sphere dye none none 1.0\nInstance ball none none none (0 0 0) (0 0 0) (1 1 1)\n");

            EXPECT_NE(integrator, nullptr);
        }

        TEST_F(ParserTest, RejectsFluorophoreArraysOfDifferentLengths) {
            EXPECT_EQ(parseFailure("Render srgb 2 2 1 1 360 830 0\nCamera perspective none (0 0 5) (0 0 0) (0 1 0) 1.0\nMaterial dye diffuse 0.3 [400 500] [100] [40] [40] [0] [0.5]\n"), "3:40: 'stokesShifts' must have 2 entries, got 1");
        }

        TEST_F(ParserTest, RejectsFluorescenceThatExceedsEnergyConservation) {
            EXPECT_EQ(parseFailure("Render srgb 2 2 1 1 360 830 0\nCamera perspective none (0 0 5) (0 0 0) (0 1 0) 1.0\nMaterial dye diffuse 0.95 [400] [100] [40] [40] [0] [1]\n"), "3:55: 'reflectance' plus fluorescence must not exceed 1.000000, got 1.951969");
        }

        TEST_F(ParserTest, RejectsFluorophoreEfficiencyAboveOne) {
            EXPECT_EQ(parseFailure("Render srgb 2 2 1 1 360 830 0\nCamera perspective none (0 0 5) (0 0 0) (0 1 0) 1.0\nMaterial dye diffuse 0.3 [400] [100] [40] [40] [0] [1.5]\n"), "3:56: 'efficiencies' must be at most 1.000000, got 1.500000");
        }

        TEST_F(ParserTest, RejectsCommandBeforeRender) {
            EXPECT_EQ(parseFailure("Camera perspective none (0 0 5) (0 0 0) (0 1 0) 1.0\n"), "1:1: expected \"Render\", got \"Camera\"");
        }

        TEST_F(ParserTest, RejectsSceneWithoutCamera) {
            EXPECT_EQ(parseFailure("Render srgb 4 4 1 0 400 700 0\n"), "1:1: expected \"Camera\"");
        }

        TEST_F(ParserTest, RejectsEmptyScene) {
            EXPECT_EQ(parseFailure("# nothing here\n"), "1:1: expected \"Render\"");
        }

        TEST_F(ParserTest, RejectsNonSquareSampleCount) {
            EXPECT_EQ(parseFailure("Render srgb 4 4 3 0 400 700 0\n"), "1:17: 'samples' must be a perfect square, got 3");
        }

        TEST_F(ParserTest, RejectsNonPositiveImageSize) {
            EXPECT_EQ(parseFailure("Render srgb 0 4 1 0 400 700 0\n"), "1:13: 'width' must be positive, got 0");
        }

        TEST_F(ParserTest, RejectsInvertedWavelengthRange) {
            EXPECT_EQ(parseFailure("Render srgb 4 4 1 0 700 400 0\n"), "1:25: 'lambdaMin' must be less than 'lambdaMax', got 700.000000 and 400.000000");
        }

        TEST_F(ParserTest, RejectsUnknownColorSpace) {
            EXPECT_EQ(parseFailure("Render xyz 4 4 1 0 400 700 0\n"), "1:8: 'space' must be \"srgb\", \"rec2020\", or \"aces2065-1\", got \"xyz\"");
        }

        TEST_F(ParserTest, RejectsUndefinedMaterialReference) {
            EXPECT_EQ(parseFailure("Render srgb 4 4 1 0 400 700 0\nCamera spherical none (0 0 0) (0 0 -1) (0 1 0)\nObject ball sphere missing none none 1.0\n"), "3:20: 'material' is not defined");
        }

        TEST_F(ParserTest, RejectsDuplicateMaterialName) {
            EXPECT_EQ(parseFailure("Render srgb 4 4 1 0 400 700 0\nCamera spherical none (0 0 0) (0 0 -1) (0 1 0)\nMaterial m emissive 1.0\nMaterial m emissive 2.0\n"), "4:10: 'name' is already defined");
        }

        TEST_F(ParserTest, RejectsDegenerateCameraOrientation) {
            EXPECT_EQ(parseFailure("Render srgb 4 4 1 0 400 700 0\nCamera perspective none (0 0 0) (0 1 0) (0 1 0) 1.0\n"), "2:41: 'lookAt' minus 'position' and 'up' must be non-parallel");
        }

        TEST_F(ParserTest, ReportsLineAndColumnOfMalformedValue) {
            EXPECT_EQ(parseFailure("Render srgb four 4 1 0 400 700 0\n"), "1:13: 'width' is missing or invalid");
        }
    }
}