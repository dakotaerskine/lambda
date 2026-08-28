#pragma once

#include <cmath>

#include "core/platform.h"
#include "core/utils.h"
#include "math/intersection.h"
#include "math/spectrum.h"

enum class ScalarTextureType { SCALAR_CONSTANT, PERLIN, WORLEY, IMAGE };
enum class SpectrumTextureType { SPECTRUM_CONSTANT, CHECKER, SCALAR, IMAGE };

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

        HOST_DEVICE static ScalarTexture makeImage(int index, int width, int height) {
            ScalarTexture texture;

            texture.type = ScalarTextureType::IMAGE;
            texture.image.index = index;
            texture.image.width = width;
            texture.image.height = height;

            return texture;
        }

        HOST_DEVICE Float min(const Float * images) const {
            switch (type) {
                case ScalarTextureType::SCALAR_CONSTANT: return constant.value;
                case ScalarTextureType::PERLIN: return perlin.min;
                case ScalarTextureType::WORLEY: return worley.min;
                case ScalarTextureType::IMAGE: return minImage(images);
            }

            return 0;
        }

        HOST_DEVICE Float max(const Float * images) const {
            switch (type) {
                case ScalarTextureType::SCALAR_CONSTANT: return constant.value;
                case ScalarTextureType::PERLIN: return perlin.max;
                case ScalarTextureType::WORLEY: return worley.max;
                case ScalarTextureType::IMAGE: return maxImage(images);
            }

            return 0;
        }

        HOST_DEVICE Float average(const Float * images) const {
            switch (type) {
                case ScalarTextureType::SCALAR_CONSTANT: return averageConstant();
                case ScalarTextureType::PERLIN: return averagePerlin();
                case ScalarTextureType::WORLEY: return averageWorley();
                case ScalarTextureType::IMAGE: return averageImage(images);
            }

            return 0;
        }

        HOST_DEVICE Float evaluate(const Float * images, const Intersection & i) const {
            switch (type) {
                case ScalarTextureType::SCALAR_CONSTANT: return evaluateConstant();
                case ScalarTextureType::PERLIN: return evaluatePerlin(i);
                case ScalarTextureType::WORLEY: return evaluateWorley(i);
                case ScalarTextureType::IMAGE: return evaluateImage(images, i);
            }

            return 0;
        }

    private:
        ScalarTextureType type;

        union {
            struct { Float value; } constant;
            struct { Float min, max, frequency; } perlin;
            struct { Float min, max, frequency; } worley;
            struct { int index, width, height; } image;
        };

        HOST_DEVICE Float minImage(const Float * images) const {
            Float min = MAX;

            for (int i = 0; i < image.width * image.height; i++)
                min = std::fmin(min, images[image.index + i]);

            return min;
        }

        HOST_DEVICE Float maxImage(const Float * images) const {
            Float max = -MAX;

            for (int i = 0; i < image.width * image.height; i++)
                max = std::fmax(max, images[image.index + i]);

            return max;
        }

        HOST_DEVICE Float averageConstant() const { return constant.value; }
        HOST_DEVICE Float averagePerlin() const { return (perlin.min + perlin.max) / 2; }
        HOST_DEVICE Float averageWorley() const { return (worley.min + worley.max) / 2; }

        HOST_DEVICE Float averageImage(const Float * images) const {
            Float sum = 0;

            for (int i = 0; i < image.width * image.height; i++)
                sum += images[image.index + i];

            return sum / Float(image.width * image.height);
        }

        HOST_DEVICE Float evaluateConstant() const {
            return constant.value;
        }

        HOST_DEVICE Float evaluatePerlin(const Intersection & i) const {
            return perlin.min + (perlin.max - perlin.min) * perlinNoise(i.localPoint * perlin.frequency);
        }

        HOST_DEVICE Float evaluateWorley(const Intersection & i) const {
            return worley.min + (worley.max - worley.min) * worleyNoise(i.localPoint * worley.frequency);
        }

        HOST_DEVICE Float evaluateImage(const Float * images, const Intersection & i) const {
            Float u = i.u - std::floor(i.u);
            Float v = i.v - std::floor(i.v);

            Float x = u * Float(image.width) - Float(0.5);
            Float y = v * Float(image.height) - Float(0.5);

            int x0 = int(std::floor(x));
            int y0 = int(std::floor(y));

            int x1 = x0 + 1;
            int y1 = y0 + 1;

            Float tx = x - Float(x0);
            Float ty = y - Float(y0);

            x0 = ((x0 % image.width) + image.width) % image.width;
            y0 = ((y0 % image.height) + image.height) % image.height;
            x1 = ((x1 % image.width) + image.width) % image.width;
            y1 = ((y1 % image.height) + image.height) % image.height;

            Float c00 = images[image.index + (y0 * image.width + x0) * 3];
            Float c10 = images[image.index + (y0 * image.width + x1) * 3];
            Float c01 = images[image.index + (y1 * image.width + x0) * 3];
            Float c11 = images[image.index + (y1 * image.width + x1) * 3];

            return interpolate(interpolate(c00, c10, tx), interpolate(c01, c11, tx), ty);
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

        HOST_DEVICE static SpectrumTexture makeImage(ColorSpace space, int index, int width, int height) {
            SpectrumTexture texture;

            texture.type = SpectrumTextureType::IMAGE;
            texture.image.space = space;
            texture.image.index = index;
            texture.image.width = width;
            texture.image.height = height;

            return texture;
        }

        HOST_DEVICE Float min(const DenseSpectrum<Float> * spectra, const ScalarTexture * scalarTextures, const Float * images) const {
            switch (type) {
                case SpectrumTextureType::SPECTRUM_CONSTANT: return spectra[constant.value].min();
                case SpectrumTextureType::CHECKER: return std::fmin(spectra[checker.value1].min(), spectra[checker.value2].min());
                case SpectrumTextureType::SCALAR: return scalarTextures[scalar.index].min(images);
                case SpectrumTextureType::IMAGE: return minImage(images);
            }

            return 0;
        }

        HOST_DEVICE Float max(const DenseSpectrum<Float> * spectra, const ScalarTexture * scalarTextures, const Float * images) const {
            switch (type) {
                case SpectrumTextureType::SPECTRUM_CONSTANT: return spectra[constant.value].max();
                case SpectrumTextureType::CHECKER: return std::fmax(spectra[checker.value1].max(), spectra[checker.value2].max());
                case SpectrumTextureType::SCALAR: return scalarTextures[scalar.index].max(images);
                case SpectrumTextureType::IMAGE: return maxImage(images);
            }

            return 0;
        }

        HOST_DEVICE Float average(const DenseSpectrum<Float> * spectra, const ScalarTexture * scalarTextures, const Float * images) const {
            switch (type) {
                case SpectrumTextureType::SPECTRUM_CONSTANT: return averageConstant(spectra);
                case SpectrumTextureType::CHECKER: return averageChecker(spectra);
                case SpectrumTextureType::SCALAR: return averageScalar(scalarTextures, images);
                case SpectrumTextureType::IMAGE: return averageImage(images);
            }

            return 0;
        }

        HOST_DEVICE Float evaluate(const DenseSpectrum<Float> * spectra, const ScalarTexture * scalarTextures, const Float * images, const Intersection & i, Float lambda) const {
            switch (type) {
                case SpectrumTextureType::SPECTRUM_CONSTANT: return evaluateConstant(spectra, lambda);
                case SpectrumTextureType::CHECKER: return evaluateChecker(spectra, i, lambda);
                case SpectrumTextureType::SCALAR: return evaluateScalar(scalarTextures, images, i);
                case SpectrumTextureType::IMAGE: return evaluateImage(images, i, lambda);
            }

            return 0;
        }

    private:
        SpectrumTextureType type;

        union {
            struct { int value; } constant;
            struct { int value1, value2; Float scale; } checker;
            struct { int index; } scalar;
            struct { ColorSpace space; int index, width, height; } image;
        };

        HOST_DEVICE Float minImage(const Float * images) const {
            Float min = MAX;

            for (int i = 0; i < image.width * image.height * 3; i++)
                min = std::fmin(min, images[image.index + i]);

            return min;
        }

        HOST_DEVICE Float maxImage(const Float * images) const {
            Float max = -MAX;

            for (int i = 0; i < image.width * image.height * 3; i++)
                max = std::fmax(max, images[image.index + i]);

            return max;
        }

        HOST_DEVICE Float averageConstant(const DenseSpectrum<Float> * spectra) const { return spectra[constant.value].average(); }
        HOST_DEVICE Float averageChecker(const DenseSpectrum<Float> * spectra) const { return Float(0.5) * (spectra[checker.value1].average() + spectra[checker.value2].average()); }
        HOST_DEVICE Float averageScalar(const ScalarTexture * scalarTextures, const Float * images) const { return scalarTextures[scalar.index].average(images); }

        HOST_DEVICE Float averageImage(const Float * images) const {
            Float sum = 0;

            for (int i = 0; i < image.width * image.height * 3; i++)
                sum += images[image.index + i];

            return sum / Float(image.width * image.height * 3);
        }

        HOST_DEVICE Float evaluateConstant(const DenseSpectrum<Float> * spectra, Float lambda) const { return spectra[constant.value](lambda); }

        HOST_DEVICE Float evaluateChecker(const DenseSpectrum<Float> * spectra, const Intersection & i, Float lambda) const {
            int x = int(std::floor(i.u / checker.scale));
            int y = int(std::floor(i.v / checker.scale));

            return abs(x + y) % 2 == 0 ? spectra[checker.value1](lambda) : spectra[checker.value2](lambda);
        }

        HOST_DEVICE Float evaluateScalar(const ScalarTexture * scalarTextures, const Float * images, const Intersection & i) const { return scalarTextures[scalar.index].evaluate(images, i); }

        HOST_DEVICE Float evaluateImage(const Float * images, const Intersection & i, Float lambda) const {
            Float u = i.u - std::floor(i.u);
            Float v = i.v - std::floor(i.v);

            Float x = u * Float(image.width) - Float(0.5);
            Float y = v * Float(image.height) - Float(0.5);

            int x0 = int(std::floor(x));
            int y0 = int(std::floor(y));

            int x1 = x0 + 1;
            int y1 = y0 + 1;

            Float tx = x - Float(x0);
            Float ty = y - Float(y0);

            x0 = ((x0 % image.width) + image.width) % image.width;
            y0 = ((y0 % image.height) + image.height) % image.height;
            x1 = ((x1 % image.width) + image.width) % image.width;
            y1 = ((y1 % image.height) + image.height) % image.height;

            Vector<Float> color;

            for (int j = 0; j < 3; j++) {
                Float c00 = images[image.index + (y0 * image.width + x0) * 3 + j];
                Float c10 = images[image.index + (y0 * image.width + x1) * 3 + j];
                Float c01 = images[image.index + (y1 * image.width + x0) * 3 + j];
                Float c11 = images[image.index + (y1 * image.width + x1) * 3 + j];

                color[j] = interpolate(interpolate(c00, c10, tx), interpolate(c01, c11, tx), ty);
            }

            Float scale = std::fmax(color[0], std::fmax(color[1], color[2]));

            if (scale <= 1) scale = 1;
            else color /= scale;

            Vector<Float> coefficients = upsampleRGB(image.space, color);

            return sigmoidF((coefficients[0] * lambda + coefficients[1]) * lambda + coefficients[2]) * scale;
        }
};