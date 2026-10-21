#pragma once

#include <cmath>

#include "core/platform.h"
#include "core/utils.h"
#include "math/spectrum.h"
#include "scene/intersection.h"

namespace lambda {
    class ScalarTexture {
        public:
            ScalarTexture() : type(Type::CONSTANT) {}

            static ScalarTexture makeConstant(Float v) {
                ScalarTexture texture;

                texture.type = Type::CONSTANT;
                texture.width = 1;
                texture.height = 1;
                texture.min = v;
                texture.max = v;
                texture.average = v;
                texture.constant.value = v;

                return texture;
            }

            static ScalarTexture makeScale(ScalarTexture * t1, ScalarTexture * t2) {
                ScalarTexture texture;

                texture.type = Type::SCALE;

                int width1 = t1->width;
                int width2 = t2->width;

                int height1 = t1->height;
                int height2 = t2->height;

                int minWidth = std::min(width1, width2);
                int minHeight = std::min(height1, height2);

                int maxWidth = std::max(width1, width2);
                int maxHeight = std::max(height1, height2);

                if (width1 == 0 || width2 == 0) texture.width = 0;
                else if (maxWidth % minWidth == 0) texture.width = maxWidth;
                else texture.width = 0;

                if (height1 == 0 || height2 == 0) texture.height = 0;
                else if (maxHeight % minHeight == 0) texture.height = maxHeight;
                else texture.height = 0;

                texture.min = std::fmin(t1->min * t2->min, std::fmin(t1->min * t2->max, std::fmin(t1->max * t2->min, t1->max * t2->max)));
                texture.max = std::fmax(t1->min * t2->min, std::fmax(t1->min * t2->max, std::fmax(t1->max * t2->min, t1->max * t2->max)));
                texture.average = t1->average * t2->average;
                texture.scale.texture1 = t1;
                texture.scale.texture2 = t2;

                return texture;
            }

            static ScalarTexture makeMix(ScalarTexture * t1, ScalarTexture * t2, ScalarTexture * factor) {
                ScalarTexture texture;

                texture.type = Type::MIX;

                int width1 = t1->width;
                int width2 = t2->width;
                int width3 = factor->width;

                int height1 = t1->height;
                int height2 = t2->height;
                int height3 = factor->height;

                int minWidth = std::min(width1, std::min(width2, width3));
                int minHeight = std::min(height1, std::min(height2, height3));

                int maxWidth = std::max(width1, std::max(width2, width3));
                int maxHeight = std::max(height1, std::max(height2, height3));

                int midWidth = width1 + width2 + width3 - minWidth - maxWidth;
                int midHeight = height1 + height2 + height3 - minHeight - maxHeight;
                
                if (width1 == 0 || width2 == 0 || width3 == 0) texture.width = 0;
                else if (maxWidth % minWidth == 0 && maxWidth % midWidth == 0) texture.width = maxWidth;
                else texture.width = 0;

                if (height1 == 0 || height2 == 0 || height3 == 0) texture.height = 0;
                else if (maxHeight % minHeight == 0 && maxHeight % midHeight == 0) texture.height = maxHeight;
                else texture.height = 0;

                texture.min = std::fmin(t1->min, t2->min);
                texture.max = std::fmax(t1->max, t2->max);
                texture.average = utils::interpolate(t1->average, t2->average, factor->average);
                texture.mix.texture1 = t1;
                texture.mix.texture2 = t2;
                texture.mix.factor = factor;

                return texture;
            }

            static ScalarTexture makeChecker(Float v1, Float v2, Float uScale, Float vScale, Float uOffset, Float vOffset) {
                ScalarTexture texture;

                texture.type = Type::CHECKER;
                texture.width = 0;
                texture.height = 0;
                texture.min = std::fmin(v1, v2);
                texture.max = std::fmax(v1, v2);
                texture.average = Float(0.5) * (v1 + v2);
                texture.checker.value1 = v1;
                texture.checker.value2 = v2;
                texture.checker.uScale = uScale;
                texture.checker.vScale = vScale;
                texture.checker.uOffset = uOffset;
                texture.checker.vOffset = vOffset;

                return texture;
            }

            static ScalarTexture makePerlin(Float frequency, Float roughness, int octaves) {
                ScalarTexture texture;

                texture.type = Type::PERLIN;
                texture.width = 0;
                texture.height = 0;
                texture.min = 0;
                texture.max = 1;
                texture.average = 0.5;
                texture.perlin.frequency = frequency;
                texture.perlin.roughness = roughness;
                texture.perlin.octaves = octaves;

                return texture;
            }

            static ScalarTexture makeWorley(Float frequency, Float roughness, int octaves) {
                ScalarTexture texture;

                texture.type = Type::WORLEY;
                texture.width = 0;
                texture.height = 0;
                texture.min = 0;
                texture.max = 1;
                texture.average = 0.5;
                texture.worley.frequency = frequency;
                texture.worley.roughness = roughness;
                texture.worley.octaves = octaves;

                return texture;
            }

            static ScalarTexture makeImage(Float * image, int width, int height, Float uScale, Float vScale, Float uOffset, Float vOffset) {
                ScalarTexture texture;

                texture.type = Type::IMAGE;
                texture.width = (uScale == 1 && vScale == 1 && uOffset == 0 && vOffset == 0) ? width : 0;
                texture.height = (uScale == 1 && vScale == 1 && uOffset == 0 && vOffset == 0) ? height : 0;

                texture.min = constants::MAX;
                texture.max = -constants::MAX;
                texture.average = 0;

                for (int i = 0; i < height; i++) {
                    Float rowSum = 0;

                    for (int j = 0; j < width; j++) {
                        texture.min = std::fmin(texture.min, image[i * width + j]);
                        texture.max = std::fmax(texture.max, image[i * width + j]);

                        rowSum += image[i * width + j];
                    }

                    texture.average += rowSum / Float(width);
                }

                texture.average /= Float(height);

                texture.image.image = image;
                texture.image.width = width;
                texture.image.height = height;
                texture.image.uScale = uScale;
                texture.image.vScale = vScale;
                texture.image.uOffset = uOffset;
                texture.image.vOffset = vOffset;

                return texture;
            }

            LAMBDA_HOST_DEVICE int getWidth() const { return width > 0 ? width : 1; }

            LAMBDA_HOST_DEVICE int getHeight() const { return height > 0 ? height : 1; }

            LAMBDA_HOST_DEVICE Float getMin() const { return min; }

            LAMBDA_HOST_DEVICE Float getMax() const { return max; }

            LAMBDA_HOST_DEVICE Float getAverage() const { return average; }

            LAMBDA_HOST_DEVICE Float getAverage(int x, int y) const {
                switch (type) {
                    case Type::CONSTANT: return average;
                    case Type::SCALE: return scale.texture1->getAverage(width != 0 ? x * scale.texture1->width / width : x, height != 0 ? y * scale.texture1->height / height : y) * scale.texture2->getAverage(width != 0 ? x * scale.texture2->width / width : x, height != 0 ? y * scale.texture2->height / height : y);
                    case Type::MIX: return utils::interpolate(mix.texture1->getAverage(width != 0 ? x * mix.texture1->width / width : x, height != 0 ? y * mix.texture1->height / height : y), mix.texture2->getAverage(width != 0 ? x * mix.texture2->width / width : x, height != 0 ? y * mix.texture2->height / height : y), mix.factor->getAverage(width != 0 ? x * mix.factor->width / width : x, height != 0 ? y * mix.factor->height / height : y));
                    case Type::CHECKER: return average;
                    case Type::PERLIN: return average;
                    case Type::WORLEY: return average;
                    case Type::IMAGE: return getAverageImage(x, y);
                }

                return 0;
            }

            LAMBDA_HOST_DEVICE_NOINLINE Float evaluate(const Intersection & intersection) const {
                switch (type) {
                    case Type::CONSTANT: return constant.value;
                    case Type::SCALE: return scale.texture1->evaluate(intersection) * scale.texture2->evaluate(intersection);
                    case Type::MIX: return utils::interpolate(mix.texture1->evaluate(intersection), mix.texture2->evaluate(intersection), mix.factor->evaluate(intersection));
                    case Type::CHECKER: return evaluateChecker(intersection);
                    case Type::PERLIN: return evaluatePerlin(intersection);
                    case Type::WORLEY: return evaluateWorley(intersection);
                    case Type::IMAGE: return evaluateImage(intersection);
                }

                return 0;
            }

        private:
            friend class SpectrumTexture;

            enum class Type { CONSTANT, SCALE, MIX, CHECKER, PERLIN, WORLEY, IMAGE };

            Type type;
            int width, height;
            Float min, max, average;

            union {
                struct { Float value; } constant;
                struct { ScalarTexture * texture1, * texture2; } scale;
                struct { ScalarTexture * texture1, * texture2, * factor; } mix;
                struct { Float value1, value2, uScale, vScale, uOffset, vOffset; } checker;
                struct { Float frequency, roughness; int octaves; } perlin;
                struct { Float frequency, roughness; int octaves; } worley;
                struct { Float * image; int width, height; Float uScale, vScale, uOffset, vOffset; } image;
            };

            LAMBDA_HOST_DEVICE Float getAverageImage(int x, int y) const {
                if (image.uScale != 1 || image.vScale != 1 || image.uOffset != 0 || image.vOffset != 0) return average;

                x = ((x % image.width) + image.width) % image.width;
                y = ((y % image.height) + image.height) % image.height;

                return image.image[y * image.width + x];
            }

            LAMBDA_HOST_DEVICE Float evaluateChecker(const Intersection & intersection) const {
                int x = int(std::floor(intersection.textureCoordinate[0] * checker.uScale + checker.uOffset));
                int y = int(std::floor(intersection.textureCoordinate[1] * checker.vScale + checker.vOffset));

                return abs(x + y) % 2 == 0 ? checker.value1 : checker.value2;
            }

            LAMBDA_HOST_DEVICE Float evaluatePerlin(const Intersection & intersection) const {
                Float value = 0, amplitude = 1, factor = 0;

                Vector<Float, 3> point = intersection.useTransformed ? intersection.transformedPoint : intersection.point;

                point *= perlin.frequency;

                for (int i = 0; i < perlin.octaves; i++) {
                    value += utils::perlinNoise(point) * amplitude;
                    factor += amplitude;

                    amplitude *= perlin.roughness;
                    point *= 2;
                }

                return value / factor;
            }

            LAMBDA_HOST_DEVICE Float evaluateWorley(const Intersection & intersection) const {
                Float value = 0, amplitude = 1, factor = 0;

                Vector<Float, 3> point = intersection.useTransformed ? intersection.transformedPoint : intersection.point;

                point *= worley.frequency;

                for (int i = 0; i < worley.octaves; i++) {
                    value += utils::worleyNoise(point) * amplitude;
                    factor += amplitude;

                    amplitude *= worley.roughness;
                    point *= 2;
                }

                return value / factor;
            }

            LAMBDA_HOST_DEVICE Float evaluateImage(const Intersection & intersection) const {
                Float u = intersection.textureCoordinate[0] * image.uScale + image.uOffset;
                Float v = intersection.textureCoordinate[1] * image.vScale + image.vOffset;

                u = u - std::floor(u);
                v = utils::clamp(v, Float(0), Float(1));

                Float x = u * Float(image.width) - Float(0.5);
                Float y = v * Float(image.height) - Float(0.5);

                int x0 = int(std::floor(x));
                int y0 = int(std::floor(y));

                int x1 = x0 + 1;
                int y1 = y0 + 1;

                Float tx = x - Float(x0);
                Float ty = y - Float(y0);

                x0 = ((x0 % image.width) + image.width) % image.width;
                x1 = ((x1 % image.width) + image.width) % image.width;

                y1 = utils::clamp(y1, 0, image.height - 1);
                y0 = utils::clamp(y0, 0, image.height - 1);

                Float c00 = image.image[y0 * image.width + x0];
                Float c10 = image.image[y0 * image.width + x1];
                Float c01 = image.image[y1 * image.width + x0];
                Float c11 = image.image[y1 * image.width + x1];

                return utils::interpolate(utils::interpolate(c00, c10, tx), utils::interpolate(c01, c11, tx), ty);
            }
    };

    class SpectrumTexture {
        public:
            SpectrumTexture() : type(Type::CONSTANT) {}

            static SpectrumTexture makeConstant(DenseSpectrum<Float> * v) {
                SpectrumTexture texture;

                texture.type = Type::CONSTANT;
                texture.width = 1;
                texture.height = 1;
                texture.min = v->min();
                texture.max = v->max();
                texture.average = v->average();
                texture.constant.value = v;

                return texture;
            }

            static SpectrumTexture makeScalar(ScalarTexture * t) {
                SpectrumTexture texture;

                texture.type = Type::SCALAR;
                texture.width = t->width;
                texture.height = t->height;
                texture.min = t->min;
                texture.max = t->max;
                texture.average = t->average;
                texture.scalar.texture = t;

                return texture;
            }

            static SpectrumTexture makeScale(SpectrumTexture * t1, SpectrumTexture * t2) {
                SpectrumTexture texture;

                texture.type = Type::SCALE;

                int width1 = t1->width;
                int width2 = t2->width;

                int height1 = t1->height;
                int height2 = t2->height;

                int minWidth = std::min(width1, width2);
                int minHeight = std::min(height1, height2);

                int maxWidth = std::max(width1, width2);
                int maxHeight = std::max(height1, height2);

                if (width1 == 0 || width2 == 0) texture.width = 0;
                else if (maxWidth % minWidth == 0) texture.width = maxWidth;
                else texture.width = 0;

                if (height1 == 0 || height2 == 0) texture.height = 0;
                else if (maxHeight % minHeight == 0) texture.height = maxHeight;
                else texture.height = 0;

                texture.min = std::fmin(t1->min * t2->min, std::fmin(t1->min * t2->max, std::fmin(t1->max * t2->min, t1->max * t2->max)));
                texture.max = std::fmax(t1->min * t2->min, std::fmax(t1->min * t2->max, std::fmax(t1->max * t2->min, t1->max * t2->max)));
                texture.average = t1->average * t2->average;
                texture.scale.texture1 = t1;
                texture.scale.texture2 = t2;

                return texture;
            }

            static SpectrumTexture makeMix(SpectrumTexture * t1, SpectrumTexture * t2, ScalarTexture * factor) {
                SpectrumTexture texture;

                texture.type = Type::MIX;

                int width1 = t1->width;
                int width2 = t2->width;
                int width3 = factor->width;

                int height1 = t1->height;
                int height2 = t2->height;
                int height3 = factor->height;

                int minWidth = std::min(width1, std::min(width2, width3));
                int minHeight = std::min(height1, std::min(height2, height3));

                int maxWidth = std::max(width1, std::max(width2, width3));
                int maxHeight = std::max(height1, std::max(height2, height3));

                int midWidth = width1 + width2 + width3 - minWidth - maxWidth;
                int midHeight = height1 + height2 + height3 - minHeight - maxHeight;
                
                if (width1 == 0 || width2 == 0 || width3 == 0) texture.width = 0;
                else if (maxWidth % minWidth == 0 && maxWidth % midWidth == 0) texture.width = maxWidth;
                else texture.width = 0;

                if (height1 == 0 || height2 == 0 || height3 == 0) texture.height = 0;
                else if (maxHeight % minHeight == 0 && maxHeight % midHeight == 0) texture.height = maxHeight;
                else texture.height = 0;

                texture.min = std::fmin(t1->min, t2->min);
                texture.max = std::fmax(t1->max, t2->max);
                texture.average = utils::interpolate(t1->average, t2->average, factor->average);
                texture.mix.texture1 = t1;
                texture.mix.texture2 = t2;
                texture.mix.factor = factor;

                return texture;
            }

            static SpectrumTexture makeChecker(DenseSpectrum<Float> * v1, DenseSpectrum<Float> * v2, Float uScale, Float vScale, Float uOffset, Float vOffset) {
                SpectrumTexture texture;

                texture.type = Type::CHECKER;
                texture.width = 0;
                texture.height = 0;
                texture.min = std::fmin(v1->min(), v2->min());
                texture.max = std::fmax(v1->max(), v2->max());
                texture.average = Float(0.5) * (v1->average() + v2->average());
                texture.checker.value1 = v1;
                texture.checker.value2 = v2;
                texture.checker.uScale = uScale;
                texture.checker.vScale = vScale;
                texture.checker.uOffset = uOffset;
                texture.checker.vOffset = vOffset;

                return texture;
            }

            static SpectrumTexture makeImage(Float * image, ColorSpace space, int width, int height, Float uScale, Float vScale, Float uOffset, Float vOffset) {
                SpectrumTexture texture;

                texture.type = Type::IMAGE;
                texture.width = (uScale == 1 && vScale == 1 && uOffset == 0 && vOffset == 0) ? width : 0;
                texture.height = (uScale == 1 && vScale == 1 && uOffset == 0 && vOffset == 0) ? height : 0;

                texture.min = constants::MAX;
                texture.max = -constants::MAX;
                texture.average = 0;

                for (int i = 0; i < height; i++) {
                    Float rowSum = 0;

                    for (int j = 0; j < width; j++)
                        for (int k = 0; k < 3; k++) {
                            texture.min = std::fmin(texture.min, image[(i * width + j) * 3 + k]);
                            texture.max = std::fmax(texture.max, image[(i * width + j) * 3 + k]);

                            rowSum += image[(i * width + j) * 3 + k];
                        }

                    texture.average += rowSum / Float(width);
                }

                texture.average /= Float(height * 3);

                texture.min = std::fmax(texture.min, Float(0));
                texture.max = std::fmax(texture.max, Float(0));
                texture.average = std::fmax(texture.average, Float(0));

                texture.image.image = image;
                texture.image.space = space;
                texture.image.width = width;
                texture.image.height = height;
                texture.image.uScale = uScale;
                texture.image.vScale = vScale;
                texture.image.uOffset = uOffset;
                texture.image.vOffset = vOffset;

                return texture;
            }

            LAMBDA_HOST_DEVICE int getWidth() const { return width > 0 ? width : 1; }

            LAMBDA_HOST_DEVICE int getHeight() const { return height > 0 ? height : 1; }

            LAMBDA_HOST_DEVICE Float getMin() const { return min; }

            LAMBDA_HOST_DEVICE Float getMax() const { return max; }

            LAMBDA_HOST_DEVICE Float getAverage() const { return average; }

            LAMBDA_HOST_DEVICE Float getAverage(int x, int y) const {
                switch (type) {
                    case Type::CONSTANT: return average;
                    case Type::SCALAR: return scalar.texture->getAverage(x, y);
                    case Type::SCALE: return scale.texture1->getAverage(width != 0 ? x * scale.texture1->width / width : x, height != 0 ? y * scale.texture1->height / height : y) * scale.texture2->getAverage(width != 0 ? x * scale.texture2->width / width : x, height != 0 ? y * scale.texture2->height / height : y);
                    case Type::MIX: return utils::interpolate(mix.texture1->getAverage(width != 0 ? x * mix.texture1->width / width : x, height != 0 ? y * mix.texture1->height / height : y), mix.texture2->getAverage(width != 0 ? x * mix.texture2->width / width : x, height != 0 ? y * mix.texture2->height / height : y), mix.factor->getAverage(width != 0 ? x * mix.factor->width / width : x, height != 0 ? y * mix.factor->height / height : y));
                    case Type::CHECKER: return average;
                    case Type::IMAGE: return getAverageImage(x, y);
                }

                return 0;
            }

            LAMBDA_HOST_DEVICE_NOINLINE Float evaluate(const Intersection & intersection, Float lambda) const {
                switch (type) {
                    case Type::CONSTANT: return constant.value->get(lambda);
                    case Type::SCALAR: return scalar.texture->evaluate(intersection);
                    case Type::SCALE: return scale.texture1->evaluate(intersection, lambda) * scale.texture2->evaluate(intersection, lambda);
                    case Type::MIX: return utils::interpolate(mix.texture1->evaluate(intersection, lambda), mix.texture2->evaluate(intersection, lambda), mix.factor->evaluate(intersection));
                    case Type::CHECKER: return evaluateChecker(intersection, lambda);
                    case Type::IMAGE: return evaluateImage(intersection, lambda);
                }

                return 0;
            }

        private:
            enum class Type { CONSTANT, SCALAR, SCALE, MIX, CHECKER, IMAGE };

            Type type;
            int width, height;
            Float min, max, average;

            union {
                struct { DenseSpectrum<Float> * value; } constant;
                struct { SpectrumTexture * texture1, * texture2; } scale;
                struct { ScalarTexture * texture; } scalar;
                struct { SpectrumTexture * texture1, * texture2; ScalarTexture * factor; } mix;
                struct { DenseSpectrum<Float> * value1, * value2; Float uScale, vScale, uOffset, vOffset; } checker;
                struct { Float * image; ColorSpace space; int width, height; Float uScale, vScale, uOffset, vOffset; } image;
            };

            LAMBDA_HOST_DEVICE Float getAverageImage(int x, int y) const {
                if (image.uScale != 1 || image.vScale != 1 || image.uOffset != 0 || image.vOffset != 0) return average;

                x = ((x % image.width) + image.width) % image.width;
                y = ((y % image.height) + image.height) % image.height;

                Float sum = 0;

                for (int i = 0; i < 3; i++) sum += image.image[(y * image.width + x) * 3 + i];

                return sum / 3;
            }

            LAMBDA_HOST_DEVICE Float evaluateChecker(const Intersection & intersection, Float lambda) const {
                int x = int(std::floor(intersection.textureCoordinate[0] * checker.uScale + checker.uOffset));
                int y = int(std::floor(intersection.textureCoordinate[1] * checker.vScale + checker.vOffset));

                return abs(x + y) % 2 == 0 ? checker.value1->get(lambda) : checker.value2->get(lambda);
            }

            LAMBDA_HOST_DEVICE Float evaluateImage(const Intersection & intersection, Float lambda) const {
                Float u = intersection.textureCoordinate[0] * image.uScale + image.uOffset;
                Float v = intersection.textureCoordinate[1] * image.vScale + image.vOffset;

                u = u - std::floor(u);
                v = utils::clamp(v, Float(0), Float(1));

                Float x = u * Float(image.width) - Float(0.5);
                Float y = v * Float(image.height) - Float(0.5);

                int x0 = int(std::floor(x));
                int y0 = int(std::floor(y));

                int x1 = x0 + 1;
                int y1 = y0 + 1;

                Float tx = x - Float(x0);
                Float ty = y - Float(y0);

                x0 = ((x0 % image.width) + image.width) % image.width;
                x1 = ((x1 % image.width) + image.width) % image.width;

                y1 = utils::clamp(y1, 0, image.height - 1);
                y0 = utils::clamp(y0, 0, image.height - 1);

                Vector<Float, 3> color;

                for (int i = 0; i < 3; i++) {
                    Float c00 = image.image[(y0 * image.width + x0) * 3 + i];
                    Float c10 = image.image[(y0 * image.width + x1) * 3 + i];
                    Float c01 = image.image[(y1 * image.width + x0) * 3 + i];
                    Float c11 = image.image[(y1 * image.width + x1) * 3 + i];

                    color[i] = utils::interpolate(utils::interpolate(c00, c10, tx), utils::interpolate(c01, c11, tx), ty);
                }

                Float upsamplingScale = std::fmax(color[0], std::fmax(color[1], color[2]));

                if (upsamplingScale <= 1) upsamplingScale = 1;
                else color /= upsamplingScale;

                Vector<Float, 3> coefficients = utils::upsampleRGB(image.space, color);

                return utils::sigmoid((coefficients[0] * lambda + coefficients[1]) * lambda + coefficients[2]) * upsamplingScale;
            }
    };
}