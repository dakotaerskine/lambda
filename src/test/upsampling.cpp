#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

#include <gtest/gtest.h>

#include "core/allocator.h"
#include "core/color.h"
#include "core/constants.h"
#include "core/error.h"
#include "core/utils.h"
#include "math/vector.h"
#include "test/test.h"
#include "test/tolerances.h"
#include "upsampling/loader.h"
#include "upsampling/solver.h"

namespace lambda {
    namespace {
        class LoaderTest : public IOTest {
            protected:
                float * scale;
                float (* table)[constants::UPSAMPLING_RESOLUTION][constants::UPSAMPLING_RESOLUTION][constants::UPSAMPLING_RESOLUTION][3];

                void SetUp() override {
                    scale = allocator.allocate<float>(constants::UPSAMPLING_RESOLUTION);
                    table = allocator.allocate<float[constants::UPSAMPLING_RESOLUTION][constants::UPSAMPLING_RESOLUTION][constants::UPSAMPLING_RESOLUTION][3]>(3);

                    for (int i = 0; i < constants::UPSAMPLING_RESOLUTION; i++) scale[i] = float(i) / float(constants::UPSAMPLING_RESOLUTION - 1);

                    for (int c = 0; c < 3; c++)
                        for (int i = 0; i < constants::UPSAMPLING_RESOLUTION; i++)
                            for (int j = 0; j < constants::UPSAMPLING_RESOLUTION; j++)
                                for (int k = 0; k < constants::UPSAMPLING_RESOLUTION; k++)
                                    for (int m = 0; m < 3; m++) table[c][i][j][k][m] = float(((c * constants::UPSAMPLING_RESOLUTION + i) * constants::UPSAMPLING_RESOLUTION + j) * constants::UPSAMPLING_RESOLUTION + k) * float(m + 1);
                }

                bool matchesSource(const float * loadedScale, float (* loadedTable)[constants::UPSAMPLING_RESOLUTION][constants::UPSAMPLING_RESOLUTION][constants::UPSAMPLING_RESOLUTION][3]) const {
                    return std::memcmp(loadedScale, scale, sizeof(float) * constants::UPSAMPLING_RESOLUTION) == 0 && std::memcmp(loadedTable, table, sizeof(float) * 3 * constants::UPSAMPLING_RESOLUTION * constants::UPSAMPLING_RESOLUTION * constants::UPSAMPLING_RESOLUTION * 3) == 0;
                }
        };

        TEST_F(LoaderTest, LoadingMissingFileFails) {
            EXPECT_FALSE(Loader::loadUpsampling("missing-upsampling-file.coeff", scale, table));
        }

        TEST_F(LoaderTest, SavingToUnwritableLocationThrows) {
            EXPECT_THROW(Loader::saveUpsampling("missing-directory/upsampling.coeff", scale, table), FileError);
        }

        TEST_F(LoaderTest, SavedFileHasMagicResolutionAndExpectedSize) {
            std::string path = makePath("upsampling.coeff");

            Loader::saveUpsampling(path, scale, table);

            std::ifstream file(path, std::ios::binary);

            char magic[4];
            uint32_t resolution;

            file.read(magic, 4);
            file.read(reinterpret_cast<char *>(&resolution), sizeof(resolution));

            EXPECT_EQ(std::string(magic, 4), "SPEC");
            EXPECT_EQ(resolution, uint32_t(constants::UPSAMPLING_RESOLUTION));
            EXPECT_EQ(std::filesystem::file_size(path), 4 + sizeof(uint32_t) + sizeof(float) * constants::UPSAMPLING_RESOLUTION + sizeof(float) * 3 * constants::UPSAMPLING_RESOLUTION * constants::UPSAMPLING_RESOLUTION * constants::UPSAMPLING_RESOLUTION * 3);
        }

        TEST_F(LoaderTest, SavedUpsamplingLoadsBackIdentically) {
            std::string path = makePath("upsampling.coeff");

            Loader::saveUpsampling(path, scale, table);

            float * loadedScale = allocator.allocate<float>(constants::UPSAMPLING_RESOLUTION);
            float (* loadedTable)[constants::UPSAMPLING_RESOLUTION][constants::UPSAMPLING_RESOLUTION][constants::UPSAMPLING_RESOLUTION][3] = allocator.allocate<float[constants::UPSAMPLING_RESOLUTION][constants::UPSAMPLING_RESOLUTION][constants::UPSAMPLING_RESOLUTION][3]>(3);

            ASSERT_TRUE(Loader::loadUpsampling(path, loadedScale, loadedTable));

            EXPECT_TRUE(matchesSource(loadedScale, loadedTable));
        }

        TEST_F(LoaderTest, LoadingRejectsWrongMagic) {
            std::string path = makePath("upsampling.coeff");

            Loader::saveUpsampling(path, scale, table);

            std::fstream file(path, std::ios::in | std::ios::out | std::ios::binary);

            file.write("XXXX", 4);
            file.close();

            EXPECT_FALSE(Loader::loadUpsampling(path, allocator.allocate<float>(constants::UPSAMPLING_RESOLUTION), allocator.allocate<float[constants::UPSAMPLING_RESOLUTION][constants::UPSAMPLING_RESOLUTION][constants::UPSAMPLING_RESOLUTION][3]>(3)));
        }

        TEST_F(LoaderTest, LoadingRejectsWrongResolution) {
            std::string path = makePath("upsampling.coeff");

            Loader::saveUpsampling(path, scale, table);

            std::fstream file(path, std::ios::in | std::ios::out | std::ios::binary);

            uint32_t resolution = constants::UPSAMPLING_RESOLUTION / 2;

            file.seekp(4);
            file.write(reinterpret_cast<const char *>(&resolution), sizeof(resolution));
            file.close();

            EXPECT_FALSE(Loader::loadUpsampling(path, allocator.allocate<float>(constants::UPSAMPLING_RESOLUTION), allocator.allocate<float[constants::UPSAMPLING_RESOLUTION][constants::UPSAMPLING_RESOLUTION][constants::UPSAMPLING_RESOLUTION][3]>(3)));
        }

        TEST_F(LoaderTest, LoadingRejectsTruncatedFile) {
            std::string path = makePath("upsampling.coeff");

            Loader::saveUpsampling(path, scale, table);

            std::filesystem::resize_file(path, std::filesystem::file_size(path) - 1);

            EXPECT_FALSE(Loader::loadUpsampling(path, allocator.allocate<float>(constants::UPSAMPLING_RESOLUTION), allocator.allocate<float[constants::UPSAMPLING_RESOLUTION][constants::UPSAMPLING_RESOLUTION][constants::UPSAMPLING_RESOLUTION][3]>(3)));
        }

        TEST_F(LoaderTest, LoadingRejectsEmptyFile) {
            EXPECT_FALSE(Loader::loadUpsampling(writeFile("upsampling.coeff", ""), allocator.allocate<float>(constants::UPSAMPLING_RESOLUTION), allocator.allocate<float[constants::UPSAMPLING_RESOLUTION][constants::UPSAMPLING_RESOLUTION][constants::UPSAMPLING_RESOLUTION][3]>(3)));
        }

        class SolverTest : public ::testing::Test {
            protected:
                inline static Allocator allocator;

                inline static ColorSpace space;
                inline static float * scale;
                inline static float (* table)[constants::UPSAMPLING_RESOLUTION][constants::UPSAMPLING_RESOLUTION][constants::UPSAMPLING_RESOLUTION][3];

                static void SetUpTestSuite() {
                    space = ColorSpace(constants::CHROMATICITIES_SRGB, constants::CIE_D65);
                    scale = allocator.allocate<float>(constants::UPSAMPLING_RESOLUTION);
                    table = allocator.allocate<float[constants::UPSAMPLING_RESOLUTION][constants::UPSAMPLING_RESOLUTION][constants::UPSAMPLING_RESOLUTION][3]>(3);

                    Solver::solveGrid(&space, scale, table);
                }

                static Vector<double, 3> reconstruct(const float * coefficients) {
                    double illuminantIntegral = 0;

                    for (int i = 0; i < constants::CIE_LAMBDA_BINS; i++) illuminantIntegral += constants::CIE_XYZ_1931[i][1] * space.illuminant(i + constants::CIE_LAMBDA_MIN) * weight(i);

                    Vector<double, 3> color;

                    for (int i = 0; i < constants::CIE_LAMBDA_BINS; i++) {
                        double lambda = i + constants::CIE_LAMBDA_MIN;
                        double x = (double(coefficients[0]) * lambda + double(coefficients[1])) * lambda + double(coefficients[2]);

                        color += space.toRGB(Vector<double, 3>(constants::CIE_XYZ_1931[i])) * (space.illuminant(i + constants::CIE_LAMBDA_MIN) / illuminantIntegral * weight(i) * utils::sigmoid(x));
                    }

                    return color;
                }

                static Vector<double, 3> colorAt(int channel, int k, int index, int j) {
                    Vector<double, 3> color;

                    color[channel] = scale[k];
                    color[(channel + 1) % 3] = double(j) / (constants::UPSAMPLING_RESOLUTION - 1) * scale[k];
                    color[(channel + 2) % 3] = double(index) / (constants::UPSAMPLING_RESOLUTION - 1) * scale[k];

                    return color;
                }

            private:
                static double weight(int i) { return (i == 0 || i == constants::CIE_LAMBDA_BINS - 1) ? 1.0 / 3.0 : (i % 2 == 1 ? 4.0 / 3.0 : 2.0 / 3.0); }
        };

        TEST_F(SolverTest, UnitSpectrumUnderTheSpaceIlluminantIsWhite) {
            float coefficients[3] = { 0, 0, 1e4f };

            Vector<double, 3> color = reconstruct(coefficients);

            for (int i = 0; i < 3; i++) EXPECT_NEAR(color[i], 1, tolerances::COARSE * 10);
        }

        TEST_F(SolverTest, HalfSpectrumUnderTheSpaceIlluminantIsMiddleGray) {
            float coefficients[3] = { 0, 0, 0 };

            Vector<double, 3> color = reconstruct(coefficients);

            for (int i = 0; i < 3; i++) EXPECT_NEAR(color[i], 0.5, tolerances::COARSE * 10);
        }

        TEST_F(SolverTest, ZeroSpectrumIsBlack) {
            float coefficients[3] = { 0, 0, -1e4f };

            Vector<double, 3> color = reconstruct(coefficients);

            for (int i = 0; i < 3; i++) EXPECT_NEAR(color[i], 0, tolerances::COARSE * 10);
        }

        TEST_F(SolverTest, DISABLED_ScaleFollowsDoubleSmoothStep) {
            for (int i = 0; i < constants::UPSAMPLING_RESOLUTION; i++) {
                double t = double(i) / (constants::UPSAMPLING_RESOLUTION - 1);

                EXPECT_NEAR(scale[i], utils::smoothStep(utils::smoothStep(t)), tolerances::FINE);
            }
        }

        TEST_F(SolverTest, DISABLED_ScaleSpansUnitIntervalIncreasing) {
            EXPECT_EQ(scale[0], 0);
            EXPECT_EQ(scale[constants::UPSAMPLING_RESOLUTION - 1], 1);

            for (int i = 1; i < constants::UPSAMPLING_RESOLUTION; i++) EXPECT_GT(scale[i], scale[i - 1]);
        }

        TEST_F(SolverTest, DISABLED_GridReproducesSampledColors) {
            int indices[2] = { constants::UPSAMPLING_RESOLUTION / 3, 2 * constants::UPSAMPLING_RESOLUTION / 3 };
            int levels[3] = { constants::UPSAMPLING_RESOLUTION / 3, 2 * constants::UPSAMPLING_RESOLUTION / 3, constants::UPSAMPLING_RESOLUTION - 1 };

            for (int channel = 0; channel < 3; channel++)
                for (int k : levels)
                    for (int index : indices)
                        for (int j : indices) {
                            Vector<double, 3> expected = colorAt(channel, k, index, j);
                            Vector<double, 3> actual = reconstruct(table[channel][k][index][j]);

                            for (int i = 0; i < 3; i++) EXPECT_NEAR(actual[i], expected[i], 0.02);
                        }
        }

        TEST_F(SolverTest, DISABLED_GrayColorsHaveFlatSpectra) {
            for (int k : { constants::UPSAMPLING_RESOLUTION / 3, 2 * constants::UPSAMPLING_RESOLUTION / 3, constants::UPSAMPLING_RESOLUTION - 1 }) {
                const float * coefficients = table[0][k][constants::UPSAMPLING_RESOLUTION - 1][constants::UPSAMPLING_RESOLUTION - 1];

                double previous = 0;

                for (int lambda = constants::CIE_LAMBDA_MIN; lambda <= constants::CIE_LAMBDA_MAX; lambda += 10) {
                    double x = (double(coefficients[0]) * lambda + double(coefficients[1])) * lambda + double(coefficients[2]);
                    double s = utils::sigmoid(x);

                    if (lambda > constants::CIE_LAMBDA_MIN) {
                        EXPECT_NEAR(s, previous, 0.01);
                    }

                    previous = s;
                }
            }
        }
    }
}