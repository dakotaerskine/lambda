#pragma once

#include <cmath>

#include "core/platform.h"
#include "core/utils.h"
#include "math/complex.h"
#include "math/matrix.h"
#include "math/random.h"
#include "math/ray.h"
#include "math/spectrum.h"
#include "math/vector.h"
#include "scene/intersection.h"
#include "scene/texture.h"

namespace lambda {
    class Medium;

    class Material {
        public:
            Material() : type(Type::LAMBERTIAN) {}

            static Material makeLambertian(SpectrumTexture * a) {
                Material material;

                material.type = Type::LAMBERTIAN;
                material.lambertian.albedo = a;

                return material;
            }

            static Material makeMirror(SpectrumTexture * a) {
                Material material;

                material.type = Type::MIRROR;
                material.mirror.albedo = a;

                return material;
            }

            static Material makeConductor(DenseSpectrum<Float> * n0, DenseSpectrum<Complex> * n1, ScalarTexture * alphaX, ScalarTexture * alphaY) {
                Material material;

                material.type = Type::CONDUCTOR;
                material.conductor.n0 = n0;
                material.conductor.n1 = n1;
                material.conductor.alphaX = alphaX;
                material.conductor.alphaY = alphaY;

                return material;
            }

            static Material makeDielectric(DenseSpectrum<Float> * n0, DenseSpectrum<Float> * n1, ScalarTexture * alphaX, ScalarTexture * alphaY) {
                Material material;

                material.type = Type::DIELECTRIC;
                material.dielectric.n0 = n0;
                material.dielectric.n1 = n1;
                material.dielectric.alphaX = alphaX;
                material.dielectric.alphaY = alphaY;

                return material;
            }

            static Material makeCoated(int numLayers, DenseSpectrum<Complex> ** n, ScalarTexture ** d, Material * substrate) {
                Material material;

                material.type = Type::COATED;
                material.coated.numLayers = numLayers;
                material.coated.n = n;
                material.coated.d = d;
                material.coated.substrate = substrate;

                return material;
            }

            static Material makeEmissive(SpectrumTexture * e) {
                Material material;

                material.type = Type::EMISSIVE;
                material.emissive.emission = e;

                return material;
            }

            LAMBDA_HOST_DEVICE bool isSpecular(const Intersection & intersection) const {
                switch (type) {
                    case Type::LAMBERTIAN: return false;
                    case Type::MIRROR: return true;
                    case Type::CONDUCTOR: return conductor.alphaX->evaluate(intersection) <= constants::EPSILON && conductor.alphaY->evaluate(intersection) <= constants::EPSILON;
                    case Type::DIELECTRIC: return dielectric.alphaX->evaluate(intersection) <= constants::EPSILON && dielectric.alphaY->evaluate(intersection) <= constants::EPSILON;
                    case Type::COATED: return isSpecularCoated(intersection);
                    case Type::EMISSIVE: return false;
                }

                return false;
            }

            LAMBDA_HOST_DEVICE bool isEmissive() const {
                switch (type) {
                    case Type::LAMBERTIAN: return false;
                    case Type::MIRROR: return false;
                    case Type::CONDUCTOR: return false;
                    case Type::DIELECTRIC: return false;
                    case Type::COATED: return false;
                    case Type::EMISSIVE: return true;
                }

                return false;
            }

            LAMBDA_HOST_DEVICE bool isIdealFresnel() const {
                switch (type) {
                    case Type::LAMBERTIAN: return false;
                    case Type::MIRROR: return false;
                    case Type::CONDUCTOR: return true;
                    case Type::DIELECTRIC: return true;
                    case Type::COATED: return false;
                    case Type::EMISSIVE: return false;
                }

                return false;
            }

            LAMBDA_HOST_DEVICE SampledSpectrum emission(const Intersection & intersection, const SampledSpectrum & lambdas) const {
                switch (type) {
                    case Type::LAMBERTIAN: return SampledSpectrum(0);
                    case Type::MIRROR: return SampledSpectrum(0);
                    case Type::CONDUCTOR: return SampledSpectrum(0);
                    case Type::DIELECTRIC: return SampledSpectrum(0);
                    case Type::COATED: return SampledSpectrum(0);
                    case Type::EMISSIVE: return emissionEmissive(intersection, lambdas);
                }

                return SampledSpectrum(0);
            }

            LAMBDA_HOST_DEVICE Float averageEmission() const {
                switch (type) {
                    case Type::LAMBERTIAN: return 0;
                    case Type::MIRROR: return 0;
                    case Type::CONDUCTOR: return 0;
                    case Type::DIELECTRIC: return 0;
                    case Type::COATED: return 0;
                    case Type::EMISSIVE: return emissive.emission->getAverage();
                }

                return 0;
            }

            LAMBDA_HOST_DEVICE SampledSpectrum evaluate(const Intersection & intersection, const Ray & rIn, const Ray & rOut, SampledSpectrum & probabilityDensity, bool collapsed) const {
                switch (type) {
                    case Type::LAMBERTIAN: return evaluateLambertian(intersection, rOut, probabilityDensity, collapsed);
                    case Type::MIRROR: return SampledSpectrum(0);
                    case Type::CONDUCTOR: return evaluateConductor(nullptr, intersection, rIn, rOut, probabilityDensity, collapsed);
                    case Type::DIELECTRIC: return evaluateDielectric(nullptr, intersection, rIn, rOut, probabilityDensity, collapsed);
                    case Type::COATED: return evaluateCoated(intersection, rIn, rOut, probabilityDensity, collapsed);
                    case Type::EMISSIVE: return SampledSpectrum(0);
                }

                return SampledSpectrum(0);
            }

            LAMBDA_HOST_DEVICE SampledSpectrum sample(const Intersection & intersection, Ray & r, const Medium * medium1, SampledSpectrum & probabilityDensity, bool & collapsed, Random & state) const {
                switch (type) {
                    case Type::LAMBERTIAN: return sampleLambertian(intersection, r, probabilityDensity, collapsed, state);
                    case Type::MIRROR: return sampleMirror(intersection, r, probabilityDensity, collapsed);
                    case Type::CONDUCTOR: return sampleConductor(nullptr, intersection, r, probabilityDensity, collapsed, state);
                    case Type::DIELECTRIC: return sampleDielectric(nullptr, intersection, r, medium1, probabilityDensity, collapsed, state);
                    case Type::COATED: return sampleCoated(intersection, r, medium1, probabilityDensity, collapsed, state);
                    case Type::EMISSIVE: return SampledSpectrum(0);
                }

                return SampledSpectrum(0);
            }

        private:
            enum class Type { LAMBERTIAN, MIRROR, CONDUCTOR, DIELECTRIC, COATED, EMISSIVE };

            Type type;

            union {
                struct { SpectrumTexture * albedo; } lambertian;
                struct { SpectrumTexture * albedo; } mirror;
                struct { DenseSpectrum<Float> * n0; DenseSpectrum<Complex> * n1; ScalarTexture * alphaX, * alphaY; } conductor;
                struct { DenseSpectrum<Float> * n0, * n1; ScalarTexture * alphaX, * alphaY; } dielectric;
                struct { int numLayers; DenseSpectrum<Complex> ** n; ScalarTexture ** d; Material * substrate; } coated;
                struct { SpectrumTexture * emission; } emissive;
            };

            LAMBDA_HOST_DEVICE bool isSpecularCoated(const Intersection & intersection) const {
                switch (coated.substrate->type) {
                    case Type::CONDUCTOR: return coated.substrate->conductor.alphaX->evaluate(intersection) <= constants::EPSILON && coated.substrate->conductor.alphaY->evaluate(intersection) <= constants::EPSILON;
                    case Type::DIELECTRIC: return coated.substrate->dielectric.alphaX->evaluate(intersection) <= constants::EPSILON && coated.substrate->dielectric.alphaY->evaluate(intersection) <= constants::EPSILON;
                    default: return false;
                }
            }

            LAMBDA_HOST_DEVICE SampledSpectrum emissionEmissive(const Intersection & intersection, const SampledSpectrum & lambdas) const {
                SampledSpectrum emission;

                for (int i = 0; i < constants::HERO_COUNT; i++) emission[i] = emissive.emission->evaluate(intersection, lambdas[i]);

                return emission;
            }

            LAMBDA_HOST_DEVICE SampledSpectrum evaluateLambertian(const Intersection & intersection, const Ray & r, SampledSpectrum & probabilityDensity, bool collapsed) const {
                Float cosTheta = dot(intersection.transformedNormal, r.getDirection());

                probabilityDensity = cosTheta > constants::EPSILON ? SampledSpectrum(cosTheta / constants::PI) : SampledSpectrum(0);

                if (cosTheta <= constants::EPSILON) return SampledSpectrum(0);

                SampledSpectrum attenuation;

                for (int i = 0; i < constants::HERO_COUNT; i++) attenuation[i] = !collapsed || i == 0 ? lambertian.albedo->evaluate(intersection, r.getLambdas()[i]) / constants::PI : 0;

                return attenuation;
            }

            LAMBDA_HOST_DEVICE SampledSpectrum evaluateConductor(const Material * coating, const Intersection & intersection, const Ray & rIn, const Ray & rOut, SampledSpectrum & probabilityDensity, bool collapsed) const {
                Float alphaX = conductor.alphaX->evaluate(intersection);
                Float alphaY = conductor.alphaY->evaluate(intersection);

                if (alphaX <= constants::EPSILON && alphaY <= constants::EPSILON) {
                    probabilityDensity = SampledSpectrum(1);

                    return SampledSpectrum(0);
                }

                Vector<Float, 3> vIn = -rIn.getDirection();
                Vector<Float, 3> vOut = rOut.getDirection();

                probabilityDensity = SampledSpectrum(0);

                if (dot(intersection.transformedNormal, vIn) <= constants::EPSILON || dot(intersection.transformedNormal, vOut) <= constants::EPSILON) return SampledSpectrum(0);

                Vector<Float, 3> bitangent = cross(intersection.transformedNormal, intersection.transformedTangent);

                Vector<Float, 3> vInLocal = Vector<Float, 3>(dot(vIn, intersection.transformedTangent), dot(vIn, bitangent), dot(vIn, intersection.transformedNormal));
                Vector<Float, 3> vOutLocal = Vector<Float, 3>(dot(vOut, intersection.transformedTangent), dot(vOut, bitangent), dot(vOut, intersection.transformedNormal));

                Vector<Float, 3> halfVLocal = vInLocal + vOutLocal;

                Float lengthSquared = halfVLocal.lengthSquared();

                if (lengthSquared <= constants::EPSILON_SQUARED) return SampledSpectrum(0);

                halfVLocal /= std::sqrt(lengthSquared);

                if (halfVLocal[2] <= constants::EPSILON) halfVLocal *= -1;

                Float cosTheta = dot(vInLocal, halfVLocal);

                SampledSpectrum reflectance, transmittance;

                for (int i = 0; i < constants::HERO_COUNT; i++)
                    if (!collapsed || i == 0) {
                        Float lambda = rIn.getLambdas()[i];

                        Float n0 = conductor.n0->get(lambda);
                        Complex n1 = conductor.n1->get(lambda);

                        fresnel(coating, intersection, n0, n1, cosTheta, lambda, reflectance[i], transmittance[i]);
                    }

                Float D = utils::ggxD(halfVLocal, alphaX, alphaY);
                Float G1 = utils::ggxG1(vInLocal, alphaX, alphaY);
                Float G2 = utils::ggxG2(vInLocal, vOutLocal, alphaX, alphaY);

                probabilityDensity = SampledSpectrum(D * G1 / (4 * std::fabs(vInLocal[2])));

                return reflectance * (D * G2 / (4 * vInLocal[2] * vOutLocal[2]));
            }

            LAMBDA_HOST_DEVICE SampledSpectrum evaluateDielectric(const Material * coating, const Intersection & intersection, const Ray & rIn, const Ray & rOut, SampledSpectrum & probabilityDensity, bool collapsed) const {
                Float alphaX = dielectric.alphaX->evaluate(intersection);
                Float alphaY = dielectric.alphaY->evaluate(intersection);

                Vector<Float, 3> vIn = -rIn.getDirection();
                Vector<Float, 3> vOut = rOut.getDirection();

                bool reflect = dot(intersection.transformedNormal, vOut) > constants::EPSILON;

                if (alphaX <= constants::EPSILON && alphaY <= constants::EPSILON) {
                    probabilityDensity = SampledSpectrum(1);

                    return SampledSpectrum(0);
                }

                Vector<Float, 3> bitangent = cross(intersection.transformedNormal, intersection.transformedTangent);

                Vector<Float, 3> vInLocal = Vector<Float, 3>(dot(vIn, intersection.transformedTangent), dot(vIn, bitangent), dot(vIn, intersection.transformedNormal));
                Vector<Float, 3> vOutLocal = Vector<Float, 3>(dot(vOut, intersection.transformedTangent), dot(vOut, bitangent), dot(vOut, intersection.transformedNormal));

                Float G1 = utils::ggxG1(vInLocal, alphaX, alphaY);

                probabilityDensity = SampledSpectrum(0);

                SampledSpectrum reflectance, transmittance, attenuation;

                for (int i = 0; i < constants::HERO_COUNT; i++) {
                    Float lambda = rIn.getLambdas()[i];

                    Float n0 = intersection.frontFacing ? dielectric.n0->get(lambda) : dielectric.n1->get(lambda);
                    Float n1 = intersection.frontFacing ? dielectric.n1->get(lambda) : dielectric.n0->get(lambda);

                    Float ratio = n0 / n1;

                    if (!reflect && std::fabs(ratio - 1) <= constants::EPSILON) probabilityDensity[i] = std::fabs(dot(intersection.transformedNormal, vOut));
                    else {
                        Vector<Float, 3> halfVLocal = reflect ? vInLocal + vOutLocal : -(vInLocal * ratio + vOutLocal);

                        Float lengthSquared = halfVLocal.lengthSquared();

                        if (lengthSquared > constants::EPSILON_SQUARED) {
                            halfVLocal /= std::sqrt(lengthSquared);

                            if (halfVLocal[2] <= constants::EPSILON) halfVLocal *= -1;

                            Float cosTheta = dot(halfVLocal, vInLocal);

                            if (cosTheta > constants::EPSILON && (reflect || dot(halfVLocal, vOutLocal) < 0)) {
                                fresnel(coating, intersection, n0, n1, cosTheta, lambda, reflectance[i], transmittance[i]);

                                Vector<Float, 3> vOutLocalShadow(vOutLocal[0], vOutLocal[1], std::fabs(vOutLocal[2]));

                                Float D = utils::ggxD(halfVLocal, alphaX, alphaY);
                                Float G2 = utils::ggxG2(vInLocal, vOutLocalShadow, alphaX, alphaY);

                                if (reflect) {
                                    probabilityDensity[i] = reflectance[i] * D * G1 / (4 * std::fabs(vInLocal[2]));
                                    attenuation[i] = (!collapsed || i == 0) ? reflectance[i] * (D * G2 / (4 * vInLocal[2] * vOutLocalShadow[2])) : 0;
                                }
                                else {
                                    Float denominator = ratio * dot(vInLocal, halfVLocal) + dot(vOutLocal, halfVLocal);

                                    probabilityDensity[i] = transmittance[i] * D * G1 * cosTheta * std::fabs(dot(vOutLocal, halfVLocal)) / (denominator * denominator * std::fabs(vInLocal[2]));
                                    attenuation[i] = (!collapsed || i == 0) ? transmittance[i] * D * G2 * cosTheta * std::fabs(dot(vOutLocal, halfVLocal)) / (denominator * denominator * std::fabs(vInLocal[2]) * std::fabs(vOutLocal[2])) * ratio * ratio : 0;
                                }
                            }
                        }
                    }
                }

                return attenuation;
            }

            LAMBDA_HOST_DEVICE SampledSpectrum evaluateCoated(const Intersection & intersection, const Ray & rIn, const Ray & rOut, SampledSpectrum & probabilityDensity, bool collapsed) const {
                switch (coated.substrate->type) {
                    case Type::CONDUCTOR: return coated.substrate->evaluateConductor(this, intersection, rIn, rOut, probabilityDensity, collapsed);
                    case Type::DIELECTRIC: return coated.substrate->evaluateDielectric(this, intersection, rIn, rOut, probabilityDensity, collapsed);
                    default: return SampledSpectrum(0);
                }
            }

            LAMBDA_HOST_DEVICE SampledSpectrum sampleLambertian(const Intersection & intersection, Ray & r, SampledSpectrum & probabilityDensity, bool & collapsed, Random & state) const {
                r = Ray(intersection.transformedPoint, utils::randomInHemisphere(intersection.transformedNormal, state), r.getLambdas(), r.getMedium());

                SampledSpectrum attenuation = evaluateLambertian(intersection, r, probabilityDensity, collapsed);

                if (!(probabilityDensity[0] > 0)) return SampledSpectrum(0);

                return attenuation * dot(intersection.transformedNormal, r.getDirection()) / probabilityDensity[0];
            }

            LAMBDA_HOST_DEVICE SampledSpectrum sampleMirror(const Intersection & intersection, Ray & r, SampledSpectrum & probabilityDensity, bool collapsed) const {
                r = Ray(intersection.transformedPoint, utils::reflected(r.getDirection(), intersection.transformedNormal), r.getLambdas(), r.getMedium());

                probabilityDensity = SampledSpectrum(1);

                SampledSpectrum attenuation;

                for (int i = 0; i < constants::HERO_COUNT; i++) attenuation[i] = !collapsed || i == 0 ? mirror.albedo->evaluate(intersection, r.getLambdas()[i]) : 0;

                return attenuation;
            }

            LAMBDA_HOST_DEVICE SampledSpectrum sampleConductor(const Material * coating, const Intersection & intersection, Ray & r, SampledSpectrum & probabilityDensity, bool & collapsed, Random & state) const {
                Vector<Float, 3> normal = intersection.transformedNormal;

                Float alphaX = conductor.alphaX->evaluate(intersection);
                Float alphaY = conductor.alphaY->evaluate(intersection);

                if (alphaX > constants::EPSILON || alphaY > constants::EPSILON) normal = utils::ggxNormal(-r.getDirection(), normal, intersection.transformedTangent, alphaX, alphaY, state);

                Vector<Float, 3> direction = utils::reflected(r.getDirection(), normal);

                if (dot(intersection.transformedNormal, direction) <= constants::EPSILON) return SampledSpectrum(0);

                Ray rOut(intersection.transformedPoint, direction, r.getLambdas(), r.getMedium());

                SampledSpectrum attenuation;

                if ((alphaX > constants::EPSILON || alphaY > constants::EPSILON)) {
                    attenuation = evaluateConductor(coating, intersection, r, rOut, probabilityDensity, collapsed);

                    if (!(probabilityDensity[0] > 0)) attenuation = SampledSpectrum(0);
                    else attenuation *= dot(intersection.transformedNormal, rOut.getDirection()) / probabilityDensity[0];
                }
                else {
                    probabilityDensity = SampledSpectrum(1);

                    Float cosTheta = dot(intersection.transformedNormal, -r.getDirection());

                    SampledSpectrum reflectance, transmittance;

                    for (int i = 0; i < constants::HERO_COUNT; i++)
                        if (!collapsed || i == 0) {
                            Float lambda = r.getLambdas()[i];

                            Float n0 = conductor.n0->get(lambda);
                            Complex n1 = conductor.n1->get(lambda);

                            fresnel(coating, intersection, n0, n1, cosTheta, lambda, reflectance[i], transmittance[i]);

                            attenuation[i] = reflectance[i];
                        }
                }

                r = rOut;

                return attenuation;
            }

            LAMBDA_HOST_DEVICE SampledSpectrum sampleDielectric(const Material * coating, const Intersection & intersection, Ray & r, const Medium * medium1, SampledSpectrum & probabilityDensity, bool & collapsed, Random & state) const {
                Vector<Float, 3> normal = intersection.transformedNormal;

                Float alphaX = dielectric.alphaX->evaluate(intersection);
                Float alphaY = dielectric.alphaY->evaluate(intersection);

                if (alphaX > constants::EPSILON || alphaY > constants::EPSILON) normal = utils::ggxNormal(-r.getDirection(), normal, intersection.transformedTangent, alphaX, alphaY, state);

                Float lambda = r.getLambdas()[0];

                Float n0Hero = intersection.frontFacing ? dielectric.n0->get(lambda) : dielectric.n1->get(lambda);
                Float n1Hero = intersection.frontFacing ? dielectric.n1->get(lambda) : dielectric.n0->get(lambda);

                bool collapse = false;

                Float cosTheta = dot(normal, -r.getDirection());

                SampledSpectrum reflectance, transmittance, ratio;

                for (int i = 0; i < constants::HERO_COUNT; i++) {
                    lambda = r.getLambdas()[i];

                    Float n0 = intersection.frontFacing ? dielectric.n0->get(lambda) : dielectric.n1->get(lambda);
                    Float n1 = intersection.frontFacing ? dielectric.n1->get(lambda) : dielectric.n0->get(lambda);

                    if (!collapsed && (std::fabs(n0Hero - n0) > constants::EPSILON || std::fabs(n1Hero - n1) > constants::EPSILON)) collapse = true;

                    fresnel(coating, intersection, n0, n1, cosTheta, lambda, reflectance[i], transmittance[i]);

                    ratio[i] = n0 / n1;
                }

                if (reflectance[0] + transmittance[0] <= constants::EPSILON) return SampledSpectrum(0);

                Float u = utils::randomFloat(state);

                if (u >= reflectance[0] + transmittance[0]) return SampledSpectrum(0);

                bool reflect = u < reflectance[0] || reflectance[0] >= 1;

                Vector<Float, 3> direction = reflect ? utils::reflected(r.getDirection(), normal) : utils::refracted(r.getDirection(), normal, n0Hero / n1Hero);

                if ((alphaX > constants::EPSILON || alphaY > constants::EPSILON) && (reflect ? dot(intersection.transformedNormal, direction) <= constants::EPSILON : dot(intersection.transformedNormal, direction) >= -constants::EPSILON)) return SampledSpectrum(0);

                Ray rOut(intersection.transformedPoint, direction, r.getLambdas(), reflect ? r.getMedium() : medium1);

                if (!reflect && collapse) collapsed = true;

                SampledSpectrum attenuation;

                if ((alphaX > constants::EPSILON || alphaY > constants::EPSILON) && (reflect || std::fabs(n0Hero - n1Hero) > constants::EPSILON)) {
                    attenuation = evaluateDielectric(coating, intersection, r, rOut, probabilityDensity, collapsed);

                    if (!(probabilityDensity[0] > 0)) attenuation = SampledSpectrum(0);
                    else attenuation *= std::fabs(dot(intersection.transformedNormal, rOut.getDirection())) / probabilityDensity[0];
                }
                else {
                    probabilityDensity = reflect ? reflectance : transmittance;

                    attenuation = reflect ? reflectance / reflectance[0] : transmittance / transmittance[0] * ratio * ratio;

                    if (collapsed)
                        for (int i = 1; i < constants::HERO_COUNT; i++) attenuation[i] = 0;
                }

                r = rOut;

                return attenuation;
            }

            LAMBDA_HOST_DEVICE SampledSpectrum sampleCoated(const Intersection & intersection, Ray & r, const Medium * medium1, SampledSpectrum & probabilityDensity, bool & collapsed, Random & state) const {
                switch (coated.substrate->type) {
                    case Type::CONDUCTOR: return coated.substrate->sampleConductor(this, intersection, r, probabilityDensity, collapsed, state);
                    case Type::DIELECTRIC: return coated.substrate->sampleDielectric(this, intersection, r, medium1, probabilityDensity, collapsed, state);
                    default: return SampledSpectrum(0);
                }
            }

            template <typename T>
            LAMBDA_HOST_DEVICE static void fresnel(const Material * coating, const Intersection & intersection, Float nI, const T & nT, Float cosThetaI, Float lambda, Float & reflectance, Float & transmittance) {
                using std::abs;
                using std::fabs;
                using std::sqrt;

                if (!coating) {
                    T sinTheta = nI / nT;
                    sinTheta *= sinTheta * (1 - cosThetaI * cosThetaI);

                    if constexpr (std::is_same_v<T, Float>)
                        if (sinTheta >= 1) {
                            reflectance = 1;
                            transmittance = 0;

                            return;
                        }

                    T cosThetaT = sqrt(1 - sinTheta);

                    Float R_s = abs((nI * cosThetaI - nT * cosThetaT) / (nI * cosThetaI + nT * cosThetaT));
                    Float R_p = abs((nI * cosThetaT - nT * cosThetaI) / (nI * cosThetaT + nT * cosThetaI));

                    R_s *= R_s;
                    R_p *= R_p;

                    reflectance = Float(0.5) * (R_s + R_p);

                    if constexpr (std::is_same_v<T, Float>) {
                        Float T_s = abs(2 * nI * cosThetaI / (nI * cosThetaI + nT * cosThetaT));
                        Float T_p = abs(2 * nI * cosThetaI / (nI * cosThetaT + nT * cosThetaI));

                        T_s *= T_s * (nT * cosThetaT) / (nI * cosThetaI);
                        T_p *= T_p * (nT * cosThetaT) / (nI * cosThetaI);

                        transmittance = Float(0.5) * (T_s + T_p);
                    }
                    else transmittance = 0;
                }
                else {
                    int numLayers = coating->coated.numLayers;

                    const DenseSpectrum<Complex> * const * n = coating->coated.n;
                    const ScalarTexture * const * d = coating->coated.d;

                    int jOffset = intersection.frontFacing ? 0 : numLayers - 1;
                    int jSign = intersection.frontFacing ? 1 : -1;

                    Complex n0 = nI;
                    Complex n1 = n[jOffset]->get(lambda);

                    Float nSinThetaSquared = nI * nI * (1 - cosThetaI * cosThetaI);

                    Complex cosCurrent = cosThetaI;
                    Complex sinNext = nSinThetaSquared / (n1 * n1);
                    Complex cosNext = sqrt(1 - sinNext);

                    Matrix<Complex, 2> matrices[2];
                    matrices[0] = utils::interfaceMatrixS(n0, n1, cosCurrent, cosNext);
                    matrices[1] = utils::interfaceMatrixP(n0, n1, cosCurrent, cosNext);

                    for (int i = 0; i < numLayers; i++) {
                        n0 = n[jOffset + jSign * i]->get(lambda);
                        n1 = i == numLayers - 1 ? nT : n[jOffset + jSign * (i + 1)]->get(lambda);

                        cosCurrent = cosNext;
                        sinNext = nSinThetaSquared / (n1 * n1);
                        cosNext = sqrt(1 - sinNext);

                        Matrix<Complex, 2> interfaceS = utils::interfaceMatrixS(n0, n1, cosCurrent, cosNext);
                        Matrix<Complex, 2> interfaceP = utils::interfaceMatrixP(n0, n1, cosCurrent, cosNext);

                        Complex phi = n0 * 2 * constants::PI / lambda * d[jOffset + jSign * i]->evaluate(intersection) * cosCurrent;

                        Matrix<Complex, 2> propagation = utils::propagationMatrix(phi);
                        matrices[0] *= propagation * interfaceS;
                        matrices[1] *= propagation * interfaceP;
                    }

                    Float R_s = abs(matrices[0].get(1, 0) / matrices[0].get(0, 0));
                    Float R_p = abs(matrices[1].get(1, 0) / matrices[1].get(0, 0));

                    R_s *= R_s;
                    R_p *= R_p;

                    reflectance = Float(0.5) * (R_s + R_p);

                    if constexpr (std::is_same_v<T, Float>) {
                        Float T_s = abs(1 / matrices[0].get(0, 0));
                        Float T_p = abs(1 / matrices[1].get(0, 0));

                        Float factor = Float(((nT * cosNext) / (nI * cosThetaI)).real());

                        T_s *= T_s * factor;
                        T_p *= T_p * factor;

                        transmittance = Float(0.5) * (T_s + T_p);
                    }
                    else transmittance = 0;
                }
            }
    };
}