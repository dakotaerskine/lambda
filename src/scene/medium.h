#pragma once

#include "core/constants.h"
#include "core/platform.h"
#include "core/utils.h"
#include "math/ray.h"
#include "math/spectrum.h"
#include "scene/intersection.h"

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

            LAMBDA_HOST_DEVICE SampledSpectrum transmittance(const Intersection & intersection, const Ray & r, bool collapsed) const {
                switch (type) {
                    case Type::HOMOGENEOUS: return transmittanceHomogeneous(intersection, r, collapsed);
                }

                return SampledSpectrum(0);
            }

            LAMBDA_HOST_DEVICE SampledSpectrum sampleDistance(Intersection & intersection, const Ray & r, SampledSpectrum & probabilityDensity, bool collapsed, Random & state) const {
                switch (type) {
                    case Type::HOMOGENEOUS: return sampleDistanceHomogeneous(intersection, r, probabilityDensity, collapsed, state);
                }

                return SampledSpectrum(0);
            }

            LAMBDA_HOST_DEVICE SampledSpectrum evaluate(const Ray & rIn, const Ray & rOut, SampledSpectrum & probabilityDensity, bool collapsed) const {
                switch (type) {
                    case Type::HOMOGENEOUS: return evaluateHomogeneous(rIn, rOut, probabilityDensity, collapsed);
                }

                return SampledSpectrum(0);
            }

            LAMBDA_HOST_DEVICE SampledSpectrum sample(const Intersection & intersection, Ray & r, SampledSpectrum & probabilityDensity, bool collapsed, Random & state) const {
                switch (type) {
                    case Type::HOMOGENEOUS: return sampleHomogeneous(intersection, r, probabilityDensity, collapsed, state);
                }

                return SampledSpectrum(0);
            }

        private:
            enum class Type { HOMOGENEOUS };

            Type type;

            union {
                struct { DenseSpectrum<Float> * sigmaA, * sigmaS; Float g; } homogeneous;
            };

            LAMBDA_HOST_DEVICE SampledSpectrum transmittanceHomogeneous(const Intersection & intersection, const Ray & r, bool collapsed) const {
                SampledSpectrum attenuation;

                for (int i = 0; i < constants::HERO_COUNT; i++)
                    if (!collapsed || i == 0) {
                        Float sigmaA = homogeneous.sigmaA->get(r.getLambdas()[i]);
                        Float sigmaS = homogeneous.sigmaS->get(r.getLambdas()[i]);
                        Float sigmaT = sigmaA + sigmaS;

                        if (intersection.t >= constants::MAX) attenuation[i] = sigmaT > constants::EPSILON ? 0 : 1;
                        else attenuation[i] = std::exp(-sigmaT * intersection.t);
                    }

                return attenuation;
            }

            LAMBDA_HOST_DEVICE SampledSpectrum sampleDistanceHomogeneous(Intersection & intersection, const Ray & r, SampledSpectrum & probabilityDensity, bool collapsed, Random & state) const {
                Float sigmaA = homogeneous.sigmaA->get(r.getLambdas()[0]);
                Float sigmaS = homogeneous.sigmaS->get(r.getLambdas()[0]);
                Float sigmaT = sigmaA + sigmaS;

                Float t = sigmaT > constants::EPSILON ? -std::log(utils::randomFloat(state)) / sigmaT : constants::MAX;

                bool isSurface = t >= intersection.t;

                if (isSurface) t = intersection.t;

                if (!isSurface) {
                    intersection = Intersection();

                    intersection.t = t;
                    intersection.transformedPoint = r.at(t);
                    intersection.isSurface = false;
                }

                SampledSpectrum transmittance = transmittanceHomogeneous(intersection, r, collapsed);

                for (int i = 0; i < constants::HERO_COUNT; i++)
                    if (i == 0 || !collapsed) {
                        sigmaA = homogeneous.sigmaA->get(r.getLambdas()[i]);
                        sigmaS = homogeneous.sigmaS->get(r.getLambdas()[i]);
                        sigmaT = sigmaA + sigmaS;

                        probabilityDensity[i] = isSurface ? transmittance[i] : sigmaT * transmittance[i];
                    }

                if (!(probabilityDensity[0] > 0)) return SampledSpectrum(0);

                return transmittance / probabilityDensity[0];
            }

            LAMBDA_HOST_DEVICE SampledSpectrum evaluateHomogeneous(const Ray & rIn, const Ray & rOut, SampledSpectrum & probabilityDensity, bool collapsed) const {
                if (std::fabs(homogeneous.g) <= constants::EPSILON) probabilityDensity = SampledSpectrum(1 / (4 * constants::PI));
                else {
                    Float g2 = homogeneous.g * homogeneous.g;

                    probabilityDensity = SampledSpectrum(1 / (4 * constants::PI) * (1 - g2) / std::pow(1 + g2 + 2 * homogeneous.g * dot(-rIn.getDirection(), rOut.getDirection()), Float(1.5)));
                }

                SampledSpectrum attenuation;

                for (int i = 0; i < constants::HERO_COUNT; i++)
                    if (i == 0 || !collapsed) attenuation[i] = homogeneous.sigmaS->get(rIn.getLambdas()[i]);

                return attenuation * probabilityDensity;
            }

            LAMBDA_HOST_DEVICE SampledSpectrum sampleHomogeneous(const Intersection & intersection, Ray & r, SampledSpectrum & probabilityDensity, bool collapsed, Random & state) const {
                Float cosTheta = 0;

                if (std::fabs(homogeneous.g) <= constants::EPSILON) cosTheta = 1 - 2 * utils::randomFloat(state);
                else {
                    Float g2 = homogeneous.g * homogeneous.g;

                    Float term = (1 - g2) / (1 - homogeneous.g + 2 * homogeneous.g * utils::randomFloat(state));

                    cosTheta = -(1 + g2 - term * term) / (2 * homogeneous.g);
                }

                Float sinTheta = std::sqrt(std::fmax(Float(0), 1 - cosTheta * cosTheta));

                Float phi = 2 * constants::PI * utils::randomFloat(state);

                Vector<Float, 3> vIn = -r.getDirection();

                Vector<Float, 3> tangent = (std::fabs(vIn[0]) > std::fabs(vIn[1])) ? normalize(Vector<Float, 3>(-vIn[2], 0, vIn[0])) : normalize(Vector<Float, 3>(0, vIn[2], -vIn[1]));
                Vector<Float, 3> bitangent = cross(vIn, tangent);

                Vector<Float, 3> direction = tangent * (sinTheta * std::cos(phi)) + bitangent * (sinTheta * std::sin(phi)) - r.getDirection() * (cosTheta);

                Ray rOut(intersection.transformedPoint, direction, r.getLambdas(), r.getMedium());

                SampledSpectrum attenuation = evaluateHomogeneous(r, rOut, probabilityDensity, collapsed);

                if (!(probabilityDensity[0] > 0)) attenuation = SampledSpectrum(0);
                else attenuation /= probabilityDensity[0];

                r = rOut;

                return attenuation;
            }
    };
}