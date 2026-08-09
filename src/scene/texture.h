#pragma once

#include "core/platform.h"
#include "core/utils.h"
#include "math/intersection.h"
#include "math/spectrum.h"

enum class ScalarTextureType { SCALAR_CONSTANT, PERLIN, WORLEY };
enum class SpectrumTextureType { SPECTRUM_CONSTANT, CHECKER };

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

        HOST_DEVICE SpectrumTexture(const SpectrumTexture & s) {
            type = s.type;

            switch (type) {
                case SpectrumTextureType::SPECTRUM_CONSTANT:
                    constant.value = s.constant.value;
                    break;
                case SpectrumTextureType::CHECKER:
                    checker.value1 = s.checker.value1;
                    checker.value2 = s.checker.value2;
                    checker.scale = s.checker.scale;
                    break;
            }
        }

        HOST_DEVICE SpectrumTexture & operator=(const SpectrumTexture & s) {
            type = s.type;

            switch (type) {
                case SpectrumTextureType::SPECTRUM_CONSTANT:
                    constant.value = s.constant.value;
                    break;
                case SpectrumTextureType::CHECKER:
                    checker.value1 = s.checker.value1;
                    checker.value2 = s.checker.value2;
                    checker.scale = s.checker.scale;
                    break;
            }

            return *this;
        }

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

        HOST_DEVICE Float evaluate(DenseSpectrum<Float> * const spectra, const Intersection & i, Float lambda) const {
            switch (type) {
                case SpectrumTextureType::SPECTRUM_CONSTANT:
                    return evaluateConstant(spectra, lambda);
                case SpectrumTextureType::CHECKER:
                    return evaluateChecker(spectra, i, lambda);
            }

            return constant.value;
        }

    private:
        SpectrumTextureType type;

        union {
            struct { int value; } constant;
            struct { int value1, value2; Float scale; } checker;
        };

        HOST_DEVICE Float evaluateConstant(DenseSpectrum<Float> * const spectra, Float lambda) const { return spectra[constant.value](lambda); }

        HOST_DEVICE Float evaluateChecker(DenseSpectrum<Float> * const spectra, const Intersection & i, Float lambda) const {
            int x = int(floor(i.u / checker.scale));
            int y = int(floor(i.v / checker.scale));

            return (x + y) % 2 == 0 ? spectra[checker.value1](lambda) : spectra[checker.value2](lambda);
        }
};