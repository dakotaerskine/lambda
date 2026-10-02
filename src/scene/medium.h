#pragma once

#include "core/constants.h"
#include "core/platform.h"
#include "core/utils.h"
#include "math/intersection.h"
#include "math/ray.h"
#include "math/spectrum.h"

namespace lambda {
    class Medium {
        public:
            Medium() : type(Type::HOMOGENEOUS) {}

            static Medium makeHomogeneous(DenseSpectrum<Float> * sigmaA, DenseSpectrum<Float> * sigmaS, Float g) {
                Medium medium;

                medium.type = Type::HOMOGENEOUS;
                medium.homogeneous.sigmaA = sigmaA;
                medium.homogeneous.sigmaS = sigmaS;
                medium.homogeneous.g = g;

                return medium;
            }

            LAMBDA_HOST_DEVICE SampledSpectrum transmittance(const Ray & r, const Intersection & intersection) const {
                switch (type) {
                    case Type::HOMOGENEOUS: return transmittanceHomogeneous(r, intersection);
                }

                return SampledSpectrum(0);
            }

            LAMBDA_HOST_DEVICE SampledSpectrum intersect(const Ray & r, Intersection & intersection, Random & state) const {
                switch (type) {
                    case Type::HOMOGENEOUS: return intersectHomogeneous(r, intersection, state);
                }

                return SampledSpectrum(0);
            }

            LAMBDA_HOST_DEVICE SampledSpectrum pdf(const Intersection &, const Ray & rIn, const Ray & rOut) const {
                switch (type) {
                    case Type::HOMOGENEOUS: return pdfHomogeneous(rIn, rOut);
                }

                return SampledSpectrum(0);
            }

            LAMBDA_HOST_DEVICE SampledSpectrum evaluate(const Intersection &, const Ray & rIn, const Ray & rOut) const {
                switch (type) {
                    case Type::HOMOGENEOUS: return evaluateHomogeneous(rIn, rOut);
                }

                return SampledSpectrum(0);
            }

            LAMBDA_HOST_DEVICE SampledSpectrum scatter(const Intersection & i, Ray & r, Random & state) const {
                switch (type) {
                    case Type::HOMOGENEOUS: return scatterHomogeneous(i, r, state);
                }

                return SampledSpectrum(0);
            }

        private:
            enum class Type { HOMOGENEOUS };

            Type type;

            union {
                struct { DenseSpectrum<Float> * sigmaA, * sigmaS; Float g; } homogeneous;
            };

            LAMBDA_HOST_DEVICE SampledSpectrum transmittanceHomogeneous(const Ray & r, const Intersection & intersection) const {
                SampledSpectrum attenuation;

                for (int j = 0; j < constants::HERO_COUNT; j++) {
                    Float sigmaA = homogeneous.sigmaA->get(r.getLambdas()[j]);
                    Float sigmaS = homogeneous.sigmaS->get(r.getLambdas()[j]);
                    Float sigmaT = sigmaA + sigmaS;

                    if (intersection.t >= constants::MAX) attenuation[j] = sigmaT > constants::EPSILON ? 0 : 1;
                    else attenuation[j] = std::exp(-sigmaT * intersection.t);
                }

                return attenuation;
            }

            LAMBDA_HOST_DEVICE SampledSpectrum intersectHomogeneous(const Ray & r, Intersection & intersection, Random & state) const {
                Float sigmaA = homogeneous.sigmaA->get(r.getLambdas()[0]);
                Float sigmaS = homogeneous.sigmaS->get(r.getLambdas()[0]);
                Float sigmaT = sigmaA + sigmaS;

                Float t = sigmaT > constants::EPSILON ? -std::log(utils::randomFloat(state)) / sigmaT : constants::MAX;

                bool isSurface = t >= intersection.t;

                if (isSurface) t = intersection.t;

                SampledSpectrum transmittance;
                SampledSpectrum scatterProbability;

                for (int j = 0; j < constants::HERO_COUNT; j++) {
                    sigmaA = homogeneous.sigmaA->get(r.getLambdas()[j]);
                    sigmaS = homogeneous.sigmaS->get(r.getLambdas()[j]);
                    sigmaT = sigmaA + sigmaS;

                    transmittance[j] = std::exp(-sigmaT * t);

                    scatterProbability[j] = isSurface ? transmittance[j] : (sigmaT * transmittance[j]);
                }

                if (!isSurface) {
                    intersection.t = t;
                    intersection.transformedPoint = r.at(t);
                    intersection.isSurface = false;
                }

                Float averageProbability = scatterProbability.average();

                if (averageProbability < constants::EPSILON) return SampledSpectrum(0);

                return transmittance / averageProbability;
            }

            LAMBDA_HOST_DEVICE SampledSpectrum pdfHomogeneous(const Ray & rIn, const Ray & rOut) const {
                Float g2 = homogeneous.g * homogeneous.g;

                return SampledSpectrum(1 / (4 * constants::PI) * (1 - g2) / std::pow(1 + g2 + 2 * homogeneous.g * dot(-rIn.getDirection(), rOut.getDirection()), Float(1.5)));
            }

            LAMBDA_HOST_DEVICE SampledSpectrum evaluateHomogeneous(const Ray & rIn, const Ray & rOut) const {
                SampledSpectrum attenuation;

                for (int j = 0; j < constants::HERO_COUNT; j++) attenuation[j] = homogeneous.sigmaS->get(rIn.getLambdas()[j]);

                return attenuation * pdfHomogeneous(rIn, rOut);
            }

            LAMBDA_HOST_DEVICE SampledSpectrum scatterHomogeneous(const Intersection & i, Ray & r, Random & state) const {
                Float g2 = homogeneous.g * homogeneous.g;

                Float cosTheta = 0;

                if (std::fabs(homogeneous.g) < constants::EPSILON) cosTheta = 1 - 2 * utils::randomFloat(state);
                else {
                    Float term = (1 - g2) / (1 - homogeneous.g + 2 * homogeneous.g * utils::randomFloat(state));

                    cosTheta = -(1 + g2 - term * term) / (2 * homogeneous.g);
                }

                Float sinTheta = std::sqrt(std::fmax(Float(0), 1 - cosTheta * cosTheta));

                Float phi = 2 * constants::PI * utils::randomFloat(state);

                Vector<Float, 3> vIn = -r.getDirection();

                Vector<Float, 3> tangent = (std::fabs(vIn[0]) > std::fabs(vIn[1])) ? normalize(Vector<Float, 3>(-vIn[2], 0, vIn[0])) : normalize(Vector<Float, 3>(0, vIn[2], -vIn[1]));
                Vector<Float, 3> bitangent = cross(vIn, tangent);

                Vector<Float, 3> direction = tangent * (sinTheta * std::cos(phi)) + bitangent * (sinTheta * std::sin(phi)) - r.getDirection() * (cosTheta);

                Ray rOut(i.transformedPoint, direction, r.getLambdas(), r.getMedium());

                SampledSpectrum attenuation;

                for (int j = 0; j < constants::HERO_COUNT; j++) attenuation[j] = homogeneous.sigmaS->get(r.getLambdas()[j]);

                attenuation *= pdfHomogeneous(r, rOut);

                r = rOut;

                return attenuation;
            }
    };
}