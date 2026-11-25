#pragma once

#include <cmath>

#include "core/platform.h"
#include "core/utils.h"
#include "geometry/intersection.h"
#include "math/complex.h"
#include "math/matrix.h"
#include "math/random.h"
#include "math/ray.h"
#include "math/spectrum.h"
#include "math/vector.h"
#include "shading/fluorophore.h"
#include "shading/texture.h"

namespace lambda {
    class Medium;

    class Material {
        public:
            Material() : type(Type::DIFFUSE) {}

            static Material makeDiffuse(SpectrumTexture * r, Fluorophore * f) {
                Material material;

                material.type = Type::DIFFUSE;
                material.diffuse.reflectance = r;
                material.diffuse.fluorophore = f;

                return material;
            }

            static Material makeFresnel(LinearSpectrum<Complex> * n0, LinearSpectrum<Complex> * n1, ScalarTexture * alphaX, ScalarTexture * alphaY) {
                Material material;

                material.type = Type::FRESNEL;
                material.fresnel.n0 = n0;
                material.fresnel.n1 = n1;
                material.fresnel.alphaX = alphaX;
                material.fresnel.alphaY = alphaY;

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
                    case Type::DIFFUSE: return false;
                    case Type::FRESNEL: return true;
                    case Type::COATED: return false;
                    case Type::EMISSIVE: return false;
                }

                return false;
            }

            LAMBDA_HOST_DEVICE bool isEmissive() const {
                switch (type) {
                    case Type::DIFFUSE: return false;
                    case Type::FRESNEL: return false;
                    case Type::COATED: return false;
                    case Type::EMISSIVE: return true;
                }

                return false;
            }

            LAMBDA_HOST_DEVICE Float averageEmission() const {
                switch (type) {
                    case Type::DIFFUSE: return 0;
                    case Type::FRESNEL: return 0;
                    case Type::COATED: return 0;
                    case Type::EMISSIVE: return emissive.emission->getAverage();
                }

                return 0;
            }

            LAMBDA_DEVICE bool isSpecular(const Intersection & intersection) const {
                switch (type) {
                    case Type::DIFFUSE: return false;
                    case Type::FRESNEL: return fresnel.alphaX->evaluate(intersection) <= constants::SPECULAR_ALPHA || fresnel.alphaY->evaluate(intersection) <= constants::SPECULAR_ALPHA;
                    case Type::COATED: return coated.substrate->fresnel.alphaX->evaluate(intersection) <= constants::SPECULAR_ALPHA || coated.substrate->fresnel.alphaY->evaluate(intersection) <= constants::SPECULAR_ALPHA;
                    case Type::EMISSIVE: return false;
                }

                return false;
            }

            LAMBDA_DEVICE SampledSpectrum<Float> sampleLambdas(const Intersection & intersection, const SampledSpectrum<Float> & lambdas, SampledSpectrum<Float> & probabilityDensity, bool collapsed, Random & state) const {
                switch (type) {
                    case Type::DIFFUSE: return sampleLambdasDiffuse(intersection, lambdas, probabilityDensity, collapsed, state);
                    case Type::FRESNEL: return lambdas;
                    case Type::COATED: return lambdas;
                    case Type::EMISSIVE: return lambdas;
                }

                return lambdas;
            }

            LAMBDA_DEVICE SampledSpectrum<Float> emission(const Intersection & intersection, const Ray & r, bool collapsed) const {
                switch (type) {
                    case Type::DIFFUSE: return SampledSpectrum<Float>();
                    case Type::FRESNEL: return SampledSpectrum<Float>();
                    case Type::COATED: return SampledSpectrum<Float>();
                    case Type::EMISSIVE: return emissionEmissive(intersection, r, collapsed);
                }

                return SampledSpectrum<Float>();
            }

            LAMBDA_DEVICE SampledSpectrum<Float> evaluate(const Intersection & intersection, const Ray & rIn, const Ray & rOut, SampledSpectrum<Float> & probabilityDensity, bool collapsed) const {
                switch (type) {
                    case Type::DIFFUSE: return evaluateDiffuse(intersection, rIn, rOut, probabilityDensity, collapsed);
                    case Type::FRESNEL: return evaluateFresnel(nullptr, intersection, rIn, rOut, probabilityDensity, collapsed);
                    case Type::COATED: return coated.substrate->evaluateFresnel(this, intersection, rIn, rOut, probabilityDensity, collapsed);
                    case Type::EMISSIVE: return SampledSpectrum<Float>();
                }

                return SampledSpectrum<Float>();
            }

            LAMBDA_DEVICE SampledSpectrum<Float> sample(const Intersection & intersection, Ray & r, const Medium * medium1, SampledSpectrum<Float> & probabilityDensity, Float & radianceScale, bool & collapsed, Random & state) const {
                switch (type) {
                    case Type::DIFFUSE: return sampleDiffuse(intersection, r, probabilityDensity, radianceScale, collapsed, state);
                    case Type::FRESNEL: return sampleFresnel(nullptr, intersection, r, medium1, probabilityDensity, radianceScale, collapsed, state);
                    case Type::COATED: return coated.substrate->sampleFresnel(this, intersection, r, medium1, probabilityDensity, radianceScale, collapsed, state);
                    case Type::EMISSIVE: return SampledSpectrum<Float>();
                }

                return SampledSpectrum<Float>();
            }

            LAMBDA_DEVICE static void fresnelReflectanceTransmittance(const Material * coating, const Intersection & intersection, const Complex & nI, const Complex & nT, Float cosThetaI, Float lambda, Float & reflectance, Float & transmittance) {
                bool absorptive = nT.imag() > constants::ABSORPTIVE_EXTINCTION;

                Complex cosThetaT = Complex(0);

                Float R_s = 0;
                Float R_p = 0;

                Float T_s = 0;
                Float T_p = 0;

                if (!coating) {
                    if (!absorptive) {
                        Float sinTheta = nI.real() / nT.real();

                        sinTheta *= sinTheta * (1 - cosThetaI * cosThetaI);

                        if (sinTheta >= 1) {
                            R_s = 1;
                            R_p = 1;

                            T_s = 0;
                            T_p = 0;
                        }
                        else {
                            cosThetaT = std::sqrt(1 - sinTheta);

                            R_s = std::fabs((nI.real() * cosThetaI - nT.real() * cosThetaT.real()) / (nI.real() * cosThetaI + nT.real() * cosThetaT.real()));
                            R_p = std::fabs((nI.real() * cosThetaT.real() - nT.real() * cosThetaI) / (nI.real() * cosThetaT.real() + nT.real() * cosThetaI));

                            T_s = std::fabs(2 * nI.real() * cosThetaI / (nI.real() * cosThetaI + nT.real() * cosThetaT.real()));
                            T_p = std::fabs(2 * nI.real() * cosThetaI / (nI.real() * cosThetaT.real() + nT.real() * cosThetaI));
                        }
                    }
                    else {
                        Complex sinTheta = nI / nT;
                        sinTheta *= sinTheta * (1 - cosThetaI * cosThetaI);

                        cosThetaT = sqrt(1 - sinTheta);

                        R_s = abs((nI * cosThetaI - nT * cosThetaT) / (nI * cosThetaI + nT * cosThetaT));
                        R_p = abs((nI * cosThetaT - nT * cosThetaI) / (nI * cosThetaT + nT * cosThetaI));
                    }
                }
                else {
                    int numLayers = coating->coated.numLayers;

                    const LinearSpectrum<Complex> * const * n = coating->coated.n;
                    const ScalarTexture * const * d = coating->coated.d;

                    int jOffset = intersection.frontFacing ? 0 : numLayers - 1;
                    int jSign = intersection.frontFacing ? 1 : -1;

                    Complex n0 = nI;
                    Complex n1 = (*n[jOffset])[lambda];

                    Complex nSinThetaSquared = nI * nI * (1 - cosThetaI * cosThetaI);

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

                        Matrix<Complex, 2> propagation = Matrix<Complex, 2>(exp(Complex(0, -1) * phi), Complex(0), Complex(0), exp(Complex(0, 1) * phi));
                        matrices[0] *= propagation * interfaceS;
                        matrices[1] *= propagation * interfaceP;
                    }

                    cosThetaT = cosNext;

                    R_s = abs(matrices[0](1, 0) / matrices[0](0, 0));
                    R_p = abs(matrices[1](1, 0) / matrices[1](0, 0));

                    if (!absorptive) {
                        T_s = abs(1 / matrices[0](0, 0));
                        T_p = abs(1 / matrices[1](0, 0));
                    }
                }

                R_s *= R_s;
                R_p *= R_p;

                reflectance = Float(0.5) * (R_s + R_p);

                if (!absorptive) {
                    Float factor = (nT.real() * cosThetaT.real()) / (nI.real() * cosThetaI);

                    T_s *= T_s * factor;
                    T_p *= T_p * factor;

                    transmittance = Float(0.5) * (T_s + T_p);
                }
                else transmittance = 0;
            }

        private:
            enum class Type { DIFFUSE, FRESNEL, COATED, EMISSIVE };

            Type type;

            union {
                struct { SpectrumTexture * reflectance; Fluorophore * fluorophore; } diffuse;
                struct { LinearSpectrum<Complex> * n0; LinearSpectrum<Complex> * n1; ScalarTexture * alphaX, * alphaY; } fresnel;
                struct { int numLayers; LinearSpectrum<Complex> ** n; ScalarTexture ** d; Material * substrate; } coated;
                struct { SpectrumTexture * emission; } emissive;
            };

            LAMBDA_DEVICE SampledSpectrum<Float> sampleLambdasDiffuse(const Intersection & intersection, const SampledSpectrum<Float> & lambdas, SampledSpectrum<Float> & probabilityDensity, bool collapsed, Random & state) const {
                if (!diffuse.fluorophore) return lambdas;

                SampledSpectrum<Float> lambdasExcitation = lambdas;

                for (int i = 0; i < (!collapsed ? constants::HERO_COUNT : 1); i++) {
                    Float lambdaEmission = lambdas[i];

                    lambdasExcitation[i] = diffuse.fluorophore->sample(lambdaEmission, diffuse.reflectance->evaluate(intersection, lambdaEmission), probabilityDensity[i], state);
                }

                return lambdasExcitation;
            }

            LAMBDA_DEVICE SampledSpectrum<Float> emissionEmissive(const Intersection & intersection, const Ray & r, bool collapsed) const {
                SampledSpectrum<Float> emission;

                for (int i = 0; i < (!collapsed ? constants::HERO_COUNT : 1); i++) emission[i] = emissive.emission->evaluate(intersection, r.getLambdas()[i]);

                return emission;
            }

            LAMBDA_DEVICE SampledSpectrum<Float> evaluateDiffuse(const Intersection & intersection, const Ray & rIn, const Ray & rOut, SampledSpectrum<Float> & probabilityDensity, bool collapsed) const {
                Float cosTheta = dot(intersection.transformedNormal, rOut.getDirection());

                probabilityDensity = cosTheta > constants::GRAZING_COS ? SampledSpectrum<Float>(cosTheta / constants::PI) : SampledSpectrum<Float>();

                if (cosTheta <= constants::GRAZING_COS) return SampledSpectrum<Float>();

                SampledSpectrum<Float> attenuation;

                for (int i = 0; i < (!collapsed ? constants::HERO_COUNT : 1); i++) {
                    attenuation[i] = diffuse.reflectance->evaluate(intersection, rIn.getLambdas()[i]);

                    if (diffuse.fluorophore) attenuation[i] = diffuse.fluorophore->evaluate(rOut.getLambdas()[i], rIn.getLambdas()[i], attenuation[i]);

                    attenuation[i] /= constants::PI;
                }

                return attenuation;
            }

            LAMBDA_DEVICE SampledSpectrum<Float> evaluateFresnel(const Material * coating, const Intersection & intersection, const Ray & rIn, const Ray & rOut, SampledSpectrum<Float> & probabilityDensity, bool collapsed) const {
                Float alphaX = fresnel.alphaX->evaluate(intersection);
                Float alphaY = fresnel.alphaY->evaluate(intersection);

                if (alphaX <= constants::SPECULAR_ALPHA || alphaY <= constants::SPECULAR_ALPHA) {
                    probabilityDensity = SampledSpectrum<Float>(1);

                    return SampledSpectrum<Float>();
                }

                Vector<Float, 3> vIn = -rIn.getDirection();
                Vector<Float, 3> vOut = rOut.getDirection();

                bool reflect = dot(intersection.transformedNormal, vOut) > constants::GRAZING_COS;

                SampledSpectrum<Complex> n0, n1;

                for (int i = 0; i < (!collapsed ? constants::HERO_COUNT : 1); i++) {
                    Float lambda = rIn.getLambdas()[i];

                    n0[i] = intersection.frontFacing ? (*fresnel.n0)[lambda] : (*fresnel.n1)[lambda];
                    n1[i] = intersection.frontFacing ? (*fresnel.n1)[lambda] : (*fresnel.n0)[lambda];
                }

                if (n1[0].imag() > constants::ABSORPTIVE_EXTINCTION && (dot(intersection.transformedNormal, vIn) <= constants::GRAZING_COS || dot(intersection.transformedNormal, vOut) <= constants::GRAZING_COS)) {
                    probabilityDensity = SampledSpectrum<Float>(1);

                    return SampledSpectrum<Float>();
                }

                Vector<Float, 3> bitangent = cross(intersection.transformedNormal, intersection.transformedTangent);

                Vector<Float, 3> vInLocal = Vector<Float, 3>(dot(vIn, intersection.transformedTangent), dot(vIn, bitangent), dot(vIn, intersection.transformedNormal));
                Vector<Float, 3> vOutLocal = Vector<Float, 3>(dot(vOut, intersection.transformedTangent), dot(vOut, bitangent), dot(vOut, intersection.transformedNormal));

                Float G1 = 1 / (1 + microfacetLambda(vInLocal, alphaX, alphaY));

                SampledSpectrum<Float> attenuation;

                probabilityDensity = SampledSpectrum<Float>();

                for (int i = 0; i < (!collapsed ? constants::HERO_COUNT : 1); i++) {
                    if (!reflect && std::fabs(n0[i].real() - n1[i].real()) <= constants::DISPERSIVE_DELTA) {
                        probabilityDensity[i] = std::fabs(dot(intersection.transformedNormal, vOut));

                        continue;
                    }

                    bool absorptive = n1[i].imag() > constants::ABSORPTIVE_EXTINCTION;

                    Float ratio = !reflect && !absorptive ? n0[i].real() / n1[i].real() : 1;

                    Vector<Float, 3> halfVLocal = reflect ? vInLocal + vOutLocal : -(vInLocal * ratio + vOutLocal);

                    Float lengthSquared = halfVLocal.lengthSquared();

                    if (lengthSquared <= constants::EPSILON_RELATIVE_SQUARED * (ratio * ratio + 1)) continue;

                    halfVLocal /= std::sqrt(lengthSquared);

                    if (halfVLocal[2] < 0) halfVLocal *= -1;

                    Float cosTheta = dot(halfVLocal, vInLocal);

                    if (cosTheta <= constants::GRAZING_COS || (!reflect && dot(halfVLocal, vOutLocal) >= 0)) continue;

                    Float reflectance = 0;
                    Float transmittance = 0;

                    fresnelReflectanceTransmittance(coating, intersection, n0[i], n1[i], cosTheta, rIn.getLambdas()[i], reflectance, transmittance);

                    Vector<Float, 3> vOutLocalShadow(vOutLocal[0], vOutLocal[1], std::fabs(vOutLocal[2]));

                    Float D = 0;

                    if (halfVLocal[2] > constants::GRAZING_COS) {
                        Float x2 = halfVLocal[0] * halfVLocal[0] / (alphaX * alphaX);
                        Float y2 = halfVLocal[1] * halfVLocal[1] / (alphaY * alphaY);
                        Float z2 = halfVLocal[2] * halfVLocal[2];

                        Float denominator = x2 + y2 + z2;

                        D = 1 / (constants::PI * alphaX * alphaY * denominator * denominator);
                    }

                    Float G2 = 1 / (1 + microfacetLambda(vInLocal, alphaX, alphaY) + microfacetLambda(vOutLocal, alphaX, alphaY));

                    if (reflect) {
                        probabilityDensity[i] = reflectance + transmittance > 0 ? reflectance / (reflectance + transmittance) * D * G1 / (4 * std::fabs(vInLocal[2])) : 0;
                        attenuation[i] = reflectance * (D * G2 / (4 * vInLocal[2] * vOutLocalShadow[2]));
                    }
                    else {
                        Float denominator = ratio * dot(vInLocal, halfVLocal) + dot(vOutLocal, halfVLocal);

                        probabilityDensity[i] = reflectance + transmittance > 0 ? transmittance / (reflectance + transmittance) * D * G1 * cosTheta * std::fabs(dot(vOutLocal, halfVLocal)) / (denominator * denominator * std::fabs(vInLocal[2])) : 0;
                        attenuation[i] = transmittance * D * G2 * cosTheta * std::fabs(dot(vOutLocal, halfVLocal)) / (denominator * denominator * std::fabs(vInLocal[2]) * std::fabs(vOutLocal[2])) * ratio * ratio;
                    }
                }

                return attenuation;
            }

            LAMBDA_DEVICE SampledSpectrum<Float> sampleDiffuse(const Intersection & intersection, Ray & r, SampledSpectrum<Float> & probabilityDensity, Float & radianceScale, bool & collapsed, Random & state) const {
                SampledSpectrum<Float> lambdas = r.getLambdas();
                SampledSpectrum<Float> shiftProbabilityDensity(1);

                if (diffuse.fluorophore) lambdas = sampleLambdasDiffuse(intersection, lambdas, shiftProbabilityDensity, collapsed, state);

                Ray rOut = Ray(intersection.transformedPoint + intersection.geometricNormal * intersection.pointError, utils::randomInHemisphere(intersection.transformedNormal, state), lambdas, r.getMedium());

                radianceScale = 1;

                SampledSpectrum<Float> attenuation = evaluateDiffuse(intersection, r, rOut, probabilityDensity, collapsed);

                r = rOut;

                if (!(probabilityDensity[0] > 0)) return SampledSpectrum<Float>();

                for (int i = 0; i < (!collapsed ? constants::HERO_COUNT : 1); i++)
                    if (!(shiftProbabilityDensity[i] > 0)) return SampledSpectrum<Float>();

                return attenuation * dot(intersection.transformedNormal, r.getDirection()) / (probabilityDensity[0] * shiftProbabilityDensity);
            }

            LAMBDA_DEVICE SampledSpectrum<Float> sampleFresnel(const Material * coating, const Intersection & intersection, Ray & r, const Medium * medium1, SampledSpectrum<Float> & probabilityDensity, Float & radianceScale, bool & collapsed, Random & state) const {
                Vector<Float, 3> normal = intersection.transformedNormal;

                Float alphaX = fresnel.alphaX->evaluate(intersection);
                Float alphaY = fresnel.alphaY->evaluate(intersection);

                if (alphaX > constants::SPECULAR_ALPHA && alphaY > constants::SPECULAR_ALPHA) {
                    Vector<Float, 3> bitangent = cross(normal, intersection.transformedTangent);

                    Vector<Float, 3> v = -r.getDirection();

                    Vector<Float, 3> vLocal = Vector<Float, 3>(dot(v, intersection.transformedTangent), dot(v, bitangent), dot(v, normal));

                    Vector<Float, 3> vh = normalize(Vector<Float, 3>(alphaX * vLocal[0], alphaY * vLocal[1], vLocal[2]));

                    Float lengthSquared = vh[0] * vh[0] + vh[1] * vh[1];

                    Vector<Float, 3> t1 = lengthSquared > 0 ? Vector<Float, 3>(-vh[1], vh[0], 0) / std::sqrt(lengthSquared) : Vector<Float, 3>(1, 0, 0);
                    Vector<Float, 3> t2 = cross(vh, t1);

                    Float radius = std::sqrt(utils::randomFloat(state));
                    Float phi = 2 * constants::PI * utils::randomFloat(state);

                    Float t1Disk = radius * std::cos(phi);
                    Float t2Disk = radius * std::sin(phi);

                    Float s = Float(0.5) * (1 + vh[2]);

                    t2Disk = (1 - s) * std::sqrt(std::fmax(1 - t1Disk * t1Disk, Float(0))) + s * t2Disk;

                    Vector<Float, 3> nh = t1Disk * t1 + t2Disk * t2 + std::sqrt(std::fmax(1 - t1Disk * t1Disk - t2Disk * t2Disk, Float(0))) * vh;

                    Vector<Float, 3> nLocal = normalize(Vector<Float, 3>(alphaX * nh[0], alphaY * nh[1], std::fmax(nh[2], Float(0))));

                    normal = nLocal[0] * intersection.transformedTangent + nLocal[1] * bitangent + nLocal[2] * normal;
                }

                Float cosTheta = dot(normal, -r.getDirection());

                bool collapse = false;

                SampledSpectrum<Complex> n0, n1;

                for (int i = 0; i < (!collapsed ? constants::HERO_COUNT : 1); i++) {
                    Float lambda = r.getLambdas()[i];

                    n0[i] = intersection.frontFacing ? (*fresnel.n0)[lambda] : (*fresnel.n1)[lambda];
                    n1[i] = intersection.frontFacing ? (*fresnel.n1)[lambda] : (*fresnel.n0)[lambda];

                    if (n1[i].imag() <= constants::ABSORPTIVE_EXTINCTION && (std::fabs(n0[i].real() - n0[0].real()) > constants::DISPERSIVE_DELTA || std::fabs(n1[i].real() - n1[0].real()) > constants::DISPERSIVE_DELTA)) collapse = true;
                }

                Float reflectance = 0;
                Float transmittance = 0;

                fresnelReflectanceTransmittance(coating, intersection, n0[0], n1[0], cosTheta, r.getLambdas()[0], reflectance, transmittance);

                if (!(reflectance + transmittance > 0)) {
                    probabilityDensity = SampledSpectrum<Float>(1);
                    radianceScale = 1;

                    return SampledSpectrum<Float>();
                }

                bool reflect = utils::randomFloat(state) * (reflectance + transmittance) < reflectance;
                bool absorptive = n1[0].imag() > constants::ABSORPTIVE_EXTINCTION;

                Float ratio = !reflect && !absorptive ? n0[0].real() / n1[0].real() : 1;

                Vector<Float, 3> direction = reflect ? utils::reflected(r.getDirection(), normal) : utils::refracted(r.getDirection(), normal, ratio);

                if ((alphaX > constants::SPECULAR_ALPHA && alphaY > constants::SPECULAR_ALPHA) && (reflect ? dot(intersection.transformedNormal, direction) <= constants::GRAZING_COS : dot(intersection.transformedNormal, direction) >= -constants::GRAZING_COS)) {
                    probabilityDensity = SampledSpectrum<Float>(1);
                    radianceScale = 1;

                    return SampledSpectrum<Float>();
                }

                Vector<Float, 3> offset = intersection.geometricNormal * intersection.pointError;

                if (!reflect) offset *= -1;

                Ray rOut(intersection.transformedPoint + offset, direction, r.getLambdas(), reflect ? r.getMedium() : medium1);

                if (!reflect && collapse) collapsed = true;

                radianceScale = reflect ? 1 : ratio * ratio;

                Float weight = (reflectance + transmittance) / (reflect ? reflectance : transmittance);

                probabilityDensity = SampledSpectrum<Float>();

                SampledSpectrum<Float> attenuation;

                if ((alphaX > constants::SPECULAR_ALPHA && alphaY > constants::SPECULAR_ALPHA) && (reflect || std::fabs(n0[0].real() - n1[0].real()) > constants::DISPERSIVE_DELTA)) {
                    attenuation = evaluateFresnel(coating, intersection, r, rOut, probabilityDensity, collapsed);

                    if (!(probabilityDensity[0] > 0)) attenuation = SampledSpectrum<Float>();
                    else attenuation *= std::fabs(dot(intersection.transformedNormal, rOut.getDirection())) / probabilityDensity[0];
                }
                else
                    for (int i = 0; i < (!collapsed ? constants::HERO_COUNT : 1); i++) {
                        if (i > 0) {
                            fresnelReflectanceTransmittance(coating, intersection, n0[i], n1[i], cosTheta, r.getLambdas()[i], reflectance, transmittance);

                            absorptive = n1[i].imag() > constants::ABSORPTIVE_EXTINCTION;

                            ratio = !reflect && !absorptive ? n0[i].real() / n1[i].real() : 1;
                        }

                        if (reflect) {
                            probabilityDensity[i] = reflectance + transmittance > 0 ? reflectance / (reflectance + transmittance) : 0;
                            attenuation[i] = weight * reflectance;
                        }
                        else {
                            probabilityDensity[i] = reflectance + transmittance > 0 ? transmittance / (reflectance + transmittance) : 0;
                            attenuation[i] = weight * transmittance * ratio * ratio;
                        }
                    }

                r = rOut;

                return attenuation;
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

            LAMBDA_DEVICE static Float microfacetLambda(const Vector<Float, 3> & vLocal, Float alphaX, Float alphaY) {
                if (vLocal[2] <= constants::GRAZING_COS) return 0;

                Float x2 = vLocal[0] * vLocal[0] * (alphaX * alphaX);
                Float y2 = vLocal[1] * vLocal[1] * (alphaY * alphaY);
                Float z2 = vLocal[2] * vLocal[2];

                return Float(0.5) * (std::sqrt(1 + (x2 + y2) / z2) - 1);
            }
    };
}