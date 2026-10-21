#pragma once

#include "core/allocator.h"
#include "core/constants.h"
#include "core/parser.h"
#include "core/platform.h"
#include "core/utils.h"
#include "core/writer.h"
#include "math/matrix.h"

#ifndef LAMBDA_DATA_DIRECTORY
    #define LAMBDA_DATA_DIRECTORY "data"
#endif

namespace lambda {
    class Tables {
        public:
            static void initialize(Allocator & allocator) {
                constants::PERMUTATION = allocator.allocate(&constants::h_PERMUTATION[0], constants::PERMUTATION_SIZE);
                constants::CIE_XYZ_1931 = allocator.allocate(&constants::h_CIE_XYZ_1931[0][0], constants::CIE_LAMBDA_BINS * 3);

                float * upsamplingScaleSRGB = allocator.allocate<float>(constants::UPSAMPLING_RESOLUTION);
                float * upsamplingLUTSRGB = allocator.allocate<float>(3 * constants::UPSAMPLING_RESOLUTION * constants::UPSAMPLING_RESOLUTION * constants::UPSAMPLING_RESOLUTION * 3);

                loadUpsamplingTables(LAMBDA_DATA_DIRECTORY "/srgb.coeff", ColorSpace::SRGB, upsamplingScaleSRGB, upsamplingLUTSRGB);

                constants::UPSAMPLING_SCALE_SRGB = upsamplingScaleSRGB;
                constants::UPSAMPLING_LUT_SRGB = upsamplingLUTSRGB;

                float * upsamplingScaleRec2020 = allocator.allocate<float>(constants::UPSAMPLING_RESOLUTION);
                float * upsamplingLUTRec2020 = allocator.allocate<float>(3 * constants::UPSAMPLING_RESOLUTION * constants::UPSAMPLING_RESOLUTION * constants::UPSAMPLING_RESOLUTION * 3);

                loadUpsamplingTables(LAMBDA_DATA_DIRECTORY "/rec2020.coeff", ColorSpace::REC2020, upsamplingScaleRec2020, upsamplingLUTRec2020);

                constants::UPSAMPLING_SCALE_REC2020 = upsamplingScaleRec2020;
                constants::UPSAMPLING_LUT_REC2020 = upsamplingLUTRec2020;

                float * upsamplingScaleACES2065 = allocator.allocate<float>(constants::UPSAMPLING_RESOLUTION);
                float * upsamplingLUTACES2065 = allocator.allocate<float>(3 * constants::UPSAMPLING_RESOLUTION * constants::UPSAMPLING_RESOLUTION * constants::UPSAMPLING_RESOLUTION * 3);

                loadUpsamplingTables(LAMBDA_DATA_DIRECTORY "/aces2065-1.coeff", ColorSpace::ACES2065, upsamplingScaleACES2065, upsamplingLUTACES2065);

                constants::UPSAMPLING_SCALE_ACES2065 = upsamplingScaleACES2065;
                constants::UPSAMPLING_LUT_ACES2065 = upsamplingLUTACES2065;
            }

        private:
            static void loadUpsamplingTables(const std::string & file, ColorSpace space, float * scale, float * lut) {
                if (!Parser::parseUpsamplingTables(file, scale, lut)) {
                    solveGrid(space, scale, lut);

                    Writer::writeUpsamplingTables(file, scale, lut);
                }
            }

            static Vector<double, 3> residual(ColorSpace space, const Vector<double, 3> & coefficients, const Vector<double, 3> & color, const double rgbTable[3][constants::CIE_LAMBDA_BINS], const Vector<double, 3> & whitepoint) {
                Vector<double, 3> out(0, 0, 0);

                double inverseRange = 1.0 / (constants::CIE_LAMBDA_MAX - constants::CIE_LAMBDA_MIN);

                for (int i = 0; i < constants::CIE_LAMBDA_BINS; i++) {
                    double lambda = i * inverseRange;
                    double x = (coefficients[0] * lambda + coefficients[1]) * lambda + coefficients[2];
                    double s = 0.5 + 0.5 * x / std::sqrt(1 + x * x);

                    for (int j = 0; j < 3; j++) out[j] += rgbTable[j][i] * s;
                }

                out = utils::cieLab(space, out, whitepoint);

                Vector<double, 3> res;

                for (int i = 0; i < 3; i++) res[i] = color[i];

                res = utils::cieLab(space, res, whitepoint);

                for (int i = 0; i < 3; i++) res[i] -= out[i];

                return res;
            }

            static double solveLM(ColorSpace space, const Vector<double, 3> & color, Vector<double, 3> & coefficients, const double rgbTable[3][constants::CIE_LAMBDA_BINS], const Vector<double, 3> & whitepoint, int iterations = 15) {
                double h = constants::LM_JACOBIAN_STEP;

                Vector<double, 3> res = residual(space, coefficients, color, rgbTable, whitepoint);
                double cost = res.lengthSquared();
                double lambda = constants::LM_INITIAL_DAMPING;

                for (int i = 0; i < iterations && cost > constants::LM_COST_TOLERANCE; i++) {
                    Matrix<double, 3> jacobian;

                    for (int j = 0; j < 3; j++) {
                        Vector<double, 3> perturbed = coefficients;
                        perturbed[j] += h;

                        Vector<double, 3> perturbedRes = residual(space, perturbed, color, rgbTable, whitepoint);

                        for (int k = 0; k < 3; k++) jacobian.get(k, j) = (perturbedRes[k] - res[k]) / h;
                    }

                    Matrix<double, 3> A;
                    Vector<double, 3> g;

                    for (int j = 0; j < 3; j++)
                        for (int k = 0; k < 3; k++) {
                            double sum = 0;

                            for (int l = 0; l < 3; l++) sum += jacobian.get(l, j) * jacobian.get(l, k);

                            A.get(j, k) = sum;

                            g[j] += jacobian.get(k, j) * res[k];
                        }

                    bool accepted = false;

                    for (int j = 0; j < constants::LM_MAX_ATTEMPTS && !accepted; j++) {
                        Matrix<double, 3> M = A;

                        for (int k = 0; k < 3; k++)
                            for (int l = 0; l < 3; l++) M.get(k, l) += (k == l ? lambda : 0);

                        if (std::fmax(std::fabs(M.get(0, 0)) + std::fabs(M.get(1, 0)) + std::fabs(M.get(2, 0)), std::fmax(std::fabs(M.get(0, 1)) + std::fabs(M.get(1, 1)) + std::fabs(M.get(2, 1)), std::fabs(M.get(0, 2)) + std::fabs(M.get(1, 2)) + std::fabs(M.get(2, 2)))) < constants::LM_SINGULAR_NORM) {
                            lambda *= constants::LM_DAMPING_GROWTH;

                            continue;
                        }

                        Vector<double, 3> step = inverse(M) * g;
                        Vector<double, 3> trial = coefficients - step;
                        Vector<double, 3> trialRes = residual(space, trial, color, rgbTable, whitepoint);
                        double trialCost = trialRes.lengthSquared();

                        if (trialCost < cost) {
                            coefficients = trial;
                            cost = trialCost;
                            lambda = std::fmax(lambda * constants::LM_DAMPING_DECAY, constants::LM_MIN_DAMPING);
                            accepted = true;
                        }
                        else {
                            lambda *= constants::LM_DAMPING_GROWTH;

                            if (lambda > constants::LM_MAX_DAMPING) break;
                        }
                    }

                    if (!accepted) break;

                    res = residual(space, coefficients, color, rgbTable, whitepoint);
                }

                return std::sqrt(cost);
            }

            static void solveGrid(ColorSpace space, float * scale, float * lut) {
                Vector<double, 3> whitepoint;
                Matrix<double, 3> toRGB = utils::toRGBMatrix(space);
                double rgbTable[3][constants::CIE_LAMBDA_BINS];

                double rawYIntegral = 0;

                for (int i = 0; i < constants::CIE_LAMBDA_BINS; i++) {
                    int lambda = i + constants::CIE_LAMBDA_MIN;

                    double weight = (i == 0 || i == constants::CIE_LAMBDA_BINS - 1) ? 1.0 / 3.0 : (i % 2 == 1 ? 4.0 / 3.0 : 2.0 / 3.0);

                    rawYIntegral += utils::xyz31(lambda, 1) * utils::illuminant(space, lambda - constants::CIE_ILLUMINANT_MIN) * weight;
                }

                for (int i = 0; i < constants::CIE_LAMBDA_BINS; i++) {
                    int lambda = i + constants::CIE_LAMBDA_MIN;

                    double illumination = utils::illuminant(space, lambda - constants::CIE_ILLUMINANT_MIN) / rawYIntegral;
                    double weight = (i == 0 || i == constants::CIE_LAMBDA_BINS - 1) ? 1.0 / 3.0 : (i % 2 == 1 ? 4.0 / 3.0 : 2.0 / 3.0);

                    for (int j = 0; j < 3; j++) {
                        double accumulated = 0;

                        for (int k = 0; k < 3; k++) accumulated += toRGB.get(j, k) * utils::xyz31(lambda, k) * illumination * weight;

                        rgbTable[j][i] = accumulated;
                    }

                    for (int j = 0; j < 3; j++) whitepoint[j] += utils::xyz31(lambda, j) * illumination * weight;
                }

                for (int j = 0; j < constants::UPSAMPLING_RESOLUTION; j++) scale[j] = float(utils::smoothStep(utils::smoothStep(double(j) / (constants::UPSAMPLING_RESOLUTION - 1))));

                int nTasks = 3 * constants::UPSAMPLING_RESOLUTION;

                #pragma omp parallel for schedule(dynamic)
                for (int task = 0; task < nTasks; task++) {
                    int channel = task / constants::UPSAMPLING_RESOLUTION;
                    int index = task % constants::UPSAMPLING_RESOLUTION;

                    double y = double(index) / (constants::UPSAMPLING_RESOLUTION - 1);

                    for (int j = 0; j < constants::UPSAMPLING_RESOLUTION; j++) {
                        double x = double(j) / (constants::UPSAMPLING_RESOLUTION - 1);

                        Vector<double, 3> coefficients, color;

                        int start = constants::UPSAMPLING_RESOLUTION / 5;

                        for (int k = start; k < constants::UPSAMPLING_RESOLUTION; k++) {
                            double b = scale[k];

                            color[channel] = b;
                            color[(channel + 1) % 3] = x * b;
                            color[(channel + 2) % 3] = y * b;

                            solveLM(space, color, coefficients, rgbTable, whitepoint);

                            double c0 = constants::CIE_LAMBDA_MIN;
                            double c1 = 1.0 / (constants::CIE_LAMBDA_MAX - constants::CIE_LAMBDA_MIN);

                            int offset = ((channel * constants::UPSAMPLING_RESOLUTION + k) * constants::UPSAMPLING_RESOLUTION + index) * constants::UPSAMPLING_RESOLUTION + j;

                            lut[offset * 3 + 0] = float(coefficients[0] * c1 * c1);
                            lut[offset * 3 + 1] = float(coefficients[1] * c1 - 2 * coefficients[0] * c0 * c1 * c1);
                            lut[offset * 3 + 2] = float(coefficients[2] - coefficients[1] * c0 * c1 + coefficients[0] * c0 * c0 * c1 * c1);
                        }

                        coefficients = Vector<double, 3>();

                        for (int k = start - 1; k >= 0; k--) {
                            double b = scale[k];

                            color[channel] = b;
                            color[(channel + 1) % 3] = x * b;
                            color[(channel + 2) % 3] = y * b;

                            solveLM(space, color, coefficients, rgbTable, whitepoint);

                            double c0 = constants::CIE_LAMBDA_MIN;
                            double c1 = 1.0 / (constants::CIE_LAMBDA_MAX - constants::CIE_LAMBDA_MIN);

                            int offset = ((channel * constants::UPSAMPLING_RESOLUTION + k) * constants::UPSAMPLING_RESOLUTION + index) * constants::UPSAMPLING_RESOLUTION + j;

                            lut[offset * 3 + 0] = float(coefficients[0] * c1 * c1);
                            lut[offset * 3 + 1] = float(coefficients[1] * c1 - 2 * coefficients[0] * c0 * c1 * c1);
                            lut[offset * 3 + 2] = float(coefficients[2] - coefficients[1] * c0 * c1 + coefficients[0] * c0 * c0 * c1 * c1);
                        }
                    }
                }
            }
    };
}