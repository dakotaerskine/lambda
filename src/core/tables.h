#pragma once

#include "core/buffer.h"
#include "core/constants.h"
#include "core/parser.h"
#include "core/platform.h"
#include "core/utils.h"
#include "core/writer.h"

class Tables {
    public:
        Tables() : d_SRGB_TO_XYZ(toXYZMatrix(CHROMATICITIES_SRGB).data(), 3 * 3), d_XYZ_TO_SRGB(inverse(toXYZMatrix(CHROMATICITIES_SRGB)).data(), 3 * 3), d_REC2020_TO_XYZ(toXYZMatrix(CHROMATICITIES_REC2020).data(), 3 * 3), d_XYZ_TO_REC2020(inverse(toXYZMatrix(CHROMATICITIES_REC2020)).data(), 3 * 3), d_ACES2065_TO_XYZ(toXYZMatrix(CHROMATICITIES_ACES2065).data(), 3 * 3), d_XYZ_TO_ACES2065(inverse(toXYZMatrix(CHROMATICITIES_ACES2065)).data(), 3 * 3), d_BRADFORD_INVERSE(inverse(Matrix<double, 3>(BRADFORD)).data(), 3 * 3), d_PERMUTATION(&h_PERMUTATION[0], PERMUTATION_SIZE), d_CIE_XYZ_1931(&h_CIE_XYZ_1931[0][0], CIE_LAMBDA_BINS * 3) {
            SRGB_TO_XYZ = d_SRGB_TO_XYZ.data();
            XYZ_TO_SRGB = d_XYZ_TO_SRGB.data();
            REC2020_TO_XYZ = d_REC2020_TO_XYZ.data();
            XYZ_TO_REC2020 = d_XYZ_TO_REC2020.data();
            ACES2065_TO_XYZ = d_ACES2065_TO_XYZ.data();
            XYZ_TO_ACES2065 = d_XYZ_TO_ACES2065.data();
            BRADFORD_INVERSE = d_BRADFORD_INVERSE.data();
            PERMUTATION = d_PERMUTATION.data();
            CIE_XYZ_1931 = d_CIE_XYZ_1931.data();

            loadUpsamplingTables("data/srgb.coeff", ColorSpace::SRGB, d_UPSAMPLING_SCALE_SRGB, d_UPSAMPLING_LUT_SRGB);
            loadUpsamplingTables("data/rec2020.coeff", ColorSpace::REC2020, d_UPSAMPLING_SCALE_REC2020, d_UPSAMPLING_LUT_REC2020);
            loadUpsamplingTables("data/aces2065-1.coeff", ColorSpace::ACES2065, d_UPSAMPLING_SCALE_ACES2065, d_UPSAMPLING_LUT_ACES2065);

            UPSAMPLING_SCALE_SRGB = d_UPSAMPLING_SCALE_SRGB.data();
            UPSAMPLING_LUT_SRGB = d_UPSAMPLING_LUT_SRGB.data();
            UPSAMPLING_SCALE_REC2020 = d_UPSAMPLING_SCALE_REC2020.data();
            UPSAMPLING_LUT_REC2020 = d_UPSAMPLING_LUT_REC2020.data();
            UPSAMPLING_SCALE_ACES2065 = d_UPSAMPLING_SCALE_ACES2065.data();
            UPSAMPLING_LUT_ACES2065 = d_UPSAMPLING_LUT_ACES2065.data();
        }

    private:
        Buffer<double> d_SRGB_TO_XYZ, d_XYZ_TO_SRGB, d_REC2020_TO_XYZ, d_XYZ_TO_REC2020, d_ACES2065_TO_XYZ, d_XYZ_TO_ACES2065, d_BRADFORD_INVERSE;
        Buffer<int> d_PERMUTATION;
        Buffer<double> d_CIE_XYZ_1931;
        Buffer<float> d_UPSAMPLING_SCALE_SRGB, d_UPSAMPLING_LUT_SRGB, d_UPSAMPLING_SCALE_REC2020, d_UPSAMPLING_LUT_REC2020, d_UPSAMPLING_SCALE_ACES2065, d_UPSAMPLING_LUT_ACES2065;

        void loadUpsamplingTables(const std::string & file, ColorSpace space, Buffer<float> & d_scale, Buffer<float> & d_lut) {
            std::vector<float> h_scale(UPSAMPLING_RESOLUTION);
            std::vector<float> h_lut(3 * UPSAMPLING_RESOLUTION * UPSAMPLING_RESOLUTION * UPSAMPLING_RESOLUTION * 3);

            if (!Parser::parseUpsamplingTables(file, h_scale, h_lut)) {
                solveGrid(space, h_scale.data(), h_lut.data());

                Writer::writeUpsamplingTables(file, h_scale, h_lut);
            }

            d_scale = Buffer<float>(h_scale);
            d_lut = Buffer<float>(h_lut);
        }

        Vector<double> residual(ColorSpace space, const Vector<double> & coefficients, const Vector<double> & color, const double rgbTable[3][CIE_LAMBDA_BINS], const Vector<double> & whitepoint) {
            Vector<double> out(0, 0, 0);

            double inverseRange = 1.0 / (CIE_LAMBDA_MAX - CIE_LAMBDA_MIN);

            for (int i = 0; i < CIE_LAMBDA_BINS; i++) {
                double lambda = i * inverseRange;
                double x = (coefficients[0] * lambda + coefficients[1]) * lambda + coefficients[2];
                double s = 0.5 + 0.5 * x / std::sqrt(1 + x * x);

                for (int j = 0; j < 3; j++)
                    out[j] += rgbTable[j][i] * s;
            }

            out = cieLab(space, out, whitepoint);

            Vector<double> res;

            for (int i = 0; i < 3; i++)
                res[i] = color[i];

            res = cieLab(space, res, whitepoint);

            for (int i = 0; i < 3; i++)
                res[i] -= out[i];

            return res;
        }

        double solveLM(ColorSpace space, const Vector<double> & color, Vector<double> & coefficients, const double rgbTable[3][CIE_LAMBDA_BINS], const Vector<double> & whitepoint, int iterations = 15) {
            double h = LM_EPSILON;

            Vector<double> res = residual(space, coefficients, color, rgbTable, whitepoint);
            double cost = res.lengthSquared();
            double lambda = LM_EPSILON;

            for (int i = 0; i < iterations && cost > LM_EPSILON_SQUARED; i++) {
                Matrix<double, 3> jacobian;

                for (int j = 0; j < 3; j++) {
                    Vector<double> perturbed = coefficients;
                    perturbed[j] += h;

                    Vector<double> perturbedRes = residual(space, perturbed, color, rgbTable, whitepoint);

                    for (int k = 0; k < 3; k++)
                        jacobian.get(k, j) = (perturbedRes[k] - res[k]) / h;
                }

                Matrix<double, 3> A;
                Vector<double> g;

                for (int j = 0; j < 3; j++)
                    for (int k = 0; k < 3; k++) {
                        double sum = 0;

                        for (int l = 0; l < 3; l++)
                            sum += jacobian.get(l, j) * jacobian.get(l, k);

                        A.get(j, k) = sum;

                        g[j] += jacobian.get(k, j) * res[k];
                    }

                bool accepted = false;

                for (int j = 0; j < 12 && !accepted; j++) {
                    Matrix<double, 3> M = A;

                    for (int k = 0; k < 3; k++)
                        for (int l = 0; l < 3; l++)
                            M.get(k, l) += (k == l ? lambda : 0);

                    double det = M.determinant();

                    if (std::fabs(det) < LM_EPSILON) {
                        lambda *= 10;
                        continue;
                    }

                    Vector<double> step = transform(inverse(M), g);
                    Vector<double> trial = coefficients - step;
                    Vector<double> trialRes = residual(space, trial, color, rgbTable, whitepoint);
                    double trialCost = trialRes.lengthSquared();

                    if (trialCost < cost) {
                        coefficients = trial;
                        cost = trialCost;
                        lambda = std::fmax(lambda * 0.5, LM_EPSILON_SQUARED);
                        accepted = true;
                    }
                    else {
                        lambda *= 10.0;
                        if (lambda > 1e12) break;
                    }
                }

                if (!accepted) break;

                res = residual(space, coefficients, color, rgbTable, whitepoint);
            }

            return std::sqrt(cost);
        }

        void solveGrid(ColorSpace space, float * scale, float * lut) {
            Vector<double> whitepoint;
            Matrix<double, 3> toRGB = toRGBMatrix(space);
            double rgbTable[3][CIE_LAMBDA_BINS];

            double rawYIntegral = 0;

            for (int i = 0; i < CIE_LAMBDA_BINS; i++) {
                int lambda = i + CIE_LAMBDA_MIN;

                double weight = (i == 0 || i == CIE_LAMBDA_BINS - 1) ? 1.0 / 3.0 : (i % 2 == 1 ? 4.0 / 3.0 : 2.0 / 3.0);

                rawYIntegral += xyz31(lambda, 1) * illuminant(space, lambda - CIE_ILLUMINANT_MIN) * weight;
            }

            for (int i = 0; i < CIE_LAMBDA_BINS; i++) {
                int lambda = i + CIE_LAMBDA_MIN;

                double illumination = illuminant(space, lambda - CIE_ILLUMINANT_MIN) / rawYIntegral;
                double weight = (i == 0 || i == CIE_LAMBDA_BINS - 1) ? 1.0 / 3.0 : (i % 2 == 1 ? 4.0 / 3.0 : 2.0 / 3.0);

                for (int j = 0; j < 3; j++) {
                    double accumulated = 0;

                    for (int k = 0; k < 3; k++)
                        accumulated += toRGB.get(j, k) * xyz31(lambda, k) * illumination * weight;

                    rgbTable[j][i] = accumulated;
                }

                for (int j = 0; j < 3; j++)
                    whitepoint[j] += xyz31(lambda, j) * illumination * weight;
            }

            for (int j = 0; j < UPSAMPLING_RESOLUTION; j++)
                scale[j] = float(smoothStep(smoothStep(double(j) / (UPSAMPLING_RESOLUTION - 1))));

            int nTasks = 3 * UPSAMPLING_RESOLUTION;

            #pragma omp parallel for schedule(dynamic)
            for (int task = 0; task < nTasks; task++) {
                int channel = task / UPSAMPLING_RESOLUTION;
                int index = task % UPSAMPLING_RESOLUTION;

                double y = double(index) / (UPSAMPLING_RESOLUTION - 1);

                for (int j = 0; j < UPSAMPLING_RESOLUTION; j++) {
                    double x = double(j) / (UPSAMPLING_RESOLUTION - 1);

                    Vector<double> coefficients, color;

                    int start = UPSAMPLING_RESOLUTION / 5;

                    for (int k = start; k < UPSAMPLING_RESOLUTION; k++) {
                        double b = scale[k];

                        color[channel] = b;
                        color[(channel + 1) % 3] = x * b;
                        color[(channel + 2) % 3] = y * b;

                        solveLM(space, color, coefficients, rgbTable, whitepoint);

                        double c0 = CIE_LAMBDA_MIN;
                        double c1 = 1.0 / (CIE_LAMBDA_MAX - CIE_LAMBDA_MIN);

                        size_t offset = ((size_t(channel) * UPSAMPLING_RESOLUTION + k) * UPSAMPLING_RESOLUTION + index) * UPSAMPLING_RESOLUTION + j;

                        lut[offset * 3 + 0] = float(coefficients[0] * c1 * c1);
                        lut[offset * 3 + 1] = float(coefficients[1] * c1 - 2 * coefficients[0] * c0 * c1 * c1);
                        lut[offset * 3 + 2] = float(coefficients[2] - coefficients[1] * c0 * c1 + coefficients[0] * c0 * c0 * c1 * c1);
                    }

                    coefficients = Vector<double>();

                    for (int k = start; k >= 0; k--) {
                        double b = scale[k];

                        color[channel] = b;
                        color[(channel + 1) % 3] = x * b;
                        color[(channel + 2) % 3] = y * b;

                        solveLM(space, color, coefficients, rgbTable, whitepoint);

                        double c0 = CIE_LAMBDA_MIN;
                        double c1 = 1.0 / (CIE_LAMBDA_MAX - CIE_LAMBDA_MIN);

                        size_t offset = ((size_t(channel) * UPSAMPLING_RESOLUTION + k) * UPSAMPLING_RESOLUTION + index) * UPSAMPLING_RESOLUTION + j;

                        lut[offset * 3 + 0] = float(coefficients[0] * c1 * c1);
                        lut[offset * 3 + 1] = float(coefficients[1] * c1 - 2 * coefficients[0] * c0 * c1 * c1);
                        lut[offset * 3 + 2] = float(coefficients[2] - coefficients[1] * c0 * c1 + coefficients[0] * c0 * c0 * c1 * c1);
                    }
                }
            }
        }
};