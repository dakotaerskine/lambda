#pragma once

#include "core/constants.h"
#include "core/platform.h"
#include "core/utils.h"
#include "geometry/intersection.h"
#include "math/ray.h"
#include "math/spectrum.h"
#include "shading/fluorophore.h"

namespace lambda {
    class Medium {
        public:
            Medium() : type(Type::HOMOGENEOUS) {}

            static Medium makeHomogeneous(LinearSpectrum<Float> * sigmaA, LinearSpectrum<Float> * sigmaS, Float g, Fluorophore * f) {
                Medium medium;

                medium.type = Type::HOMOGENEOUS;
                medium.homogeneous.sigmaA = sigmaA;
                medium.homogeneous.sigmaS = sigmaS;
                medium.homogeneous.g = g;
                medium.homogeneous.fluorophore = f;

                return medium;
            }

            LAMBDA_DEVICE SampledSpectrum<Float> sampleLambdas(const SampledSpectrum<Float> & lambdas, SampledSpectrum<Float> & probabilityDensity, bool collapsed, Random & state) const {
                switch (type) {
                    case Type::HOMOGENEOUS: return sampleLambdasHomogeneous(lambdas, probabilityDensity, collapsed, state);
                }

                return lambdas;
            }

            LAMBDA_DEVICE SampledSpectrum<Float> transmittance(const Intersection & intersection, const Ray & r, bool collapsed) const {
                switch (type) {
                    case Type::HOMOGENEOUS: return transmittanceHomogeneous(intersection, r, collapsed);
                }

                return SampledSpectrum<Float>();
            }

            LAMBDA_DEVICE SampledSpectrum<Float> sampleDistance(Intersection & intersection, const Ray & r, SampledSpectrum<Float> & probabilityDensity, bool collapsed, Random & state) const {
                switch (type) {
                    case Type::HOMOGENEOUS: return sampleDistanceHomogeneous(intersection, r, probabilityDensity, collapsed, state);
                }

                return SampledSpectrum<Float>();
            }

            LAMBDA_DEVICE SampledSpectrum<Float> evaluate(const Ray & rIn, const Ray & rOut, SampledSpectrum<Float> & probabilityDensity, bool collapsed) const {
                switch (type) {
                    case Type::HOMOGENEOUS: return evaluateHomogeneous(rIn, rOut, probabilityDensity, collapsed);
                }

                return SampledSpectrum<Float>();
            }

            LAMBDA_DEVICE SampledSpectrum<Float> sample(const Intersection & intersection, Ray & r, SampledSpectrum<Float> & probabilityDensity, bool collapsed, Random & state) const {
                switch (type) {
                    case Type::HOMOGENEOUS: return sampleHomogeneous(intersection, r, probabilityDensity, collapsed, state);
                }

                return SampledSpectrum<Float>();
            }

        private:
            enum class Type { HOMOGENEOUS };

            Type type;

            union {
                struct { LinearSpectrum<Float> * sigmaA, * sigmaS; Float g; Fluorophore * fluorophore; } homogeneous;
            };

            LAMBDA_DEVICE SampledSpectrum<Float> sampleLambdasHomogeneous(const SampledSpectrum<Float> & lambdas, SampledSpectrum<Float> & probabilityDensity, bool collapsed, Random & state) const {
                if (!homogeneous.fluorophore) return lambdas;

                SampledSpectrum<Float> lambdasExcitation = lambdas;

                for (int i = 0; i < (!collapsed ? constants::HERO_COUNT : 1); i++) {
                    Float lambdaEmission = lambdas[i];

                    lambdasExcitation[i] = homogeneous.fluorophore->sample(lambdaEmission, (*homogeneous.sigmaS)[lambdaEmission], probabilityDensity[i], state);
                }

                return lambdasExcitation;
            }

            LAMBDA_DEVICE SampledSpectrum<Float> transmittanceHomogeneous(const Intersection & intersection, const Ray & r, bool collapsed) const {
                SampledSpectrum<Float> attenuation;

                for (int i = 0; i < (!collapsed ? constants::HERO_COUNT : 1); i++) {
                    Float lambda = r.getLambdas()[i];

                    Float sigmaT = (*homogeneous.sigmaA)[lambda] + (*homogeneous.sigmaS)[lambda];

                    if (homogeneous.fluorophore) sigmaT += homogeneous.fluorophore->outShift(lambda);

                    if (intersection.t >= constants::MAX) attenuation[i] = sigmaT > 0 ? 0 : 1;
                    else attenuation[i] = std::exp(-intersection.t * sigmaT);
                }

                return attenuation;
            }

            LAMBDA_DEVICE SampledSpectrum<Float> sampleDistanceHomogeneous(Intersection & intersection, const Ray & r, SampledSpectrum<Float> & probabilityDensity, bool collapsed, Random & state) const {
                SampledSpectrum<Float> sigmaS, sigmaT;

                for (int i = 0; i < (!collapsed ? constants::HERO_COUNT : 1); i++) {
                    Float lambda = r.getLambdas()[i];

                    sigmaS[i] = (*homogeneous.sigmaS)[lambda];
                    sigmaT[i] = (*homogeneous.sigmaA)[lambda] + sigmaS[i];

                    if (homogeneous.fluorophore) {
                        sigmaS[i] += homogeneous.fluorophore->inShift(lambda);
                        sigmaT[i] += homogeneous.fluorophore->outShift(lambda);
                    }
                }

                Float d = intersection.t;
                Float u = utils::randomFloat(state);
                Float t = constants::MAX;

                if (d >= constants::MAX) {
                    if (sigmaT[0] > 0) t = -std::log(1 - u) / sigmaT[0];
                }
                else if (!(sigmaS[0] > 0)) t = d;
                else if (sigmaT[0] > 0) {
                    Float factor = 1 - u * (1 + (sigmaT[0] / sigmaS[0] - 1) * std::exp(-d * sigmaT[0]));

                    t = factor > 0 ? -std::log(factor) / sigmaT[0] : d;
                }
                else t = u * (d * sigmaS[0] + 1) / sigmaS[0];

                bool isSurface = t >= d;

                if (isSurface) t = d;
                else {
                    intersection = Intersection();

                    intersection.t = t;
                    intersection.transformedPoint = r.at(t);
                    intersection.isSurface = false;
                }

                for (int i = 0; i < (!collapsed ? constants::HERO_COUNT : 1); i++) {
                    if (d >= constants::MAX) probabilityDensity[i] = !isSurface ? sigmaT[i] * std::exp(-t * sigmaT[i]) : 1;
                    else {
                        Float weight = sigmaT[i] > 0 ? (sigmaS[i] + (sigmaT[i] - sigmaS[i]) * std::exp(-d * sigmaT[i])) / sigmaT[i] : d * sigmaS[i] + 1;

                        probabilityDensity[i] = weight > 0 ? (!isSurface ? sigmaS[i] * std::exp(-t * sigmaT[i]) : std::exp(-d * sigmaT[i])) / weight : 0;
                    }
                }

                if (!(probabilityDensity[0] > 0)) return SampledSpectrum<Float>();

                return transmittanceHomogeneous(intersection, r, collapsed) / probabilityDensity[0];
            }

            LAMBDA_DEVICE SampledSpectrum<Float> evaluateHomogeneous(const Ray & rIn, const Ray & rOut, SampledSpectrum<Float> & probabilityDensity, bool collapsed) const {
                if (std::fabs(homogeneous.g) <= constants::ISOTROPIC_G) probabilityDensity = SampledSpectrum<Float>(1 / (4 * constants::PI));
                else {
                    Float c = dot(-rIn.getDirection(), rOut.getDirection());

                    Float d = homogeneous.g >= 0 ? (1 - homogeneous.g) * (1 - homogeneous.g) + 2 * homogeneous.g * (1 + c) : (1 + homogeneous.g) * (1 + homogeneous.g) - 2 * homogeneous.g * (1 - c);

                    probabilityDensity = SampledSpectrum<Float>((1 - homogeneous.g * homogeneous.g) / (4 * constants::PI * d * std::sqrt(d)));
                }

                SampledSpectrum<Float> attenuation;

                for (int i = 0; i < (!collapsed ? constants::HERO_COUNT : 1); i++) {
                    Float lambdaExcitation = rOut.getLambdas()[i];
                    Float lambdaEmission = rIn.getLambdas()[i];

                    Float sigmaS = (*homogeneous.sigmaS)[lambdaEmission];

                    attenuation[i] = homogeneous.fluorophore ? homogeneous.fluorophore->evaluate(lambdaExcitation, lambdaEmission, sigmaS) : sigmaS;
                }

                return attenuation * probabilityDensity;
            }

            LAMBDA_DEVICE SampledSpectrum<Float> sampleHomogeneous(const Intersection & intersection, Ray & r, SampledSpectrum<Float> & probabilityDensity, bool collapsed, Random & state) const {
                Float cosTheta = 0;

                if (std::fabs(homogeneous.g) <= constants::ISOTROPIC_G) cosTheta = 1 - 2 * utils::randomFloat(state);
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

                SampledSpectrum<Float> lambdas = r.getLambdas();
                SampledSpectrum<Float> shiftProbabilityDensity(1);

                lambdas = sampleLambdasHomogeneous(lambdas, shiftProbabilityDensity, collapsed, state);

                Ray rOut(intersection.transformedPoint, direction, lambdas, r.getMedium());

                SampledSpectrum<Float> attenuation = evaluateHomogeneous(r, rOut, probabilityDensity, collapsed);

                r = rOut;

                if (!(probabilityDensity[0] > 0)) return SampledSpectrum<Float>();

                for (int i = 0; i < (!collapsed ? constants::HERO_COUNT : 1); i++)
                    if (!(shiftProbabilityDensity[i] > 0)) return SampledSpectrum<Float>();

                return attenuation / (probabilityDensity[0] * shiftProbabilityDensity);
            }
    };
}