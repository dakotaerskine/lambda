#pragma once

#include <cmath>

#include "core/color.h"
#include "core/constants.h"
#include "core/platform.h"
#include "core/utils.h"
#include "math/vector.h"

namespace lambda {
    class Solver {
        public:
            Solver() = delete;

            static void solveGrid(const ColorSpace * space, float * upsamplingScale, float (* upsamplingLUT)[constants::UPSAMPLING_RESOLUTION][constants::UPSAMPLING_RESOLUTION][constants::UPSAMPLING_RESOLUTION][3]) {
                Vector<double, 3> whitepoint;
                Vector<double, 3> response[constants::CIE_LAMBDA_BINS];

                double rawYIntegral = 0;

                for (int i = 0; i < constants::CIE_LAMBDA_BINS; i++) {
                    int lambda = i + constants::CIE_LAMBDA_MIN;

                    double weight = (i == 0 || i == constants::CIE_LAMBDA_BINS - 1) ? 1.0 / 3.0 : (i % 2 == 1 ? 4.0 / 3.0 : 2.0 / 3.0);

                    rawYIntegral += constants::CIE_XYZ_1931[i][1] * space->illuminant(lambda) * weight;
                }

                for (int i = 0; i < constants::CIE_LAMBDA_BINS; i++) {
                    int lambda = i + constants::CIE_LAMBDA_MIN;

                    double illumination = space->illuminant(lambda) / rawYIntegral;
                    double weight = (i == 0 || i == constants::CIE_LAMBDA_BINS - 1) ? 1.0 / 3.0 : (i % 2 == 1 ? 4.0 / 3.0 : 2.0 / 3.0);

                    response[i] = space->toRGB(Vector<double, 3>(constants::CIE_XYZ_1931[i])) * illumination * weight;

                    for (int j = 0; j < 3; j++) whitepoint[j] += constants::CIE_XYZ_1931[i][j] * illumination * weight;
                }

                for (int j = 0; j < constants::UPSAMPLING_RESOLUTION; j++) upsamplingScale[j] = float(utils::smoothStep(utils::smoothStep(double(j) / (constants::UPSAMPLING_RESOLUTION - 1))));

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
                            double b = upsamplingScale[k];

                            color[channel] = b;
                            color[(channel + 1) % 3] = x * b;
                            color[(channel + 2) % 3] = y * b;

                            solveLM(space, color, whitepoint, coefficients, response);

                            double c0 = constants::CIE_LAMBDA_MIN;
                            double c1 = 1.0 / (constants::CIE_LAMBDA_MAX - constants::CIE_LAMBDA_MIN);

                            upsamplingLUT[channel][k][index][j][0] = float(coefficients[0] * c1 * c1);
                            upsamplingLUT[channel][k][index][j][1] = float(coefficients[1] * c1 - 2 * coefficients[0] * c0 * c1 * c1);
                            upsamplingLUT[channel][k][index][j][2] = float(coefficients[2] - coefficients[1] * c0 * c1 + coefficients[0] * c0 * c0 * c1 * c1);
                        }

                        coefficients = Vector<double, 3>();

                        for (int k = start - 1; k >= 0; k--) {
                            double b = upsamplingScale[k];

                            color[channel] = b;
                            color[(channel + 1) % 3] = x * b;
                            color[(channel + 2) % 3] = y * b;

                            solveLM(space, color, whitepoint, coefficients, response);

                            double c0 = constants::CIE_LAMBDA_MIN;
                            double c1 = 1.0 / (constants::CIE_LAMBDA_MAX - constants::CIE_LAMBDA_MIN);

                            upsamplingLUT[channel][k][index][j][0] = float(coefficients[0] * c1 * c1);
                            upsamplingLUT[channel][k][index][j][1] = float(coefficients[1] * c1 - 2 * coefficients[0] * c0 * c1 * c1);
                            upsamplingLUT[channel][k][index][j][2] = float(coefficients[2] - coefficients[1] * c0 * c1 + coefficients[0] * c0 * c0 * c1 * c1);
                        }
                    }
                }
            }

        private:
            static Vector<double, 3> residual(const ColorSpace * space, const Vector<double, 3> & color, const Vector<double, 3> & whitepoint, const Vector<double, 3> & coefficients, const Vector<double, 3> response[constants::CIE_LAMBDA_BINS]) {
                Vector<double, 3> out;

                double inverseRange = 1.0 / (constants::CIE_LAMBDA_MAX - constants::CIE_LAMBDA_MIN);

                for (int i = 0; i < constants::CIE_LAMBDA_BINS; i++) {
                    double lambda = i * inverseRange;
                    double x = (coefficients[0] * lambda + coefficients[1]) * lambda + coefficients[2];
                    double s = 0.5 + 0.5 * x / std::sqrt(1 + x * x);

                    out += response[i] * s;
                }

                out = space->toLab(out, whitepoint);

                Vector<double, 3> res;

                for (int i = 0; i < 3; i++) res[i] = color[i];

                res = space->toLab(res, whitepoint);

                for (int i = 0; i < 3; i++) res[i] -= out[i];

                return res;
            }

            static double solveLM(const ColorSpace * space, const Vector<double, 3> & color, const Vector<double, 3> & whitepoint, Vector<double, 3> & coefficients, const Vector<double, 3> response[constants::CIE_LAMBDA_BINS]) {
                double h = constants::LM_JACOBIAN_STEP;

                Vector<double, 3> res = residual(space, color, whitepoint, coefficients, response);

                double cost = res.lengthSquared();
                double lambda = constants::LM_INITIAL_DAMPING;

                for (int i = 0; i < constants::LM_ITERATIONS && cost > constants::LM_COST_TOLERANCE; i++) {
                    Matrix<double, 3> jacobian;

                    for (int j = 0; j < 3; j++) {
                        Vector<double, 3> perturbed = coefficients;

                        perturbed[j] += h;

                        Vector<double, 3> perturbedRes = residual(space, color, whitepoint, perturbed, response);

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
                        Vector<double, 3> trialRes = residual(space, color, whitepoint, trial, response);

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

                    res = residual(space, color, whitepoint, coefficients, response);
                }

                return std::sqrt(cost);
            }
    };
}