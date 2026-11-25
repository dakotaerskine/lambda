#pragma once

#include <cmath>

#include "core/constants.h"
#include "core/platform.h"
#include "core/utils.h"

namespace lambda {
    class Fluorophore {
        public:
            Fluorophore() : scale(0), numFluorophores(0), lambdasExcitation(nullptr), lambdasEmission(nullptr), variancesExcitation(nullptr), variancesEmission(nullptr), covariances(nullptr), weights(nullptr) {}

            Fluorophore(Float _scale, int _numFluorophores, Float * _lambdasExcitation, Float * _lambdasEmission, Float * _variancesExcitation, Float * _variancesEmission, Float * _covariances, Float * _weights) : scale(_scale), numFluorophores(_numFluorophores), lambdasExcitation(_lambdasExcitation), lambdasEmission(_lambdasEmission), variancesExcitation(_variancesExcitation), variancesEmission(_variancesEmission), covariances(_covariances), weights(_weights) {}

            Float getMaxOutShift() const {
                Float low = constants::MAX;
                Float high = -constants::MAX;
                Float step = constants::MAX;
                Float curvature = 0;
                Float tail = 0;

                for (int i = 0; i < numFluorophores; i++) {
                    Float sigma = std::sqrt(variancesExcitation[i]);
                    Float peak = scale * weights[i] / std::sqrt(2 * constants::PI * variancesExcitation[i]);

                    low = std::fmin(low, lambdasExcitation[i] - 8 * sigma);
                    high = std::fmax(high, lambdasExcitation[i] + 8 * sigma);
                    step = std::fmin(step, sigma / 8);
                    curvature += peak / variancesExcitation[i];
                    tail += peak * std::exp(Float(-32));
                }

                Float maximum = 0;

                for (Float lambda = low; lambda <= high + step; lambda += step) {
                    Float sum = 0;

                    for (int i = 0; i < numFluorophores; i++) {
                        Float difference = lambda - lambdasExcitation[i];

                        sum += weights[i] * std::exp(Float(-0.5) * difference * difference / variancesExcitation[i]) / std::sqrt(2 * constants::PI * variancesExcitation[i]);
                    }

                    maximum = std::fmax(maximum, scale * sum);
                }

                return maximum + curvature * step * step / 8 + tail;
            }

            LAMBDA_DEVICE Float inShift(Float lambdaEmission) const {
                Float total = 0;

                return scale * inShift(lambdaEmission, total);
            }

            LAMBDA_DEVICE Float outShift(Float lambdaExcitation) const {
                Float lambdaEmission = 0;
                Float variance = 0;
                Float weight = 0;

                Float shifted = 0;

                for (int i = 0; i < numFluorophores; i++) {
                    conditionOut(i, lambdaExcitation, lambdaEmission, variance, weight);

                    shifted += weight * Float(0.5) * (std::erfc((lambdaExcitation - lambdaEmission) / std::sqrt(2 * variance)));
                }

                return scale * shifted;
            }

            LAMBDA_DEVICE Float evaluate(Float lambdaExcitation, Float lambdaEmission, Float reflectance) const {
                if (lambdaExcitation == lambdaEmission) return reflectance;

                if (!(lambdaExcitation > 0) || lambdaExcitation > lambdaEmission) return 0;

                return scale * shiftProbabilityDensity(lambdaExcitation, lambdaEmission);
            }

            LAMBDA_DEVICE Float sample(Float lambdaEmission, Float reflectance, Float & probabilityDensity, Random & state) const {
                Float total, shifted = inShift(lambdaEmission, total);

                if (!(scale * shifted > 0) || !(total > 0)) {
                    probabilityDensity = 1;

                    return lambdaEmission;
                }

                probabilityDensity = reflectance / (reflectance + scale * shifted);

                if (utils::randomFloat(state) < probabilityDensity) return lambdaEmission;

                Float meanLambdaExcitation = 0;
                Float variance = 0;
                Float weight = 0;

                Float lambdaExcitation = lambdaEmission;

                for (int i = 0; i < constants::FLUOROPHORE_MAX_SAMPLES; i++) {
                    Float target = utils::randomFloat(state) * total;

                    Float cumulativeDensity = 0;

                    int j;

                    for (j = 0; j < numFluorophores; j++) {
                        conditionIn(j, lambdaEmission, meanLambdaExcitation, variance, weight);

                        cumulativeDensity += weight;

                        if (target < cumulativeDensity) break;
                    }

                    lambdaExcitation = meanLambdaExcitation + std::sqrt(variance) * std::sqrt(-2 * std::log(1 - utils::randomFloat(state))) * std::cos(2 * constants::PI * utils::randomFloat(state));

                    if (lambdaExcitation > 0 && lambdaExcitation < lambdaEmission) {
                        probabilityDensity = (1 - probabilityDensity) * shiftProbabilityDensity(lambdaExcitation, lambdaEmission) / shifted;

                        return lambdaExcitation;
                    }
                }

                probabilityDensity = 0;

                return lambdaEmission;
            }

        private:
            Float scale;
            int numFluorophores;
            Float * lambdasExcitation, * lambdasEmission, * variancesExcitation, * variancesEmission, * covariances, * weights;

            LAMBDA_DEVICE Float inShift(Float lambdaEmission, Float & total) const {
                Float lambdaExcitation = 0;
                Float variance = 0;
                Float weight = 0;

                total = 0;

                Float shifted = 0;

                for (int i = 0; i < numFluorophores; i++) {
                    conditionIn(i, lambdaEmission, lambdaExcitation, variance, weight);

                    total += weight;
                    shifted += weight * Float(0.5) * (std::erfc((lambdaExcitation - lambdaEmission) / std::sqrt(2 * variance)));
                }

                return shifted;
            }

            LAMBDA_DEVICE Float shiftProbabilityDensity(Float lambdaExcitation, Float lambdaEmission) const {
                Float meanLambdaExcitation = 0;
                Float variance = 0;
                Float weight = 0;

                Float probabilityDensity = 0;

                for (int i = 0; i < numFluorophores; i++) {
                    conditionIn(i, lambdaEmission, meanLambdaExcitation, variance, weight);

                    Float difference = lambdaExcitation - meanLambdaExcitation;

                    probabilityDensity += weight * std::exp(Float(-0.5) * difference * difference / variance) / std::sqrt(2 * constants::PI * variance);
                }

                return probabilityDensity;
            }

            LAMBDA_DEVICE void conditionIn(int i, Float lambdaEmission, Float & lambdaExcitation, Float & variance, Float & weight) const {
                Float slope = covariances[i] / variancesEmission[i];
                Float difference = lambdaEmission - lambdasEmission[i];

                lambdaExcitation = lambdasExcitation[i] + slope * difference;
                variance = variancesExcitation[i] - slope * covariances[i];
                weight = weights[i] * std::exp(Float(-0.5) * difference * difference / variancesEmission[i]) / std::sqrt(2 * constants::PI * variancesEmission[i]);
            }

            LAMBDA_DEVICE void conditionOut(int i, Float lambdaExcitation, Float & lambdaEmission, Float & variance, Float & weight) const {
                Float slope = covariances[i] / variancesExcitation[i];
                Float difference = lambdaExcitation - lambdasExcitation[i];

                lambdaEmission = lambdasEmission[i] + slope * difference;
                variance = variancesEmission[i] - slope * covariances[i];
                weight = weights[i] * std::exp(Float(-0.5) * difference * difference / variancesExcitation[i]) / std::sqrt(2 * constants::PI * variancesExcitation[i]);
            }
    };
}