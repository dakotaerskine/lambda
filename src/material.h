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

        HOST_DEVICE Material(const Material & m) {
            type = m.type;

            switch (type) {
                case MaterialType::LAMBERTIAN:
                    lambertian.albedo = m.lambertian.albedo;
                    break;
                case MaterialType::METAL:
                    metal.albedo = m.metal.albedo;
                    break;
                case MaterialType::DIELECTRIC:
                    dielectric.n0 = m.dielectric.n0;
                    dielectric.n1 = m.dielectric.n1;
                    break;
                case MaterialType::EMISSIVE:
                    emissive.emission = m.emissive.emission;
                    break;
                case MaterialType::THINFILM:
                    thinFilm.numLayers = m.thinFilm.numLayers;

                    for (int i = 0; i < thinFilm.numLayers + 2; i++)
                        thinFilm.n[i] = m.thinFilm.n[i];

                    for (int i = 0; i < thinFilm.numLayers; i++)
                        thinFilm.d[i] = m.thinFilm.d[i];

                    break;
            }
        }

        HOST_DEVICE Material & operator=(const Material & m) {
            type = m.type;
            switch (type) {
                case MaterialType::LAMBERTIAN:
                    lambertian.albedo = m.lambertian.albedo;
                    break;
                case MaterialType::METAL:
                    metal.albedo = m.metal.albedo;
                    break;
                case MaterialType::DIELECTRIC:
                    dielectric.n0 = m.dielectric.n0;
                    dielectric.n1 = m.dielectric.n1;
                    break;
                case MaterialType::EMISSIVE:
                    emissive.emission = m.emissive.emission;
                    break;
                case MaterialType::THINFILM:
                    thinFilm.numLayers = m.thinFilm.numLayers;

                    for (int i = 0; i < thinFilm.numLayers + 2; i++)
                        thinFilm.n[i] = m.thinFilm.n[i];

                    for (int i = 0; i < thinFilm.numLayers; i++)
                        thinFilm.d[i] = m.thinFilm.d[i];

                    break;
            }

            return *this;
        }

        HOST_DEVICE static Material makeLambertian(const SpectrumTexture & a) {
            Material material;

            material.type = MaterialType::LAMBERTIAN;
            material.lambertian.albedo = a;

            return material;
        }

        HOST_DEVICE static Material makeMetal(const SpectrumTexture & a) {
            Material material;

            material.type = MaterialType::METAL;
            material.metal.albedo = a;

            return material;
        }

        HOST_DEVICE static Material makeDielectric(Float n0, Float n1) {
            Material material;

            material.type = MaterialType::DIELECTRIC;
            material.dielectric.n0 = n0;
            material.dielectric.n1 = n1;

            return material;
        }

        HOST_DEVICE static Material makeEmissive(const SpectrumTexture & e) {
            Material material;

            material.type = MaterialType::EMISSIVE;
            material.emissive.emission = e;

            return material;
        }

        HOST_DEVICE static Material makeThinFilm(int numLayers, Float * n, ScalarTexture * d) {
            numLayers = numLayers <= MAX_THIN_FILM_LAYERS ? numLayers : MAX_THIN_FILM_LAYERS;

            Material material;

            material.type = MaterialType::THINFILM;
            material.thinFilm.numLayers = numLayers;

            for (int i = 0; i < numLayers + 2; i++)
                material.thinFilm.n[i] = n[i];

            for (int i = 0; i < numLayers; i++)
                material.thinFilm.d[i] = d[i];

            return material;
        }

        HOST_DEVICE bool scatter(const Ray & r, const Intersection & i, Ray & scattered, Float & attenuation, Random & state) const {
            switch (type) {
                case MaterialType::LAMBERTIAN: return scatterLambertian(r, i, scattered, attenuation, state);
                case MaterialType::METAL: return scatterMetal(r, i, scattered, attenuation);
                case MaterialType::DIELECTRIC: return scatterDielectric(r, i, scattered, attenuation, state);
                case MaterialType::EMISSIVE: return scatterEmissive(r, i, attenuation);
                case MaterialType::THINFILM: return scatterThinFilm(r, i, scattered, attenuation, state);
            }

            return false;
        }

    private:
        MaterialType type;

        union {
            struct { SpectrumTexture albedo; } lambertian;
            struct { SpectrumTexture albedo; } metal;
            struct { Float n0, n1; } dielectric;
            struct { SpectrumTexture emission; } emissive;
            struct { int numLayers; Float n[MAX_THIN_FILM_LAYERS + 2]; ScalarTexture d[MAX_THIN_FILM_LAYERS]; } thinFilm;
        };

        HOST_DEVICE bool scatterLambertian(const Ray & r, const Intersection & i, Ray & scattered, Float & attenuation, Random & state) const {
            scattered = Ray(i.point, randomInHemisphere(i.normal, state), r.getLambda(), r.getLambdaIndex());
            attenuation = lambertian.albedo.evaluate(i)[r.getLambdaIndex()];

            return true;
        }

        HOST_DEVICE bool scatterMetal(const Ray & r, const Intersection & i, Ray & scattered, Float & attenuation) const {
            scattered = Ray(i.point, reflected(r.getDirection(), i.normal), r.getLambda(), r.getLambdaIndex());
            attenuation = metal.albedo.evaluate(i)[r.getLambdaIndex()];

            return true;
        }

        HOST_DEVICE bool scatterDielectric(const Ray & r, const Intersection & i, Ray & scattered, Float & attenuation, Random & state) const {
            Float ratio = i.frontFacing ? dielectric.n0 / dielectric.n1 : dielectric.n1 / dielectric.n0;
            Float reflectance = schlick(r.getDirection(), i.normal);
            Vector direction = (randomDouble(state) < reflectance) ? reflected(r.getDirection(), i.normal) : refracted(r.getDirection(), i.normal, ratio);

            scattered = Ray(i.point, direction, r.getLambda(), r.getLambdaIndex());
            attenuation = 1;

            return true;
        }

        HOST_DEVICE Float schlick(const Vector & v, const Vector & n) const {
            Float r0 = (dielectric.n0 - dielectric.n1) / (dielectric.n0 + dielectric.n1);
            r0 *= r0;
            return r0 + (1 - r0) * pow(1 + v.dot(n), 5);
        }

        HOST_DEVICE bool scatterEmissive(const Ray & r, const Intersection & i, Float & attenuation) const {
            attenuation = emissive.emission.evaluate(i)[r.getLambdaIndex()];

            return false;
        }

        HOST_DEVICE bool scatterThinFilm(const Ray & r, const Intersection & i, Ray & scattered, Float & attenuation, Random & state) const {
            Float reflectance = thinFilmReflectance(r, i);
            Float ratio = i.frontFacing ? thinFilm.n[0] / thinFilm.n[thinFilm.numLayers + 1] : thinFilm.n[thinFilm.numLayers + 1] / thinFilm.n[0];
            Vector direction = reflected(r.getDirection(), i.normal);
            attenuation = 1;

            if (randomDouble(state) > reflectance) direction = refracted(r.getDirection(), i.normal, ratio);

            scattered = Ray(i.point, direction, r.getLambda(), r.getLambdaIndex());

            return true;
        }

        HOST_DEVICE Float thinFilmReflectance(const Ray & r, const Intersection & i) const {
            int jOffset = i.frontFacing ? 0 : thinFilm.numLayers + 1;
            int jSign = i.frontFacing ? 1 : -1;

            Float n0 = thinFilm.n[jOffset];
            Float n1 = thinFilm.n[jOffset + jSign];

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

                n0 = thinFilm.n[jOffset + jSign * (j + 1)];
                n1 = thinFilm.n[jOffset + jSign * (j + 2)];

                cosCurrent = cosNext;
                sinCurrent = sinNext;
                sinNext = n0 / n1;
                sinNext *= sinNext * sinCurrent;
                cosNext = squareRoot(Complex(1) - sinNext);

                Matrix interfaceS = interfaceMatrixS(n0, n1, cosCurrent, cosNext);
                Matrix interfaceP = interfaceMatrixP(n0, n1, cosCurrent, cosNext);

                Complex phi = Complex(2 * PI / r.getLambda()) * n0 * thinFilm.d[dIndex].evaluate(i) * cosCurrent;

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