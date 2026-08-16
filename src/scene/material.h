#pragma once

#include <cmath>

#include "core/platform.h"
#include "core/utils.h"
#include "math/intersection.h"
#include "math/matrix.h"
#include "math/random.h"
#include "math/ray.h"
#include "math/spectrum.h"
#include "math/vector.h"
#include "scene/texture.h"

enum class MaterialType { LAMBERTIAN, MIRROR, DIELECTRIC, EMISSIVE, THINFILM };

class Material {
    public:
        HOST_DEVICE Material() : type(MaterialType::LAMBERTIAN) {}

        HOST_DEVICE static Material makeLambertian(int a) {
            Material material;

            material.type = MaterialType::LAMBERTIAN;
            material.lambertian.albedo = a;

            return material;
        }

        HOST_DEVICE static Material makeMirror(int a) {
            Material material;

            material.type = MaterialType::MIRROR;
            material.mirror.albedo = a;

            return material;
        }

        HOST_DEVICE static Material makeDielectric(int n0, int n1) {
            Material material;

            material.type = MaterialType::DIELECTRIC;
            material.dielectric.n0 = n0;
            material.dielectric.n1 = n1;

            return material;
        }

        HOST_DEVICE static Material makeEmissive(int e) {
            Material material;

            material.type = MaterialType::EMISSIVE;
            material.emissive.emission = e;

            return material;
        }

        HOST_DEVICE static Material makeThinFilm(int numLayers, int n, int d) {
            Material material;

            material.type = MaterialType::THINFILM;
            material.thinFilm.numLayers = numLayers;
            material.thinFilm.n = n;
            material.thinFilm.d = d;

            return material;
        }

        HOST_DEVICE bool isSpecular() const {
            switch (type) {
                case MaterialType::LAMBERTIAN: return false;
                default: return true;
            }
        }

        HOST_DEVICE bool isEmissive() const {
            switch (type) {
                case MaterialType::EMISSIVE: return true;
                default: return false;
            }
        }

        HOST_DEVICE SampledSpectrum emission(DenseSpectrum<Float> * const spectra, ScalarTexture * const scalarTextures, SpectrumTexture * const spectrumTextures, const Intersection & i, const SampledSpectrum & lambdas) const {
            switch (type) {
                case MaterialType::EMISSIVE: return emissionEmissive(spectra, scalarTextures, spectrumTextures, i, lambdas);
                default: return SampledSpectrum(0);
            }
        }

        HOST_DEVICE Float averageEmission(DenseSpectrum<Float> * const spectra, ScalarTexture * const scalarTextures, SpectrumTexture * const spectrumTextures) const {
            switch (type) {
                case MaterialType::EMISSIVE: return averageEmissionEmissive(spectra, scalarTextures, spectrumTextures);
                default: return 0;
            }

            return 0;
        }

        HOST_DEVICE Float pdf(const Intersection & i, const Ray & r) const {
            switch (type) {
                case MaterialType::LAMBERTIAN: return pdfLambertian(i, r);
                default: return 0;
            }

            return 0;
        }

        HOST_DEVICE SampledSpectrum evaluate(DenseSpectrum<Float> * const spectra, ScalarTexture * const scalarTextures, SpectrumTexture * const spectrumTextures, const Intersection & i, const Ray & r) const {
            switch (type) {
                case MaterialType::LAMBERTIAN: return evaluateLambertian(spectra, scalarTextures, spectrumTextures, i, r);
                default: return SampledSpectrum(0);
            }

            return SampledSpectrum(0);
        }

        HOST_DEVICE SampledSpectrum scatter(DenseSpectrum<Float> * const spectra, DenseSpectrum<Complex> * const complexSpectra, int * const materialProperties, ScalarTexture * const scalarTextures, SpectrumTexture * const spectrumTextures, const Intersection & i, Ray & r, Random & state) const {
            switch (type) {
                case MaterialType::LAMBERTIAN: return scatterLambertian(spectra, scalarTextures, spectrumTextures, i, r, state);
                case MaterialType::MIRROR: return scatterMirror(spectra, scalarTextures, spectrumTextures, i, r);
                case MaterialType::DIELECTRIC: return scatterDielectric(spectra, i, r, state);
                case MaterialType::THINFILM: return scatterThinFilm(complexSpectra, materialProperties, scalarTextures, i, r, state);
                default: return SampledSpectrum(0);
            }

            return SampledSpectrum(0);
        }

    private:
        MaterialType type;

        union {
            struct { int albedo; } lambertian;
            struct { int albedo; } mirror;
            struct { int n0, n1; } dielectric;
            struct { int emission; } emissive;
            struct { int numLayers; int n; int d; } thinFilm;
        };

        HOST_DEVICE SampledSpectrum emissionEmissive(DenseSpectrum<Float> * const spectra, ScalarTexture * const scalarTextures, SpectrumTexture * const spectrumTextures, const Intersection & i, const SampledSpectrum & lambdas) const {
            SampledSpectrum emission;

            for (int j = 0; j < HERO_COUNT; j++)
                emission[j] = spectrumTextures[emissive.emission].evaluate(spectra, scalarTextures, i, lambdas[j]);

            return emission;
        }

        HOST_DEVICE Float averageEmissionEmissive(DenseSpectrum<Float> * const spectra, ScalarTexture * const scalarTextures, SpectrumTexture * const spectrumTextures) const { return spectrumTextures[emissive.emission].average(spectra, scalarTextures); }

        HOST_DEVICE Float pdfLambertian(const Intersection & i, const Ray & r) const {
            Float cosTheta = dot(i.normal, r.getDirection());

            return cosTheta > 0 ? cosTheta / PI : 0;
        }

        HOST_DEVICE SampledSpectrum evaluateLambertian(DenseSpectrum<Float> * const spectra, ScalarTexture * const scalarTextures, SpectrumTexture * const spectrumTextures, const Intersection & i, const Ray & r) const {
            if (dot(i.normal, r.getDirection()) <= 0) return SampledSpectrum(0);

            SampledSpectrum attenuation;

            for (int j = 0; j < HERO_COUNT; j++)
                attenuation[j] = spectrumTextures[lambertian.albedo].evaluate(spectra, scalarTextures, i, r.getLambdas()[j]) / PI;

            return attenuation;
        }

        HOST_DEVICE SampledSpectrum scatterLambertian(DenseSpectrum<Float> * const spectra, ScalarTexture * const scalarTextures, SpectrumTexture * const spectrumTextures, const Intersection & i, Ray & r, Random & state) const {
            r = Ray(i.point, randomInHemisphere(i.normal, state), r.getLambdas());

            return evaluateLambertian(spectra, scalarTextures, spectrumTextures, i, r);
        }

        HOST_DEVICE SampledSpectrum scatterMirror(DenseSpectrum<Float> * const spectra, ScalarTexture * const scalarTextures, SpectrumTexture * const spectrumTextures, const Intersection & i, Ray & r) const {
            r = Ray(i.point, reflected(r.getDirection(), i.normal), r.getLambdas());

            SampledSpectrum attenuation;

            for (int j = 0; j < HERO_COUNT; j++)
                attenuation[j] = spectrumTextures[mirror.albedo].evaluate(spectra, scalarTextures, i, r.getLambdas()[j]);

            return attenuation;
        }

        HOST_DEVICE SampledSpectrum scatterDielectric(DenseSpectrum<Float> * const spectra, const Intersection & i, Ray & r, Random & state) const {
            Float factor = powF(1 + dot(r.getDirection(), i.normal), 5);

            Float lambda = r.getLambdas()[0];

            Float n0Hero = spectra[dielectric.n0](lambda);
            Float n1Hero = spectra[dielectric.n1](lambda);

            Float ratio = i.frontFacing ? n0Hero / n1Hero : n1Hero / n0Hero;

            Float r0 = (n0Hero - n1Hero) / (n0Hero + n1Hero);
            r0 *= r0;

            Float reflectanceHero = r0 + (1 - r0) * factor;

            bool reflect = randomFloat(state) < reflectanceHero;

            Vector direction = reflect ? reflected(r.getDirection(), i.normal) : refracted(r.getDirection(), i.normal, ratio);

            SampledSpectrum attenuation;
            attenuation[0] = 1;

            for (int j = 1; j < HERO_COUNT; j++) {
                lambda = r.getLambdas()[j];

                Float n0 = spectra[dielectric.n0](lambda);
                Float n1 = spectra[dielectric.n1](lambda);

                ratio = i.frontFacing ? n0 / n1 : n1 / n0;

                r0 = (n0 - n1) / (n0 + n1);
                r0 *= r0;

                Float reflectance = r0 + (1 - r0) * factor;

                attenuation[j] = reflect ? reflectance / reflectanceHero : (fabsF(n0 - n0Hero) < EPSILON && fabsF(n1 - n1Hero) < EPSILON) ? (1 - reflectance) / (1 - reflectanceHero) : 0;
            }

            r = Ray(i.point, direction, r.getLambdas());

            return attenuation;
        }

        HOST_DEVICE SampledSpectrum scatterThinFilm(DenseSpectrum<Complex> * const complexSpectra, int * const materialProperties, ScalarTexture * const scalarTextures, const Intersection & i, Ray & r, Random & state) const {
            Float lambda = r.getLambdas()[0];

            Float n0Hero = Float(complexSpectra[materialProperties[thinFilm.n]](lambda).real());
            Float n1Hero = Float(complexSpectra[materialProperties[thinFilm.n + thinFilm.numLayers + 1]](lambda).real());

            Float ratio = i.frontFacing ? n0Hero / n1Hero : n1Hero / n0Hero;

            Float reflectanceHero = thinFilmReflectance(complexSpectra, materialProperties, scalarTextures, i, r, lambda);

            bool reflect = randomFloat(state) < reflectanceHero;

            Vector direction = reflect ? reflected(r.getDirection(), i.normal) : refracted(r.getDirection(), i.normal, ratio);

            SampledSpectrum attenuation;
            attenuation[0] = 1;

            for (int j = 1; j < HERO_COUNT; j++) {
                lambda = r.getLambdas()[j];

                Float n0 = Float(complexSpectra[materialProperties[thinFilm.n]](lambda).real());
                Float n1 = Float(complexSpectra[materialProperties[thinFilm.n + thinFilm.numLayers + 1]](lambda).real());

                ratio = i.frontFacing ? n0 / n1 : n1 / n0;

                Float reflectance = thinFilmReflectance(complexSpectra, materialProperties, scalarTextures, i, r, lambda);

                attenuation[j] = reflect ? reflectance / reflectanceHero : (fabsF(n0 - n0Hero) < EPSILON && fabsF(n1 - n1Hero) < EPSILON) ? (1 - reflectance) / (1 - reflectanceHero) : 0;
            }

            r = Ray(i.point, direction, r.getLambdas());

            return attenuation;
        }

        HOST_DEVICE Float thinFilmReflectance(DenseSpectrum<Complex> * const complexSpectra, int * const materialProperties, ScalarTexture * const scalarTextures, const Intersection & i, const Ray & r, Float lambda) const {
            int jOffset = i.frontFacing ? 0 : thinFilm.numLayers + 1;
            int jSign = i.frontFacing ? 1 : -1;

            Complex n0 = complexSpectra[materialProperties[thinFilm.n + jOffset]](lambda);
            Complex n1 = complexSpectra[materialProperties[thinFilm.n + jOffset + jSign]](lambda);

            Complex cosCurrent = dot(i.normal, -r.getDirection());
            Complex sinCurrent;
            Complex sinNext = n0 / n1;
            sinNext *= sinNext * (Complex(1) - cosCurrent * cosCurrent);
            Complex cosNext = sqrtC(Complex(1) - sinNext);

            Matrix2<Complex> matrices[2];
            matrices[0] = interfaceMatrixS(n0, n1, cosCurrent, cosNext);
            matrices[1] = interfaceMatrixP(n0, n1, cosCurrent, cosNext);

            for (int j = 0; j < thinFilm.numLayers; j++) {
                int dIndex = i.frontFacing ? j : thinFilm.numLayers - 1 - j;

                n0 = complexSpectra[materialProperties[thinFilm.n + jOffset + jSign * (j + 1)]](lambda);
                n1 = complexSpectra[materialProperties[thinFilm.n + jOffset + jSign * (j + 2)]](lambda);

                cosCurrent = cosNext;
                sinCurrent = sinNext;
                sinNext = n0 / n1;
                sinNext *= sinNext * sinCurrent;
                cosNext = sqrtC(Complex(1) - sinNext);

                Matrix2<Complex> interfaceS = interfaceMatrixS(n0, n1, cosCurrent, cosNext);
                Matrix2<Complex> interfaceP = interfaceMatrixP(n0, n1, cosCurrent, cosNext);

                Complex phi = n0 * Complex(2 * PI / lambda) * double(scalarTextures[materialProperties[thinFilm.d + dIndex]].evaluate(i)) * cosCurrent;

                Matrix2<Complex> propagation = propagationMatrix(phi);
                matrices[0] *= propagation * interfaceS;
                matrices[1] *= propagation * interfaceP;
            }

            Float R_s = absC(matrices[0].get(0, 1) / matrices[0].get(0, 0));
            Float R_p = absC(matrices[1].get(0, 1) / matrices[1].get(0, 0));

            R_s *= R_s;
            R_p *= R_p;

            return (R_s + R_p) * Float(0.5);
        }
};