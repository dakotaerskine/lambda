#pragma once

#include "core/platform.h"
#include "core/utils.h"
#include "math/intersection.h"
#include "math/spectrum.h"

enum class ScalarTextureType { SCALAR_CONSTANT, PERLIN, WORLEY };
enum class SpectrumTextureType { SPECTRUM_CONSTANT, CHECKER, SCALAR };

class ScalarTexture {
    public:
        HOST_DEVICE ScalarTexture() : type(ScalarTextureType::SCALAR_CONSTANT) {}

        HOST_DEVICE static ScalarTexture makeConstant(Float v) {
            ScalarTexture texture;

            texture.type = ScalarTextureType::SCALAR_CONSTANT;
            texture.constant.value = v;

            return texture;
        }

        HOST_DEVICE static ScalarTexture makePerlin(Float min, Float max, Float frequency) {
            ScalarTexture texture;

            texture.type = ScalarTextureType::PERLIN;
            texture.perlin.min = min;
            texture.perlin.max = max;
            texture.perlin.frequency = frequency;

            return texture;
        }

        HOST_DEVICE static ScalarTexture makeWorley(Float min, Float max, Float frequency) {
            ScalarTexture texture;

            texture.type = ScalarTextureType::WORLEY;
            texture.worley.min = min;
            texture.worley.max = max;
            texture.worley.frequency = frequency;

            return texture;
        }

        HOST_DEVICE Float min() const {
            switch (type) {
                case ScalarTextureType::SCALAR_CONSTANT: return constant.value;
                case ScalarTextureType::PERLIN: return perlin.min;
                case ScalarTextureType::WORLEY: return worley.min;
            }

            return 0;
        }

        HOST_DEVICE Float max() const {
            switch (type) {
                case ScalarTextureType::SCALAR_CONSTANT: return constant.value;
                case ScalarTextureType::PERLIN: return perlin.max;
                case ScalarTextureType::WORLEY: return worley.max;
            }

            return 0;
        }

        HOST_DEVICE Float average() const {
            switch (type) {
                case ScalarTextureType::SCALAR_CONSTANT: return averageConstant();
                case ScalarTextureType::PERLIN: return averagePerlin();
                case ScalarTextureType::WORLEY: return averageWorley();
            }

            return 0;
        }

        HOST_DEVICE Float evaluate(const Intersection & i) const {
            switch (type) {
                case ScalarTextureType::SCALAR_CONSTANT:
                    return evaluateConstant();
                case ScalarTextureType::PERLIN:
                    return evaluatePerlin(i);
                case ScalarTextureType::WORLEY:
                    return evaluateWorley(i);
            }

            return 0;
        }

    private:
        ScalarTextureType type;

        union {
            struct { Float value; } constant;
            struct { Float min, max, frequency; } perlin;
            struct { Float min, max, frequency; } worley;
        };

        HOST_DEVICE Float averageConstant() const { return constant.value; }
        HOST_DEVICE Float averagePerlin() const { return (perlin.min + perlin.max) / 2; }
        HOST_DEVICE Float averageWorley() const { return (worley.min + worley.max) / 2; }

        HOST_DEVICE Float evaluateConstant() const {
            return constant.value;
        }

        HOST_DEVICE Float evaluatePerlin(const Intersection & i) const {
            return perlin.min + (perlin.max - perlin.min) * perlinNoise(i.point * perlin.frequency);
        }

        HOST_DEVICE Float evaluateWorley(const Intersection & i) const {
            return worley.min + (worley.max - worley.min) * worleyNoise(i.point * worley.frequency);
        }
};

class SpectrumTexture {
    public:
        HOST_DEVICE SpectrumTexture() : type(SpectrumTextureType::SPECTRUM_CONSTANT) {}

        HOST_DEVICE static SpectrumTexture makeConstant(int v) {
            SpectrumTexture texture;

            texture.type = SpectrumTextureType::SPECTRUM_CONSTANT;
            texture.constant.value = v;

            return texture;
        }

        HOST_DEVICE static SpectrumTexture makeChecker(int v1, int v2, Float scale) {
            SpectrumTexture texture;

            texture.type = SpectrumTextureType::CHECKER;
            texture.checker.value1 = v1;
            texture.checker.value2 = v2;
            texture.checker.scale = scale;

            return texture;
        }

        HOST_DEVICE static SpectrumTexture makeScalar(int index) {
            SpectrumTexture texture;

            texture.type = SpectrumTextureType::SCALAR;
            texture.scalar.index = index;

            return texture;
        }

        HOST_DEVICE Float min(DenseSpectrum<Float> * const spectra, ScalarTexture * const scalarTextures) const {
            switch (type) {
                case SpectrumTextureType::SPECTRUM_CONSTANT: return spectra[constant.value].min();
                case SpectrumTextureType::CHECKER: return fminF(spectra[checker.value1].min(), spectra[checker.value2].min());
                case SpectrumTextureType::SCALAR: return scalarTextures[scalar.index].min();
            }

            return 0;
        }

        HOST_DEVICE Float max(DenseSpectrum<Float> * const spectra, ScalarTexture * const scalarTextures) const {
            switch (type) {
                case SpectrumTextureType::SPECTRUM_CONSTANT: return spectra[constant.value].max();
                case SpectrumTextureType::CHECKER: return fmaxF(spectra[checker.value1].max(), spectra[checker.value2].max());
                case SpectrumTextureType::SCALAR: return scalarTextures[scalar.index].max();
            }

            return 0;
        }

        HOST_DEVICE Float average(DenseSpectrum<Float> * const spectra, ScalarTexture * const scalarTextures) const {
            switch (type) {
                case SpectrumTextureType::SPECTRUM_CONSTANT: return averageConstant(spectra);
                case SpectrumTextureType::CHECKER: return averageChecker(spectra);
                case SpectrumTextureType::SCALAR: return averageScalar(scalarTextures);
            }

            return 0;
        }

        HOST_DEVICE Float evaluate(DenseSpectrum<Float> * const spectra, ScalarTexture * const scalarTextures, const Intersection & i, Float lambda) const {
            switch (type) {
                case SpectrumTextureType::SPECTRUM_CONSTANT: return evaluateConstant(spectra, lambda);
                case SpectrumTextureType::CHECKER: return evaluateChecker(spectra, i, lambda);
                case SpectrumTextureType::SCALAR: return evaluateScalar(scalarTextures, i);
            }

            return 0;
        }

    private:
        SpectrumTextureType type;

        union {
            struct { int value; } constant;
            struct { int value1, value2; Float scale; } checker;
            struct { int index; } scalar;
        };

        HOST_DEVICE Float averageConstant(DenseSpectrum<Float> * const spectra) const { return spectra[constant.value].average(); }
        HOST_DEVICE Float averageChecker(DenseSpectrum<Float> * const spectra) const { return Float(0.5) * (spectra[checker.value1].average() + spectra[checker.value2].average()); }
        HOST_DEVICE Float averageScalar(ScalarTexture * const scalarTextures) const { return scalarTextures[scalar.index].average(); }

        HOST_DEVICE Float evaluateConstant(DenseSpectrum<Float> * const spectra, Float lambda) const { return spectra[constant.value](lambda); }

        HOST_DEVICE Float evaluateChecker(DenseSpectrum<Float> * const spectra, const Intersection & i, Float lambda) const {
            int x = int(floorF(i.u / checker.scale));
            int y = int(floorF(i.v / checker.scale));

            return (x + y) % 2 == 0 ? spectra[checker.value1](lambda) : spectra[checker.value2](lambda);
        }

        HOST_DEVICE Float evaluateScalar(ScalarTexture * const scalarTextures, const Intersection & i) const { return scalarTextures[scalar.index].evaluate(i); }
};