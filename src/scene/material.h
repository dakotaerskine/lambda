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

enum class MaterialType { LAMBERTIAN, METAL, DIELECTRIC, EMISSIVE, THINFILM };

class Material {
    public:
        HOST_DEVICE Material() : type(MaterialType::LAMBERTIAN) {}

        HOST_DEVICE static Material makeLambertian(int a) {
            Material material;

            material.type = MaterialType::LAMBERTIAN;
            material.lambertian.albedo = a;

            return material;
        }

        HOST_DEVICE static Material makeMetal(int a) {
            Material material;

            material.type = MaterialType::METAL;
            material.metal.albedo = a;

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

        HOST_DEVICE Float evaluate(DenseSpectrum<Float> * const spectra, SpectrumTexture * const spectrumTextures, const Intersection & i, const Ray & r) const {
            switch (type) {
                case MaterialType::LAMBERTIAN: return evaluateLambertian(spectra, spectrumTextures, i, r);
                case MaterialType::METAL: return evaluateMetal(spectra, spectrumTextures, i, r);
                case MaterialType::DIELECTRIC: return 1;
                case MaterialType::EMISSIVE: return evaluateEmissive(spectra, spectrumTextures, i, r);
                case MaterialType::THINFILM: return 1;
            }

            return 0;
        }

        HOST_DEVICE Float pdf(const Intersection & i, const Ray & r) const {
            switch (type) {
                case MaterialType::LAMBERTIAN: return pdfLambertian(i, r);
                case MaterialType::METAL: return 1;
                case MaterialType::DIELECTRIC: return 1;
                case MaterialType::EMISSIVE: return 0;
                case MaterialType::THINFILM: return 1;
            }

            return 0;
        }

        HOST_DEVICE Float average(DenseSpectrum<Float> * const spectra, SpectrumTexture * const spectrumTextures) const {
            switch (type) {
                case MaterialType::LAMBERTIAN: return averageLambertian(spectra, spectrumTextures);
                case MaterialType::METAL: return averageMetal(spectra, spectrumTextures);
                case MaterialType::DIELECTRIC: return 1;
                case MaterialType::EMISSIVE: return averageEmissive(spectra, spectrumTextures);
                case MaterialType::THINFILM: return 1;
            }

            return 0;
        }

        HOST_DEVICE Ray scatter(DenseSpectrum<Float> * const spectra, DenseSpectrum<Complex> * const complexSpectra, int * const materialProperties, ScalarTexture * const scalarTextures, const Ray & r, const Intersection & i, Random & state) const {
            switch (type) {
                case MaterialType::LAMBERTIAN: return scatterLambertian(r, i, state);
                case MaterialType::METAL: return scatterMetal(r, i);
                case MaterialType::DIELECTRIC: return scatterDielectric(spectra, r, i, state);
                case MaterialType::EMISSIVE: return Ray();
                case MaterialType::THINFILM: return scatterThinFilm(complexSpectra, materialProperties, scalarTextures, r, i, state);
            }

            return Ray();
        }

    private:
        MaterialType type;

        union {
            struct { int albedo; } lambertian;
            struct { int albedo; } metal;
            struct { int n0, n1; } dielectric;
            struct { int emission; } emissive;
            struct { int numLayers; int n; int d; } thinFilm;
        };

        HOST_DEVICE Float evaluateLambertian(DenseSpectrum<Float> * const spectra, SpectrumTexture * const spectrumTextures, const Intersection & i, const Ray & r) const {
            Float cosTheta = dot(i.normal, r.getDirection());

            if (cosTheta <= 0) return 0;

            return spectrumTextures[lambertian.albedo].evaluate(spectra, i, r.getLambda()) / PI * cosTheta;
        }
        HOST_DEVICE Float evaluateMetal(DenseSpectrum<Float> * const spectra, SpectrumTexture * const spectrumTextures, const Intersection & i, const Ray & r) const { return spectrumTextures[metal.albedo].evaluate(spectra, i, r.getLambda()); }
        HOST_DEVICE Float evaluateEmissive(DenseSpectrum<Float> * const spectra, SpectrumTexture * const spectrumTextures, const Intersection & i, const Ray & r) const { return spectrumTextures[emissive.emission].evaluate(spectra, i, r.getLambda()); }

        HOST_DEVICE Float pdfLambertian(const Intersection & i, const Ray & r) const {
            Float cosTheta = dot(i.normal, r.getDirection());

            return cosTheta > 0 ? cosTheta / PI : 0;
        }

        HOST_DEVICE Float averageLambertian(DenseSpectrum<Float> * const spectra, SpectrumTexture * const spectrumTextures) const { return spectrumTextures[lambertian.albedo].average(spectra); }
        HOST_DEVICE Float averageMetal(DenseSpectrum<Float> * const spectra, SpectrumTexture * const spectrumTextures) const { return spectrumTextures[metal.albedo].average(spectra); }
        HOST_DEVICE Float averageEmissive(DenseSpectrum<Float> * const spectra, SpectrumTexture * const spectrumTextures) const { return spectrumTextures[emissive.emission].average(spectra); }

        HOST_DEVICE Ray scatterLambertian(const Ray & r, const Intersection & i, Random & state) const { return Ray(i.point, randomInHemisphere(i.normal, state), r.getLambda()); }
        HOST_DEVICE Ray scatterMetal(const Ray & r, const Intersection & i) const { return Ray(i.point, reflected(r.getDirection(), i.normal), r.getLambda()); }

        HOST_DEVICE Ray scatterDielectric(DenseSpectrum<Float> * const spectra, const Ray & r, const Intersection & i, Random & state) const {
            Float lambda = r.getLambda();
            Float ratio = spectra[dielectric.n0](lambda) / spectra[dielectric.n1](lambda);
            if (!i.frontFacing) ratio = 1 / ratio;
            Float r0 = (spectra[dielectric.n0](lambda) - spectra[dielectric.n1](lambda)) / (spectra[dielectric.n0](lambda) + spectra[dielectric.n1](lambda));
            r0 *= r0;
            Float reflectance = r0 + (1 - r0) * pow(1 + dot(r.getDirection(), i.normal), 5);
            Vector direction = (randomDouble(state) > reflectance) ? refracted(r.getDirection(), i.normal, ratio) : reflected(r.getDirection(), i.normal);

            return Ray(i.point, direction, r.getLambda());
        }

        HOST_DEVICE Ray scatterThinFilm(DenseSpectrum<Complex> * const complexSpectra, int * const materialProperties, ScalarTexture * const scalarTextures, const Ray & r, const Intersection & i, Random & state) const {
            Float lambda = r.getLambda();
            Float ratio = (complexSpectra[materialProperties[thinFilm.n]](lambda) / complexSpectra[materialProperties[thinFilm.n + thinFilm.numLayers + 1]](lambda)).real();
            if (!i.frontFacing) ratio = 1 / ratio;
            Float reflectance = thinFilmReflectance(complexSpectra, materialProperties, scalarTextures, r, i);
            Vector direction = reflected(r.getDirection(), i.normal);

            if (randomDouble(state) > reflectance) direction = refracted(r.getDirection(), i.normal, ratio);

            return Ray(i.point, direction, r.getLambda());
        }

        HOST_DEVICE Float thinFilmReflectance(DenseSpectrum<Complex> * const complexSpectra, int * const materialProperties, ScalarTexture * const scalarTextures, const Ray & r, const Intersection & i) const {
            int jOffset = i.frontFacing ? 0 : thinFilm.numLayers + 1;
            int jSign = i.frontFacing ? 1 : -1;

            Float lambda = r.getLambda();
            Complex n0 = complexSpectra[materialProperties[thinFilm.n + jOffset]](lambda);
            Complex n1 = complexSpectra[materialProperties[thinFilm.n + jOffset + jSign]](lambda);

            Complex cosCurrent = dot(i.normal, -r.getDirection());
            Complex sinCurrent;
            Complex sinNext = n0 / n1;
            sinNext *= sinNext * (Complex(1) - cosCurrent * cosCurrent);
            Complex cosNext = squareRoot(Complex(1) - sinNext);

            Matrix matrices[2];
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
                cosNext = squareRoot(Complex(1) - sinNext);

                Matrix interfaceS = interfaceMatrixS(n0, n1, cosCurrent, cosNext);
                Matrix interfaceP = interfaceMatrixP(n0, n1, cosCurrent, cosNext);

                Complex phi = n0 * Complex(2 * PI / r.getLambda() * scalarTextures[materialProperties[thinFilm.d + dIndex]].evaluate(i) * cosCurrent);

                Matrix propagation = propagationMatrix(phi);
                matrices[0] *= propagation * interfaceS;
                matrices[1] *= propagation * interfaceP;
            }

            Float R_s = absoluteValue(matrices[0].get(0, 1) / matrices[0].get(0, 0));
            Float R_p = absoluteValue(matrices[1].get(0, 1) / matrices[1].get(0, 0));

            R_s *= R_s;
            R_p *= R_p;

            return (R_s + R_p) * 0.5;
        }
};