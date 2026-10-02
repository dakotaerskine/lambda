#pragma once

#include <cmath>

#include "core/platform.h"
#include "core/utils.h"
#include "math/complex.h"
#include "math/intersection.h"
#include "math/matrix.h"
#include "math/random.h"
#include "math/ray.h"
#include "math/spectrum.h"
#include "math/vector.h"
#include "scene/texture.h"

namespace lambda {
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

            LAMBDA_HOST_DEVICE bool isSpecular() const {
                switch (type) {
                    case Type::LAMBERTIAN: return false;
                    case Type::MIRROR: return true;
                    case Type::CONDUCTOR: return conductor.alphaX->getMax() < constants::EPSILON && conductor.alphaY->getMax() < constants::EPSILON;
                    case Type::DIELECTRIC: return dielectric.alphaX->getMax() < constants::EPSILON && dielectric.alphaY->getMax() < constants::EPSILON;
                    case Type::COATED: return isSpecularCoated();
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

            LAMBDA_HOST_DEVICE SampledSpectrum emission(const Intersection & i, const SampledSpectrum & lambdas) const {
                switch (type) {
                    case Type::LAMBERTIAN: return SampledSpectrum(0);
                    case Type::MIRROR: return SampledSpectrum(0);
                    case Type::CONDUCTOR: return SampledSpectrum(0);
                    case Type::DIELECTRIC: return SampledSpectrum(0);
                    case Type::COATED: return SampledSpectrum(0);
                    case Type::EMISSIVE: return emissionEmissive(i, lambdas);
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

            LAMBDA_HOST_DEVICE SampledSpectrum pdf(const Intersection & i, const Ray & rIn, const Ray & rOut) const {
                switch (type) {
                    case Type::LAMBERTIAN: return pdfLambertian(i, rOut);
                    case Type::MIRROR: return SampledSpectrum(0);
                    case Type::CONDUCTOR: return pdfConductor(i, rIn, rOut);
                    case Type::DIELECTRIC: return pdfDielectric(i, rIn, rOut);
                    case Type::COATED: return pdfCoated(i, rIn, rOut);
                    case Type::EMISSIVE: return SampledSpectrum(0);
                }

                return SampledSpectrum(0);
            }

            LAMBDA_HOST_DEVICE SampledSpectrum evaluate(const Intersection & i, const Ray & rIn, const Ray & rOut) const {
                switch (type) {
                    case Type::LAMBERTIAN: return evaluateLambertian(i, rOut);
                    case Type::MIRROR: return SampledSpectrum(0);
                    case Type::CONDUCTOR: return evaluateConductor(i, rIn, rOut);
                    case Type::DIELECTRIC: return evaluateDielectric(i, rIn, rOut);
                    case Type::COATED: return evaluateCoated(i, rIn, rOut);
                    case Type::EMISSIVE: return SampledSpectrum(0);
                }

                return SampledSpectrum(0);
            }

            LAMBDA_HOST_DEVICE SampledSpectrum scatter(const Intersection & i, Ray & r, Random & state, bool & collapsed) const {
                switch (type) {
                    case Type::LAMBERTIAN: return scatterLambertian(i, r, state, collapsed);
                    case Type::MIRROR: return scatterMirror(i, r, collapsed);
                    case Type::CONDUCTOR: return scatterConductor(i, r, state, collapsed);
                    case Type::DIELECTRIC: return scatterDielectric(i, r, state, collapsed);
                    case Type::COATED: return scatterCoated(i, r, state, collapsed);
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

            LAMBDA_HOST_DEVICE bool isSpecularCoated() const {
                switch (coated.substrate->type) {
                    case Type::LAMBERTIAN: return false;
                    case Type::MIRROR: return false;
                    case Type::CONDUCTOR: return coated.substrate->conductor.alphaX->getMax() < constants::EPSILON && coated.substrate->conductor.alphaY->getMax() < constants::EPSILON;
                    case Type::DIELECTRIC: return coated.substrate->dielectric.alphaX->getMax() < constants::EPSILON && coated.substrate->dielectric.alphaY->getMax() < constants::EPSILON;
                    case Type::COATED: return false;
                    case Type::EMISSIVE: return false;
                }

                return false;
            }

            LAMBDA_HOST_DEVICE SampledSpectrum emissionEmissive(const Intersection & i, const SampledSpectrum & lambdas) const {
                SampledSpectrum emission;

                for (int j = 0; j < constants::HERO_COUNT; j++) emission[j] = emissive.emission->evaluate(i, lambdas[j]);

                return emission;
            }

            LAMBDA_HOST_DEVICE SampledSpectrum pdfLambertian(const Intersection & i, const Ray & r) const {
                Float cosTheta = dot(i.transformedNormal, r.getDirection());

                return cosTheta > 0 ? SampledSpectrum(cosTheta / constants::PI) : SampledSpectrum(0);
            }

            LAMBDA_HOST_DEVICE SampledSpectrum pdfConductor(const Intersection & i, const Ray & rIn, const Ray & rOut) const {
                Float alphaX = conductor.alphaX->evaluate(i);
                Float alphaY = conductor.alphaY->evaluate(i);

                if (alphaX < constants::EPSILON && alphaY < constants::EPSILON) return 0;

                Vector<Float, 3> vIn = -rIn.getDirection();
                Vector<Float, 3> vOut = rOut.getDirection();

                if (dot(i.transformedNormal, vIn) < constants::EPSILON || dot(i.transformedNormal, vOut) < constants::EPSILON) return 0;

                Vector<Float, 3> bitangent = cross(i.transformedNormal, i.transformedTangent);

                Vector<Float, 3> vInLocal = Vector<Float, 3>(dot(vIn, i.transformedTangent), dot(vIn, bitangent), dot(vIn, i.transformedNormal));
                Vector<Float, 3> vOutLocal = Vector<Float, 3>(dot(vOut, i.transformedTangent), dot(vOut, bitangent), dot(vOut, i.transformedNormal));

                Vector<Float, 3> halfVLocal = vInLocal + vOutLocal;

                Float lengthSquared = halfVLocal.lengthSquared();

                if (lengthSquared < constants::EPSILON_SQUARED) return 0;

                halfVLocal /= std::sqrt(lengthSquared);

                if (halfVLocal[2] < constants::EPSILON) halfVLocal *= -1;

                Float D = utils::ggxD(halfVLocal, alphaX, alphaY);
                Float G1 = utils::ggxG1(vInLocal, alphaX, alphaY);

                return SampledSpectrum(D * G1 / (4 * std::fabs(vInLocal[2])));
            }

            LAMBDA_HOST_DEVICE SampledSpectrum pdfDielectric(const Intersection & i, const Ray & rIn, const Ray & rOut, const Material * coating = nullptr) const {
                Float alphaX = dielectric.alphaX->evaluate(i);
                Float alphaY = dielectric.alphaY->evaluate(i);

                if (alphaX < constants::EPSILON && alphaY < constants::EPSILON) return SampledSpectrum(0);

                Vector<Float, 3> vIn = -rIn.getDirection();
                Vector<Float, 3> vOut = rOut.getDirection();

                if (dot(i.transformedNormal, vIn) < constants::EPSILON) return SampledSpectrum(0);

                bool reflect = dot(i.transformedNormal, vOut) > constants::EPSILON;

                Vector<Float, 3> bitangent = cross(i.transformedNormal, i.transformedTangent);

                Vector<Float, 3> vInLocal = Vector<Float, 3>(dot(vIn, i.transformedTangent), dot(vIn, bitangent), dot(vIn, i.transformedNormal));
                Vector<Float, 3> vOutLocal = Vector<Float, 3>(dot(vOut, i.transformedTangent), dot(vOut, bitangent), dot(vOut, i.transformedNormal));

                Float G1 = utils::ggxG1(vInLocal, alphaX, alphaY);

                SampledSpectrum probability;
                Float reflectance, transmittance;

                for (int j = 0; j < constants::HERO_COUNT; j++) {
                    Float lambda = rIn.getLambdas()[j];

                    Float n0 = i.frontFacing ? dielectric.n0->get(lambda) : dielectric.n1->get(lambda);
                    Float n1 = i.frontFacing ? dielectric.n1->get(lambda) : dielectric.n0->get(lambda);

                    Float ratio = n0 / n1;

                    if (!reflect && std::fabs(ratio - 1) < constants::EPSILON) probability[j] = 0;
                    else {
                        Vector<Float, 3> halfVLocal = reflect ? vInLocal + vOutLocal : -(vInLocal * ratio + vOutLocal);

                        Float lengthSquared = halfVLocal.lengthSquared();

                        if (lengthSquared < constants::EPSILON_SQUARED) probability[j] = 0;
                        else {
                            halfVLocal /= std::sqrt(lengthSquared);

                            if (halfVLocal[2] < constants::EPSILON) halfVLocal *= -1;

                            Float D = utils::ggxD(halfVLocal, alphaX, alphaY);

                            Float cosTheta = dot(halfVLocal, vInLocal);

                            if (cosTheta < constants::EPSILON) probability[j] = 0;
                            else {
                                if (coating) coating->thinFilmFresnel(n0, n1, cosTheta, i, rIn.getLambdas()[j], reflectance, transmittance);
                                else fresnel(n0, n1, cosTheta, reflectance, transmittance);

                                if (reflect) probability[j] = reflectance * D * G1 / (4 * std::fabs(vInLocal[2]));
                                else {
                                    Float denominator = ratio * dot(vInLocal, halfVLocal) + dot(vOutLocal, halfVLocal);

                                    probability[j] = transmittance * D * G1 * cosTheta * std::fabs(dot(vOutLocal, halfVLocal)) / (denominator * denominator * std::fabs(vInLocal[2]));
                                }
                            }
                        }
                    }
                }

                return probability;
            }

            LAMBDA_HOST_DEVICE SampledSpectrum pdfCoated(const Intersection & i, const Ray & rIn, const Ray & rOut) const {
                switch (coated.substrate->type) {
                    case Type::LAMBERTIAN: return SampledSpectrum(0);
                    case Type::MIRROR: return SampledSpectrum(0);
                    case Type::CONDUCTOR: return coated.substrate->pdfConductor(i, rIn, rOut);
                    case Type::DIELECTRIC: return coated.substrate->pdfDielectric(i, rIn, rOut, this);
                    case Type::COATED: return SampledSpectrum(0);
                    case Type::EMISSIVE: return SampledSpectrum(0);
                }

                return SampledSpectrum(0);
            }

            LAMBDA_HOST_DEVICE SampledSpectrum evaluateLambertian(const Intersection & i, const Ray & r, bool collapsed = false) const {
                if (dot(i.transformedNormal, r.getDirection()) < constants::EPSILON) return SampledSpectrum(0);

                SampledSpectrum attenuation;

                for (int j = 0; j < constants::HERO_COUNT; j++) attenuation[j] = !collapsed || j == 0 ? lambertian.albedo->evaluate(i, r.getLambdas()[j]) / constants::PI : 0;

                return attenuation;
            }

            LAMBDA_HOST_DEVICE SampledSpectrum evaluateConductor(const Intersection & i, const Ray & rIn, const Ray & rOut, bool collapsed = false, const Material * coating = nullptr) const {
                Float alphaX = conductor.alphaX->evaluate(i);
                Float alphaY = conductor.alphaY->evaluate(i);

                if (alphaX < constants::EPSILON && alphaY < constants::EPSILON) return SampledSpectrum(0);

                Vector<Float, 3> vIn = -rIn.getDirection();
                Vector<Float, 3> vOut = rOut.getDirection();

                if (dot(i.transformedNormal, vIn) < constants::EPSILON || dot(i.transformedNormal, vOut) < constants::EPSILON) return SampledSpectrum(0);

                Vector<Float, 3> bitangent = cross(i.transformedNormal, i.transformedTangent);

                Vector<Float, 3> vInLocal = Vector<Float, 3>(dot(vIn, i.transformedTangent), dot(vIn, bitangent), dot(vIn, i.transformedNormal));
                Vector<Float, 3> vOutLocal = Vector<Float, 3>(dot(vOut, i.transformedTangent), dot(vOut, bitangent), dot(vOut, i.transformedNormal));

                Vector<Float, 3> halfVLocal = vInLocal + vOutLocal;

                Float lengthSquared = halfVLocal.lengthSquared();

                if (lengthSquared < constants::EPSILON_SQUARED) return SampledSpectrum(0);

                halfVLocal /= std::sqrt(lengthSquared);

                if (halfVLocal[2] < constants::EPSILON) halfVLocal *= -1;

                Float cosTheta = dot(vInLocal, halfVLocal);

                SampledSpectrum attenuation;
                Float transmittance;

                for (int j = 0; j < constants::HERO_COUNT; j++)
                    if (!collapsed || j == 0) {
                        Float lambda = rIn.getLambdas()[j];

                        Float n0 = conductor.n0->get(lambda);
                        Complex n1 = conductor.n1->get(lambda);

                        if (coating) coating->thinFilmFresnel(n0, n1, cosTheta, i, lambda, attenuation[j], transmittance);
                        else fresnel(n0, n1, cosTheta, attenuation[j], transmittance);
                    }
                    else attenuation[j] = 0;

                Float D = utils::ggxD(halfVLocal, alphaX, alphaY);
                Float G2 = utils::ggxG2(vInLocal, vOutLocal, alphaX, alphaY);

                return attenuation * (D * G2 / (4 * vInLocal[2] * vOutLocal[2]));
            }

            LAMBDA_HOST_DEVICE SampledSpectrum evaluateDielectric(const Intersection & i, const Ray & rIn, const Ray & rOut, bool collapsed = false, const Material * coating = nullptr) const {
                Float alphaX = dielectric.alphaX->evaluate(i);
                Float alphaY = dielectric.alphaY->evaluate(i);

                if (alphaX < constants::EPSILON && alphaY < constants::EPSILON) return SampledSpectrum(0);

                Vector<Float, 3> vIn = -rIn.getDirection();
                Vector<Float, 3> vOut = rOut.getDirection();

                bool reflect = dot(i.transformedNormal, vOut) > constants::EPSILON;

                Vector<Float, 3> bitangent = cross(i.transformedNormal, i.transformedTangent);

                Vector<Float, 3> vInLocal = Vector<Float, 3>(dot(vIn, i.transformedTangent), dot(vIn, bitangent), dot(vIn, i.transformedNormal));
                Vector<Float, 3> vOutLocal = Vector<Float, 3>(dot(vOut, i.transformedTangent), dot(vOut, bitangent), dot(vOut, i.transformedNormal));

                SampledSpectrum attenuation;
                Float reflectance, transmittance;

                for (int j = 0; j < constants::HERO_COUNT; j++)
                    if (!collapsed || j == 0) {
                        Float lambda = rIn.getLambdas()[j];

                        Float n0 = i.frontFacing ? dielectric.n0->get(lambda) : dielectric.n1->get(lambda);
                        Float n1 = i.frontFacing ? dielectric.n1->get(lambda) : dielectric.n0->get(lambda);

                        Float ratio = n0 / n1;

                        if (!reflect && std::fabs(ratio - 1) < constants::EPSILON) attenuation[j] = 0;
                        else {
                            Vector<Float, 3> halfVLocal = reflect ? vInLocal + vOutLocal : -(vInLocal * ratio + vOutLocal);

                            Float lengthSquared = halfVLocal.lengthSquared();

                            if (lengthSquared < constants::EPSILON_SQUARED) attenuation[j] = 0;
                            else {
                                halfVLocal /= std::sqrt(lengthSquared);

                                if (halfVLocal[2] < constants::EPSILON) halfVLocal *= -1;

                                Float cosTheta = dot(halfVLocal, vInLocal);

                                if (cosTheta < constants::EPSILON) attenuation[j] = 0;
                                else {
                                    if (coating) coating->thinFilmFresnel(n0, n1, cosTheta, i, lambda, reflectance, transmittance);
                                    else fresnel(n0, n1, cosTheta, reflectance, transmittance);

                                    if (reflect) attenuation[j] = reflectance;
                                    else attenuation[j] = transmittance;

                                    Float D = utils::ggxD(halfVLocal, alphaX, alphaY);

                                    Vector<Float, 3> vOutLocalShadow(vOutLocal[0], vOutLocal[1], std::fabs(vOutLocal[2]));

                                    Float G2 = utils::ggxG2(vInLocal, vOutLocalShadow, alphaX, alphaY);

                                    if (reflect) attenuation[j] *= (D * G2 / (4 * vInLocal[2] * vOutLocalShadow[2]));
                                    else {
                                        Float denominator = ratio * dot(vInLocal, halfVLocal) + dot(vOutLocal, halfVLocal);

                                        attenuation[j] *= D * G2 * cosTheta * std::fabs(dot(vOutLocal, halfVLocal)) / (denominator * denominator * std::fabs(vInLocal[2]) * std::fabs(vOutLocal[2]));
                                    }
                                }
                            }
                        }
                    }

                return attenuation;
            }

            LAMBDA_HOST_DEVICE SampledSpectrum evaluateCoated(const Intersection & i, const Ray & rIn, const Ray & rOut, bool collapsed = false) const {
                switch (coated.substrate->type) {
                    case Type::LAMBERTIAN: return SampledSpectrum(0);
                    case Type::MIRROR: return SampledSpectrum(0);
                    case Type::CONDUCTOR: return coated.substrate->evaluateConductor(i, rIn, rOut, collapsed, this);
                    case Type::DIELECTRIC: return coated.substrate->evaluateDielectric(i, rIn, rOut, collapsed, this);
                    case Type::COATED: return SampledSpectrum(0);
                    case Type::EMISSIVE: return SampledSpectrum(0);
                }

                return SampledSpectrum(0);
            }

            LAMBDA_HOST_DEVICE SampledSpectrum scatterLambertian(const Intersection & i, Ray & r, Random & state, bool collapsed) const {
                r = Ray(i.transformedPoint, utils::randomInHemisphere(i.transformedNormal, state), r.getLambdas(), r.getMedium());

                return evaluateLambertian(i, r, collapsed);
            }

            LAMBDA_HOST_DEVICE SampledSpectrum scatterMirror(const Intersection & i, Ray & r, bool collapsed) const {
                r = Ray(i.transformedPoint, utils::reflected(r.getDirection(), i.transformedNormal), r.getLambdas(), r.getMedium());

                SampledSpectrum attenuation;

                for (int j = 0; j < constants::HERO_COUNT; j++) attenuation[j] = !collapsed || j == 0 ? mirror.albedo->evaluate(i, r.getLambdas()[j]) : 0;

                return attenuation;
            }

            LAMBDA_HOST_DEVICE SampledSpectrum scatterConductor(const Intersection & i, Ray & r, Random & state, bool collapsed, const Material * coating = nullptr) const {
                Vector<Float, 3> normal = i.transformedNormal;

                Float alphaX = conductor.alphaX->evaluate(i);
                Float alphaY = conductor.alphaY->evaluate(i);

                if (alphaX > constants::EPSILON || alphaY > constants::EPSILON) normal = utils::ggxNormal(-r.getDirection(), normal, i.transformedTangent, alphaX, alphaY, state);

                Float cosTheta = dot(normal, -r.getDirection());

                SampledSpectrum reflectance, transmittance;

                for (int j = 0; j < constants::HERO_COUNT; j++) {
                    Float lambda = r.getLambdas()[j];

                    Float n0 = conductor.n0->get(lambda);
                    Complex n1 = conductor.n1->get(lambda);

                    if (coating) coating->thinFilmFresnel(n0, n1, cosTheta, i, lambda, reflectance[j], transmittance[j]);
                    else fresnel(n0, n1, cosTheta, reflectance[j], transmittance[j]);
                }

                bool reflect = utils::randomFloat(state) < reflectance[0] || reflectance[0] >= 1;

                if (!reflect) return SampledSpectrum(0);

                Vector<Float, 3> direction = utils::reflected(r.getDirection(), normal);

                if (dot(i.transformedNormal, direction) < constants::EPSILON) return SampledSpectrum(0);

                Ray rOut(i.transformedPoint, direction, r.getLambdas(), r.getMedium());

                Float averageReflectance = collapsed ? reflectance[0] : reflectance.average();

                SampledSpectrum attenuation;

                if ((alphaX > constants::EPSILON || alphaY > constants::EPSILON)) attenuation = evaluateConductor(i, r, rOut, collapsed, coating) / averageReflectance;
                else for (int j = 0; j < constants::HERO_COUNT; j++) attenuation[j] = (!collapsed || j == 0) ? reflectance[j] / averageReflectance : 0;

                r = rOut;

                return attenuation;
            }

            LAMBDA_HOST_DEVICE SampledSpectrum scatterDielectric(const Intersection & i, Ray & r, Random & state, bool & collapsed, const Material * coating = nullptr) const {
                Vector<Float, 3> normal = i.transformedNormal;

                Float alphaX = dielectric.alphaX->evaluate(i);
                Float alphaY = dielectric.alphaY->evaluate(i);

                if (alphaX > constants::EPSILON || alphaY > constants::EPSILON) normal = utils::ggxNormal(-r.getDirection(), normal, i.transformedTangent, alphaX, alphaY, state);

                Float lambda = r.getLambdas()[0];

                Float n0Hero = i.frontFacing ? dielectric.n0->get(lambda) : dielectric.n1->get(lambda);
                Float n1Hero = i.frontFacing ? dielectric.n1->get(lambda) : dielectric.n0->get(lambda);

                bool collapse = false;

                SampledSpectrum reflectance, transmittance;

                for (int j = 0; j < constants::HERO_COUNT; j++) {
                    lambda = r.getLambdas()[j];

                    Float n0 = i.frontFacing ? dielectric.n0->get(lambda) : dielectric.n1->get(lambda);
                    Float n1 = i.frontFacing ? dielectric.n1->get(lambda) : dielectric.n0->get(lambda);

                    if (!collapsed && (std::fabs(n0Hero - n0) > constants::EPSILON || std::fabs(n1Hero - n1) > constants::EPSILON)) collapse = true;

                    Float cosTheta = dot(normal, -r.getDirection());

                    if (coating) coating->thinFilmFresnel(n0, n1, cosTheta, i, lambda, reflectance[j], transmittance[j]);
                    else fresnel(n0, n1, cosTheta, reflectance[j], transmittance[j]);
                }

                Float u = utils::randomFloat(state);

                if (u >= reflectance[0] + transmittance[0]) return SampledSpectrum(0);

                bool reflect = u < reflectance[0] || reflectance[0] >= 1;

                Vector<Float, 3> direction = reflect ? utils::reflected(r.getDirection(), normal) : utils::refracted(r.getDirection(), normal, n0Hero / n1Hero);

                if ((alphaX > constants::EPSILON || alphaY > constants::EPSILON) && (reflect ? dot(i.transformedNormal, direction) < constants::EPSILON : dot(i.transformedNormal, direction) > -constants::EPSILON)) return SampledSpectrum(0);

                Ray rOut(i.transformedPoint, direction, r.getLambdas(), r.getMedium());

                SampledSpectrum attenuation;

                if ((alphaX > constants::EPSILON || alphaY > constants::EPSILON) && (reflect || std::fabs(n0Hero - n1Hero) > constants::EPSILON)) {
                    if (!reflect && collapse) collapsed = true;

                    attenuation = evaluateDielectric(i, r, rOut, collapsed, coating);

                    if (!reflect && collapse) attenuation[0] *= constants::HERO_COUNT;
                }
                else {
                    Float averageReflectance = collapsed || (!reflect && collapse) ? reflectance[0] : reflectance.average();
                    Float averageTransmittance = collapsed || (!reflect && collapse) ? transmittance[0] : transmittance.average();

                    attenuation = reflect ? reflectance / averageReflectance : transmittance / averageTransmittance;

                    if (!reflect && collapse) {
                        attenuation[0] *= constants::HERO_COUNT;

                        collapsed = true;
                    }

                    if (collapsed) for (int j = 1; j < constants::HERO_COUNT; j++) attenuation[j] = 0;
                }

                r = rOut;

                return attenuation;
            }

            LAMBDA_HOST_DEVICE SampledSpectrum scatterCoated(const Intersection & i, Ray & r, Random & state, bool & collapsed) const {
                switch (coated.substrate->type) {
                    case Type::LAMBERTIAN: return SampledSpectrum(0);
                    case Type::MIRROR: return SampledSpectrum(0);
                    case Type::CONDUCTOR: return coated.substrate->scatterConductor(i, r, state, collapsed, this);
                    case Type::DIELECTRIC: return coated.substrate->scatterDielectric(i, r, state, collapsed, this);
                    case Type::COATED: return SampledSpectrum(0);
                    case Type::EMISSIVE: return SampledSpectrum(0);
                }

                return SampledSpectrum(0);
            }

            template <typename T>
            LAMBDA_HOST_DEVICE void fresnel(Float nI, const T & nT, Float cosThetaI, Float & reflectance, Float & transmittance) const {
                using std::abs;
                using std::fabs;
                using std::sqrt;

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

            template <typename T>
            LAMBDA_HOST_DEVICE void thinFilmFresnel(Float nI, const T & nT, const Complex & cosThetaI, const Intersection & i, Float lambda, Float & reflectance, Float & transmittance) const {
                int jOffset = i.frontFacing ? 0 : coated.numLayers - 1;
                int jSign = i.frontFacing ? 1 : -1;

                Complex n0 = nI;
                Complex n1 = coated.n[jOffset]->get(lambda);

                Complex cosCurrent = cosThetaI;
                Complex sinCurrent;
                Complex sinNext = n0 / n1;
                sinNext *= sinNext * (1 - cosCurrent * cosCurrent);
                Complex cosNext = sqrt(1 - sinNext);

                Matrix<Complex, 2> matrices[2];
                matrices[0] = utils::interfaceMatrixS(n0, n1, cosCurrent, cosNext);
                matrices[1] = utils::interfaceMatrixP(n0, n1, cosCurrent, cosNext);

                for (int j = 0; j < coated.numLayers; j++) {
                    n0 = coated.n[jOffset + jSign * j]->get(lambda);
                    n1 = j == coated.numLayers - 1 ? nT : coated.n[jOffset + jSign * (j + 1)]->get(lambda);

                    cosCurrent = cosNext;
                    sinCurrent = sinNext;
                    sinNext = n0 / n1;
                    sinNext *= sinNext * sinCurrent;
                    cosNext = sqrt(1 - sinNext);

                    Matrix<Complex, 2> interfaceS = utils::interfaceMatrixS(n0, n1, cosCurrent, cosNext);
                    Matrix<Complex, 2> interfaceP = utils::interfaceMatrixP(n0, n1, cosCurrent, cosNext);

                    Complex phi = n0 * 2 * constants::PI / lambda * coated.d[jOffset + jSign * j]->evaluate(i) * cosCurrent;

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
    };
}