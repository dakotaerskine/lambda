#pragma once

#include "intersection.h"
#include "platform.h"
#include "scene.h"
#include "spectrum.h"
#include "utils.h"

enum class ScalarTextureType { CONSTANT, PERLIN, WORLEY };
enum class SpectrumTextureType { CONSTANT, CHECKER };

class ScalarTexture {
    public:
        HOST_DEVICE ScalarTexture() : type(ScalarTextureType::CONSTANT) {}

        HOST_DEVICE ScalarTexture(const ScalarTexture & t) {
            type = t.type;

            switch (type) {
                case ScalarTextureType::CONSTANT:
                    constant.value = t.constant.value;
                    break;
                case ScalarTextureType::PERLIN:
                    perlin.min = t.perlin.min;
                    perlin.max = t.perlin.max;
                    perlin.frequency = t.perlin.frequency;
                    break;
                case ScalarTextureType::WORLEY:
                    worley.min = t.worley.min;
                    worley.max = t.worley.max;
                    worley.frequency = t.worley.frequency;
                    break;
            }
        }

        HOST_DEVICE ScalarTexture & operator=(const ScalarTexture & t) {
            type = t.type;

            switch (type) {
                case ScalarTextureType::CONSTANT:
                    constant.value = t.constant.value;
                    break;
                case ScalarTextureType::PERLIN:
                    perlin.min = t.perlin.min;
                    perlin.max = t.perlin.max;
                    perlin.frequency = t.perlin.frequency;
                    break;
                case ScalarTextureType::WORLEY:
                    worley.min = t.worley.min;
                    worley.max = t.worley.max;
                    worley.frequency = t.worley.frequency;
                    break;
            }

            return *this;
        }

        HOST_DEVICE static ScalarTexture makeConstant(Float v) {
            ScalarTexture texture;

            texture.type = ScalarTextureType::CONSTANT;
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
                case ScalarTextureType::CONSTANT:
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
        HOST_DEVICE SpectrumTexture() : type(SpectrumTextureType::CONSTANT) {}

        HOST_DEVICE SpectrumTexture(const SpectrumTexture & s) {
            type = s.type;

            switch (type) {
                case SpectrumTextureType::CONSTANT:
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
                case SpectrumTextureType::CONSTANT:
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

            texture.type = SpectrumTextureType::CONSTANT;
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

        HOST_DEVICE Float evaluate(const Scene & s, const Intersection & i, Float lambda) const {
            switch (type) {
                case SpectrumTextureType::CONSTANT:
                    return evaluateConstant(s, lambda);
                case SpectrumTextureType::CHECKER:
                    return evaluateChecker(s, i, lambda);
            }

            return constant.value;
        }

    private:
        SpectrumTextureType type;

        union {
            struct { int value; } constant;
            struct { int value1, value2; Float scale; } checker;
        };

        HOST_DEVICE Float evaluateConstant(const Scene & s, Float lambda) const { return s.getSpectra()[constant.value](lambda); }

        HOST_DEVICE Float evaluateChecker(const Scene & s, const Intersection & i, Float lambda) const {
            int x = int(floor(i.u / checker.scale));
            int y = int(floor(i.v / checker.scale));

            return (x + y) % 2 == 0 ? s.getSpectra()[checker.value1](lambda) : s.getSpectra()[checker.value2](lambda);
        }
};