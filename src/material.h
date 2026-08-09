#pragma once

#include <cmath>

#include "intersection.h"
#include "matrix.h"
#include "platform.h"
#include "random.h"
#include "ray.h"
#include "spectrum.h"
#include "texture.h"
#include "utils.h"
#include "vector.h"

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

        HOST_DEVICE bool scatter(DenseSpectrum<Float> * const spectra, DenseSpectrum<Complex> * const complexSpectra, int * const materialProperties, ScalarTexture * const scalarTextures, SpectrumTexture * const spectrumTextures, const Ray & r, const Intersection & i, Ray & scattered, Float & attenuation, Random & state) const {
            switch (type) {
                case MaterialType::LAMBERTIAN: return scatterLambertian(spectra, spectrumTextures, r, i, scattered, attenuation, state);
                case MaterialType::METAL: return scatterMetal(spectra, spectrumTextures, r, i, scattered, attenuation);
                case MaterialType::DIELECTRIC: return scatterDielectric(spectra, r, i, scattered, attenuation, state);
                case MaterialType::EMISSIVE: return scatterEmissive(spectra, spectrumTextures, r, i, attenuation);
                case MaterialType::THINFILM: return scatterThinFilm(complexSpectra, materialProperties, scalarTextures, r, i, scattered, attenuation, state);
            }

            return false;
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

        HOST_DEVICE bool scatterLambertian(DenseSpectrum<Float> * const spectra, SpectrumTexture * const spectrumTextures, const Ray & r, const Intersection & i, Ray & scattered, Float & attenuation, Random & state) const {
            scattered = Ray(i.point, randomInHemisphere(i.normal, state), r.getLambda());
            attenuation = spectrumTextures[lambertian.albedo].evaluate(spectra, i, r.getLambda());

            return true;
        }

        HOST_DEVICE bool scatterMetal(DenseSpectrum<Float> * const spectra, SpectrumTexture * const spectrumTextures, const Ray & r, const Intersection & i, Ray & scattered, Float & attenuation) const {
            scattered = Ray(i.point, reflected(r.getDirection(), i.normal), r.getLambda());
            attenuation = spectrumTextures[metal.albedo].evaluate(spectra, i, r.getLambda());

            return true;
        }

        HOST_DEVICE bool scatterDielectric(DenseSpectrum<Float> * const spectra, const Ray & r, const Intersection & i, Ray & scattered, Float & attenuation, Random & state) const {
            Float lambda = r.getLambda();
            Float ratio = spectra[dielectric.n0](lambda) / spectra[dielectric.n1](lambda);
            if (!i.frontFacing) ratio = 1 / ratio;
            Float r0 = (spectra[dielectric.n0](lambda) - spectra[dielectric.n1](lambda)) / (spectra[dielectric.n0](lambda) + spectra[dielectric.n1](lambda));
            r0 *= r0;
            Float reflectance = r0 + (1 - r0) * pow(1 + r.getDirection().dot(i.normal), 5);
            Vector direction = (randomDouble(state) > reflectance) ? refracted(r.getDirection(), i.normal, ratio) : reflected(r.getDirection(), i.normal);

            scattered = Ray(i.point, direction, r.getLambda());
            attenuation = 1;

            return true;
        }

        HOST_DEVICE bool scatterEmissive(DenseSpectrum<Float> * const spectra, SpectrumTexture * const spectrumTextures, const Ray & r, const Intersection & i, Float & attenuation) const {
            attenuation = spectrumTextures[emissive.emission].evaluate(spectra, i, r.getLambda());

            return false;
        }

        HOST_DEVICE bool scatterThinFilm(DenseSpectrum<Complex> * const complexSpectra, int * const materialProperties, ScalarTexture * const scalarTextures, const Ray & r, const Intersection & i, Ray & scattered, Float & attenuation, Random & state) const {
            Float lambda = r.getLambda();
            Float ratio = (complexSpectra[materialProperties[thinFilm.n]](lambda) / complexSpectra[materialProperties[thinFilm.n + thinFilm.numLayers + 1]](lambda)).real();
            if (!i.frontFacing) ratio = 1 / ratio;
            Float reflectance = thinFilmReflectance(complexSpectra, materialProperties, scalarTextures, r, i);
            Vector direction = reflected(r.getDirection(), i.normal);
            attenuation = 1;

            if (randomDouble(state) > reflectance) direction = refracted(r.getDirection(), i.normal, ratio);

            scattered = Ray(i.point, direction, r.getLambda());

            return true;
        }

        HOST_DEVICE Float thinFilmReflectance(DenseSpectrum<Complex> * const complexSpectra, int * const materialProperties, ScalarTexture * const scalarTextures, const Ray & r, const Intersection & i) const {
            int jOffset = i.frontFacing ? 0 : thinFilm.numLayers + 1;
            int jSign = i.frontFacing ? 1 : -1;

            Float lambda = r.getLambda();
            Complex n0 = complexSpectra[materialProperties[thinFilm.n + jOffset]](lambda);
            Complex n1 = complexSpectra[materialProperties[thinFilm.n + jOffset + jSign]](lambda);

            Complex cosCurrent = i.normal.dot(-r.getDirection());
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