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

            static Material makeConductor(LinearSpectrum<Float> * n0, LinearSpectrum<Complex> * n1, ScalarTexture * alphaX, ScalarTexture * alphaY) {
                Material material;

                material.type = Type::CONDUCTOR;
                material.conductor.n0 = n0;
                material.conductor.n1 = n1;
                material.conductor.alphaX = alphaX;
                material.conductor.alphaY = alphaY;

                return material;
            }

            static Material makeDielectric(LinearSpectrum<Float> * n0, LinearSpectrum<Float> * n1, ScalarTexture * alphaX, ScalarTexture * alphaY) {
                Material material;

                material.type = Type::DIELECTRIC;
                material.dielectric.n0 = n0;
                material.dielectric.n1 = n1;
                material.dielectric.alphaX = alphaX;
                material.dielectric.alphaY = alphaY;

                return material;
            }

            static Material makeCoated(int numLayers, LinearSpectrum<Complex> ** n, ScalarTexture ** d, Material * substrate) {
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

            bool isIdealFresnel() const {
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

            LAMBDA_DEVICE bool isSpecular(const Intersection & intersection) const {
                switch (type) {
                    case Type::LAMBERTIAN: return false;
                    case Type::MIRROR: return true;
                    case Type::CONDUCTOR: return conductor.alphaX->evaluate(intersection) <= constants::SPECULAR_ALPHA || conductor.alphaY->evaluate(intersection) <= constants::SPECULAR_ALPHA;
                    case Type::DIELECTRIC: return dielectric.alphaX->evaluate(intersection) <= constants::SPECULAR_ALPHA || dielectric.alphaY->evaluate(intersection) <= constants::SPECULAR_ALPHA;
                    case Type::COATED: return isSpecularCoated(intersection);
                    case Type::EMISSIVE: return false;
                }

                return false;
            }

            LAMBDA_DEVICE SampledSpectrum emission(const Intersection & intersection, const SampledSpectrum & lambdas) const {
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

            LAMBDA_DEVICE SampledSpectrum evaluate(const Intersection & intersection, const Ray & rIn, const Ray & rOut, SampledSpectrum & probabilityDensity, bool collapsed) const {
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

            LAMBDA_DEVICE SampledSpectrum sample(const Intersection & intersection, Ray & r, const Medium * medium1, SampledSpectrum & probabilityDensity, Float & radianceScale, bool & collapsed, Random & state) const {
                switch (type) {
                    case Type::LAMBERTIAN: return sampleLambertian(intersection, r, probabilityDensity, radianceScale, collapsed, state);
                    case Type::MIRROR: return sampleMirror(intersection, r, probabilityDensity, radianceScale, collapsed);
                    case Type::CONDUCTOR: return sampleConductor(nullptr, intersection, r, probabilityDensity, radianceScale, collapsed, state);
                    case Type::DIELECTRIC: return sampleDielectric(nullptr, intersection, r, medium1, probabilityDensity, radianceScale, collapsed, state);
                    case Type::COATED: return sampleCoated(intersection, r, medium1, probabilityDensity, radianceScale, collapsed, state);
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
                struct { LinearSpectrum<Float> * n0; LinearSpectrum<Complex> * n1; ScalarTexture * alphaX, * alphaY; } conductor;
                struct { LinearSpectrum<Float> * n0, * n1; ScalarTexture * alphaX, * alphaY; } dielectric;
                struct { int numLayers; LinearSpectrum<Complex> ** n; ScalarTexture ** d; Material * substrate; } coated;
                struct { SpectrumTexture * emission; } emissive;
            };

            LAMBDA_DEVICE bool isSpecularCoated(const Intersection & intersection) const {
                switch (coated.substrate->type) {
                    case Type::CONDUCTOR: return coated.substrate->conductor.alphaX->evaluate(intersection) <= constants::SPECULAR_ALPHA || coated.substrate->conductor.alphaY->evaluate(intersection) <= constants::SPECULAR_ALPHA;
                    case Type::DIELECTRIC: return coated.substrate->dielectric.alphaX->evaluate(intersection) <= constants::SPECULAR_ALPHA || coated.substrate->dielectric.alphaY->evaluate(intersection) <= constants::SPECULAR_ALPHA;
                    default: return false;
                }
            }

            LAMBDA_DEVICE SampledSpectrum emissionEmissive(const Intersection & intersection, const SampledSpectrum & lambdas) const {
                SampledSpectrum emission;

                for (int i = 0; i < constants::HERO_COUNT; i++) emission[i] = emissive.emission->evaluate(intersection, lambdas[i]);

                return emission;
            }

            LAMBDA_DEVICE SampledSpectrum evaluateLambertian(const Intersection & intersection, const Ray & r, SampledSpectrum & probabilityDensity, bool collapsed) const {
                Float cosTheta = dot(intersection.transformedNormal, r.getDirection());

                probabilityDensity = cosTheta > constants::GRAZING_COS ? SampledSpectrum(cosTheta / constants::PI) : SampledSpectrum(0);

                if (cosTheta <= constants::GRAZING_COS) return SampledSpectrum(0);

                SampledSpectrum attenuation;

                for (int i = 0; i < constants::HERO_COUNT; i++) attenuation[i] = !collapsed || i == 0 ? lambertian.albedo->evaluate(intersection, r.getLambdas()[i]) / constants::PI : 0;

                return attenuation;
            }

            LAMBDA_DEVICE SampledSpectrum evaluateConductor(const Material * coating, const Intersection & intersection, const Ray & rIn, const Ray & rOut, SampledSpectrum & probabilityDensity, bool collapsed) const {
                Float alphaX = conductor.alphaX->evaluate(intersection);
                Float alphaY = conductor.alphaY->evaluate(intersection);

                if (alphaX <= constants::SPECULAR_ALPHA || alphaY <= constants::SPECULAR_ALPHA) {
                    probabilityDensity = SampledSpectrum(1);

                    return SampledSpectrum(0);
                }

                Vector<Float, 3> vIn = -rIn.getDirection();
                Vector<Float, 3> vOut = rOut.getDirection();

                probabilityDensity = SampledSpectrum(0);

                if (dot(intersection.transformedNormal, vIn) <= constants::GRAZING_COS || dot(intersection.transformedNormal, vOut) <= constants::GRAZING_COS) return SampledSpectrum(0);

                Vector<Float, 3> bitangent = cross(intersection.transformedNormal, intersection.transformedTangent);

                Vector<Float, 3> vInLocal = Vector<Float, 3>(dot(vIn, intersection.transformedTangent), dot(vIn, bitangent), dot(vIn, intersection.transformedNormal));
                Vector<Float, 3> vOutLocal = Vector<Float, 3>(dot(vOut, intersection.transformedTangent), dot(vOut, bitangent), dot(vOut, intersection.transformedNormal));

                Vector<Float, 3> halfVLocal = vInLocal + vOutLocal;

                Float lengthSquared = halfVLocal.lengthSquared();

                if (lengthSquared <= constants::EPSILON_RELATIVE_SQUARED * 2) return SampledSpectrum(0);

                halfVLocal /= std::sqrt(lengthSquared);

                if (halfVLocal[2] < 0) halfVLocal *= -1;

                Float cosTheta = dot(vInLocal, halfVLocal);

                SampledSpectrum reflectance, transmittance;

                for (int i = 0; i < constants::HERO_COUNT; i++)
                    if (!collapsed || i == 0) {
                        Float lambda = rIn.getLambdas()[i];

                        Float n0 = (*conductor.n0)[lambda];
                        Complex n1 = (*conductor.n1)[lambda];

                        fresnel(coating, intersection, n0, n1, cosTheta, lambda, reflectance[i], transmittance[i]);
                    }

                Float D = microfacetD(halfVLocal, alphaX, alphaY);
                Float G1 = microfacetG1(vInLocal, alphaX, alphaY);
                Float G2 = microfacetG2(vInLocal, vOutLocal, alphaX, alphaY);

                probabilityDensity = SampledSpectrum(D * G1 / (4 * std::fabs(vInLocal[2])));

                return reflectance * (D * G2 / (4 * vInLocal[2] * vOutLocal[2]));
            }

            LAMBDA_DEVICE SampledSpectrum evaluateDielectric(const Material * coating, const Intersection & intersection, const Ray & rIn, const Ray & rOut, SampledSpectrum & probabilityDensity, bool collapsed) const {
                Float alphaX = dielectric.alphaX->evaluate(intersection);
                Float alphaY = dielectric.alphaY->evaluate(intersection);

                Vector<Float, 3> vIn = -rIn.getDirection();
                Vector<Float, 3> vOut = rOut.getDirection();

                bool reflect = dot(intersection.transformedNormal, vOut) > constants::GRAZING_COS;

                if (alphaX <= constants::SPECULAR_ALPHA || alphaY <= constants::SPECULAR_ALPHA) {
                    probabilityDensity = SampledSpectrum(1);

                    return SampledSpectrum(0);
                }

                Vector<Float, 3> bitangent = cross(intersection.transformedNormal, intersection.transformedTangent);

                Vector<Float, 3> vInLocal = Vector<Float, 3>(dot(vIn, intersection.transformedTangent), dot(vIn, bitangent), dot(vIn, intersection.transformedNormal));
                Vector<Float, 3> vOutLocal = Vector<Float, 3>(dot(vOut, intersection.transformedTangent), dot(vOut, bitangent), dot(vOut, intersection.transformedNormal));

                Float G1 = microfacetG1(vInLocal, alphaX, alphaY);

                probabilityDensity = SampledSpectrum(0);

                SampledSpectrum reflectance, transmittance, attenuation;

                for (int i = 0; i < constants::HERO_COUNT; i++) {
                    Float lambda = rIn.getLambdas()[i];

                    Float n0 = intersection.frontFacing ? (*dielectric.n0)[lambda] : (*dielectric.n1)[lambda];
                    Float n1 = intersection.frontFacing ? (*dielectric.n1)[lambda] : (*dielectric.n0)[lambda];

                    Float ratio = n0 / n1;

                    if (!reflect && std::fabs(n0 - n1) <= constants::DISPERSION_DELTA) probabilityDensity[i] = std::fabs(dot(intersection.transformedNormal, vOut));
                    else {
                        Vector<Float, 3> halfVLocal = reflect ? vInLocal + vOutLocal : -(vInLocal * ratio + vOutLocal);

                        Float lengthSquared = halfVLocal.lengthSquared();

                        if (lengthSquared > constants::EPSILON_RELATIVE_SQUARED * (ratio * ratio + 1)) {
                            halfVLocal /= std::sqrt(lengthSquared);

                            if (halfVLocal[2] < 0) halfVLocal *= -1;

                            Float cosTheta = dot(halfVLocal, vInLocal);

                            if (cosTheta > constants::GRAZING_COS && (reflect || dot(halfVLocal, vOutLocal) < 0)) {
                                fresnel(coating, intersection, n0, n1, cosTheta, lambda, reflectance[i], transmittance[i]);

                                Vector<Float, 3> vOutLocalShadow(vOutLocal[0], vOutLocal[1], std::fabs(vOutLocal[2]));

                                Float D = microfacetD(halfVLocal, alphaX, alphaY);
                                Float G2 = microfacetG2(vInLocal, vOutLocalShadow, alphaX, alphaY);

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

            LAMBDA_DEVICE SampledSpectrum evaluateCoated(const Intersection & intersection, const Ray & rIn, const Ray & rOut, SampledSpectrum & probabilityDensity, bool collapsed) const {
                switch (coated.substrate->type) {
                    case Type::CONDUCTOR: return coated.substrate->evaluateConductor(this, intersection, rIn, rOut, probabilityDensity, collapsed);
                    case Type::DIELECTRIC: return coated.substrate->evaluateDielectric(this, intersection, rIn, rOut, probabilityDensity, collapsed);
                    default: return SampledSpectrum(0);
                }
            }

            LAMBDA_DEVICE SampledSpectrum sampleLambertian(const Intersection & intersection, Ray & r, SampledSpectrum & probabilityDensity, Float & radianceScale, bool & collapsed, Random & state) const {
                r = Ray(intersection.transformedPoint + intersection.geometricNormal * intersection.pointError, utils::randomInHemisphere(intersection.transformedNormal, state), r.getLambdas(), r.getMedium());

                radianceScale = 1;

                SampledSpectrum attenuation = evaluateLambertian(intersection, r, probabilityDensity, collapsed);

                if (!(probabilityDensity[0] > 0)) return SampledSpectrum(0);

                return attenuation * dot(intersection.transformedNormal, r.getDirection()) / probabilityDensity[0];
            }

            LAMBDA_DEVICE SampledSpectrum sampleMirror(const Intersection & intersection, Ray & r, SampledSpectrum & probabilityDensity, Float & radianceScale, bool collapsed) const {
                r = Ray(intersection.transformedPoint + intersection.geometricNormal * intersection.pointError, utils::reflected(r.getDirection(), intersection.transformedNormal), r.getLambdas(), r.getMedium());

                probabilityDensity = SampledSpectrum(1);
                radianceScale = 1;

                SampledSpectrum attenuation;

                for (int i = 0; i < constants::HERO_COUNT; i++) attenuation[i] = !collapsed || i == 0 ? mirror.albedo->evaluate(intersection, r.getLambdas()[i]) : 0;

                return attenuation;
            }

            LAMBDA_DEVICE SampledSpectrum sampleConductor(const Material * coating, const Intersection & intersection, Ray & r, SampledSpectrum & probabilityDensity, Float & radianceScale, bool & collapsed, Random & state) const {
                Vector<Float, 3> normal = intersection.transformedNormal;

                Float alphaX = conductor.alphaX->evaluate(intersection);
                Float alphaY = conductor.alphaY->evaluate(intersection);

                if (alphaX > constants::SPECULAR_ALPHA && alphaY > constants::SPECULAR_ALPHA) normal = microfacetNormal(-r.getDirection(), normal, intersection.transformedTangent, alphaX, alphaY, state);

                Vector<Float, 3> direction = utils::reflected(r.getDirection(), normal);

                if (dot(intersection.transformedNormal, direction) <= constants::GRAZING_COS) return SampledSpectrum(0);

                Ray rOut(intersection.transformedPoint + intersection.geometricNormal * intersection.pointError, direction, r.getLambdas(), r.getMedium());

                radianceScale = 1;

                SampledSpectrum attenuation;

                if ((alphaX > constants::SPECULAR_ALPHA && alphaY > constants::SPECULAR_ALPHA)) {
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

                            Float n0 = (*conductor.n0)[lambda];
                            Complex n1 = (*conductor.n1)[lambda];

                            fresnel(coating, intersection, n0, n1, cosTheta, lambda, reflectance[i], transmittance[i]);

                            attenuation[i] = reflectance[i];
                        }
                }

                r = rOut;

                return attenuation;
            }

            LAMBDA_DEVICE SampledSpectrum sampleDielectric(const Material * coating, const Intersection & intersection, Ray & r, const Medium * medium1, SampledSpectrum & probabilityDensity, Float & radianceScale, bool & collapsed, Random & state) const {
                Vector<Float, 3> normal = intersection.transformedNormal;

                Float alphaX = dielectric.alphaX->evaluate(intersection);
                Float alphaY = dielectric.alphaY->evaluate(intersection);

                if (alphaX > constants::SPECULAR_ALPHA && alphaY > constants::SPECULAR_ALPHA) normal = microfacetNormal(-r.getDirection(), normal, intersection.transformedTangent, alphaX, alphaY, state);

                Float lambda = r.getLambdas()[0];

                Float n0Hero = intersection.frontFacing ? (*dielectric.n0)[lambda] : (*dielectric.n1)[lambda];
                Float n1Hero = intersection.frontFacing ? (*dielectric.n1)[lambda] : (*dielectric.n0)[lambda];

                bool collapse = false;

                Float cosTheta = dot(normal, -r.getDirection());

                SampledSpectrum reflectance, transmittance, ratio;

                for (int i = 0; i < constants::HERO_COUNT; i++) {
                    lambda = r.getLambdas()[i];

                    Float n0 = intersection.frontFacing ? (*dielectric.n0)[lambda] : (*dielectric.n1)[lambda];
                    Float n1 = intersection.frontFacing ? (*dielectric.n1)[lambda] : (*dielectric.n0)[lambda];

                    if (!collapsed && (std::fabs(n0Hero - n0) > constants::DISPERSION_DELTA || std::fabs(n1Hero - n1) > constants::DISPERSION_DELTA)) collapse = true;

                    fresnel(coating, intersection, n0, n1, cosTheta, lambda, reflectance[i], transmittance[i]);

                    ratio[i] = n0 / n1;
                }

                if (!(reflectance[0] + transmittance[0] > 0)) return SampledSpectrum(0);

                Float u = utils::randomFloat(state);

                if (u >= reflectance[0] + transmittance[0]) return SampledSpectrum(0);

                bool reflect = u < reflectance[0] || reflectance[0] >= 1;

                Vector<Float, 3> direction = reflect ? utils::reflected(r.getDirection(), normal) : utils::refracted(r.getDirection(), normal, n0Hero / n1Hero);

                if ((alphaX > constants::SPECULAR_ALPHA && alphaY > constants::SPECULAR_ALPHA) && (reflect ? dot(intersection.transformedNormal, direction) <= constants::GRAZING_COS : dot(intersection.transformedNormal, direction) >= -constants::GRAZING_COS)) return SampledSpectrum(0);

                Vector<Float, 3> offset = intersection.geometricNormal * intersection.pointError;

                if (!reflect) offset *= -1;

                Ray rOut(intersection.transformedPoint + offset, direction, r.getLambdas(), reflect ? r.getMedium() : medium1);

                if (!reflect && collapse) collapsed = true;

                radianceScale = reflect ? 1 : ratio[0] * ratio[0];

                SampledSpectrum attenuation;

                if ((alphaX > constants::SPECULAR_ALPHA && alphaY > constants::SPECULAR_ALPHA) && (reflect || std::fabs(n0Hero - n1Hero) > constants::DISPERSION_DELTA)) {
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

            LAMBDA_DEVICE SampledSpectrum sampleCoated(const Intersection & intersection, Ray & r, const Medium * medium1, SampledSpectrum & probabilityDensity, Float & radianceScale, bool & collapsed, Random & state) const {
                switch (coated.substrate->type) {
                    case Type::CONDUCTOR: return coated.substrate->sampleConductor(this, intersection, r, probabilityDensity, radianceScale, collapsed, state);
                    case Type::DIELECTRIC: return coated.substrate->sampleDielectric(this, intersection, r, medium1, probabilityDensity, radianceScale, collapsed, state);
                    default: return SampledSpectrum(0);
                }
            }

            template <typename T>
            LAMBDA_DEVICE static void fresnel(const Material * coating, const Intersection & intersection, Float nI, const T & nT, Float cosThetaI, Float lambda, Float & reflectance, Float & transmittance) {
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

                    const LinearSpectrum<Complex> * const * n = coating->coated.n;
                    const ScalarTexture * const * d = coating->coated.d;

                    int jOffset = intersection.frontFacing ? 0 : numLayers - 1;
                    int jSign = intersection.frontFacing ? 1 : -1;

                    Complex n0 = nI;
                    Complex n1 = (*n[jOffset])[lambda];

                    Float nSinThetaSquared = nI * nI * (1 - cosThetaI * cosThetaI);

                    Complex cosCurrent = cosThetaI;
                    Complex sinNext = nSinThetaSquared / (n1 * n1);
                    Complex cosNext = sqrt(1 - sinNext);

                    Matrix<Complex, 2> matrices[2];
                    matrices[0] = interfaceMatrixS(n0, n1, cosCurrent, cosNext);
                    matrices[1] = interfaceMatrixP(n0, n1, cosCurrent, cosNext);

                    for (int i = 0; i < numLayers; i++) {
                        n0 = (*n[jOffset + jSign * i])[lambda];
                        n1 = i == numLayers - 1 ? nT : (*n[jOffset + jSign * (i + 1)])[lambda];

                        cosCurrent = cosNext;
                        sinNext = nSinThetaSquared / (n1 * n1);
                        cosNext = sqrt(1 - sinNext);

                        Matrix<Complex, 2> interfaceS = interfaceMatrixS(n0, n1, cosCurrent, cosNext);
                        Matrix<Complex, 2> interfaceP = interfaceMatrixP(n0, n1, cosCurrent, cosNext);

                        Complex phi = n0 * 2 * constants::PI / lambda * d[jOffset + jSign * i]->evaluate(intersection) * cosCurrent;

                        Matrix<Complex, 2> propagation = propagationMatrix(phi);
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

            LAMBDA_DEVICE static Matrix<Complex, 2> interfaceMatrixS(const Complex & n1, const Complex & n2, const Complex & cos1, const Complex & cos2) {
            Complex r = (n1 * cos1 - n2 * cos2) / (n1 * cos1 + n2 * cos2);

            Matrix<Complex, 2> interfaceMatrix(Complex(1), r, r, Complex(1));

            interfaceMatrix /= Complex(2) * n1 * cos1 / (n1 * cos1 + n2 * cos2);

            return interfaceMatrix;
            }

            LAMBDA_DEVICE static Matrix<Complex, 2> interfaceMatrixP(const Complex & n1, const Complex & n2, const Complex & cos1, const Complex & cos2) {
            Complex r = (n2 * cos1 - n1 * cos2) / (n2 * cos1 + n1 * cos2);

            Matrix<Complex, 2> interfaceMatrix(Complex(1), r, r, Complex(1));

            interfaceMatrix /= Complex(2) * n1 * cos1 / (n2 * cos1 + n1 * cos2);

            return interfaceMatrix;
            }

            LAMBDA_DEVICE static Matrix<Complex, 2> propagationMatrix(const Complex & phi) { return Matrix<Complex, 2>(exp(Complex(0, -1) * phi), Complex(0), Complex(0), exp(Complex(0, 1) * phi)); }

            LAMBDA_DEVICE static Vector<Float, 3> microfacetNormal(const Vector<Float, 3> & v, const Vector<Float, 3> & normal, const Vector<Float, 3> & tangent, Float alphaX, Float alphaY, Random & state) {
                Vector<Float, 3> bitangent = cross(normal, tangent);

                Vector<Float, 3> vLocal = Vector<Float, 3>(dot(v, tangent), dot(v, bitangent), dot(v, normal));

                Vector<Float, 3> vh = normalize(Vector<Float, 3>(alphaX * vLocal[0], alphaY * vLocal[1], vLocal[2]));

                Float lengthSquared = vh[0] * vh[0] + vh[1] * vh[1];

                Vector<Float, 3> t1 = lengthSquared > 0 ? Vector<Float, 3>(-vh[1], vh[0], 0) / std::sqrt(lengthSquared) : Vector<Float, 3>(1, 0, 0);
                Vector<Float, 3> t2 = cross(vh, t1);

                Float r = std::sqrt(utils::randomFloat(state));
                Float phi = 2 * constants::PI * utils::randomFloat(state);

                Float t1Disk = r * std::cos(phi);
                Float t2Disk = r * std::sin(phi);

                Float s = Float(0.5) * (1 + vh[2]);

                t2Disk = (1 - s) * std::sqrt(std::fmax(1 - t1Disk * t1Disk, Float(0))) + s * t2Disk;

                Vector<Float, 3> nh = t1Disk * t1 + t2Disk * t2 + std::sqrt(std::fmax(1 - t1Disk * t1Disk - t2Disk * t2Disk, Float(0))) * vh;

                Vector<Float, 3> nLocal = normalize(Vector<Float, 3>(alphaX * nh[0], alphaY * nh[1], std::fmax(nh[2], Float(0))));

                return nLocal[0] * tangent + nLocal[1] * bitangent + nLocal[2] * normal;
            }

            LAMBDA_DEVICE static Float microfacetD(const Vector<Float, 3> & mLocal, Float alphaX, Float alphaY) {
                if (mLocal[2] <= constants::GRAZING_COS) return 0;

                Float x2 = mLocal[0] * mLocal[0] / (alphaX * alphaX);
                Float y2 = mLocal[1] * mLocal[1] / (alphaY * alphaY);
                Float z2 = mLocal[2] * mLocal[2];

                Float denominator = x2 + y2 + z2;

                return 1 / (constants::PI * alphaX * alphaY * denominator * denominator);
            }

            LAMBDA_DEVICE static Float microfacetG1(const Vector<Float, 3> & vLocal, Float alphaX, Float alphaY) { return 1 / (1 + microfacetLambda(vLocal, alphaX, alphaY)); }

            LAMBDA_DEVICE static Float microfacetG2(const Vector<Float, 3> & vInLocal, const Vector<Float, 3> & vOutLocal, Float alphaX, Float alphaY) { return 1 / (1 + microfacetLambda(vInLocal, alphaX, alphaY) + microfacetLambda(vOutLocal, alphaX, alphaY)); }

            LAMBDA_DEVICE static Float microfacetLambda(const Vector<Float, 3> & vLocal, Float alphaX, Float alphaY) {
                if (vLocal[2] <= constants::GRAZING_COS) return 0;

                Float x2 = vLocal[0] * vLocal[0] * (alphaX * alphaX);
                Float y2 = vLocal[1] * vLocal[1] * (alphaY * alphaY);
                Float z2 = vLocal[2] * vLocal[2];

                return Float(0.5) * (std::sqrt(1 + (x2 + y2) / z2) - 1);
            }
    };
}