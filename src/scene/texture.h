#pragma once

#include <cmath>

#include "core/platform.h"
#include "core/utils.h"
#include "math/intersection.h"
#include "math/spectrum.h"

namespace lambda {
    class ScalarTexture {
        public:
            ScalarTexture() : type(Type::CONSTANT) {}

            static ScalarTexture makeConstant(Float v) {
                ScalarTexture texture;

                texture.type = Type::CONSTANT;
                texture.isConstant = true;
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
                texture.isConstant = t1->isConstant && t2->isConstant;

                int width1 = t1->width;
                int width2 = t2->width;

                int height1 = t1->height;
                int height2 = t2->height;

                int maxHeight = std::max(height1, height2);
                int maxWidth = std::max(width1, width2);

                if (t1->isConstant) width1 = maxWidth;
                if (t2->isConstant) width2 = maxWidth;

                if (t1->isConstant) height1 = maxHeight;
                if (t2->isConstant) height2 = maxHeight;

                texture.width = width1 == width2 ? width1 : 1;
                texture.height = height1 == height2 ? height1 : 1;

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
                texture.isConstant = t1->isConstant && t2->isConstant && factor->isConstant;

                int width1 = t1->width;
                int width2 = t2->width;
                int width3 = factor->width;

                int height1 = t1->height;
                int height2 = t2->height;
                int height3 = factor->height;

                int maxWidth = std::max(width1, std::max(width2, width3));
                int maxHeight = std::max(height1, std::max(height2, height3));

                if (t1->isConstant) width1 = maxWidth;
                if (t2->isConstant) width2 = maxWidth;
                if (factor->isConstant) width3 = maxWidth;

                if (t1->isConstant) height1 = maxHeight;
                if (t2->isConstant) height2 = maxHeight;
                if (factor->isConstant) height3 = maxHeight;

                texture.width = (width1 == width2 && width2 == width3) ? width1 : 1;
                texture.height = (height1 == height2 && height2 == height3) ? height1 : 1;

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
                texture.isConstant = false;
                texture.width = 1;
                texture.height = 1;
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
                texture.isConstant = false;
                texture.width = 1;
                texture.height = 1;
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
                texture.isConstant = false;
                texture.width = 1;
                texture.height = 1;
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
                texture.isConstant = false;
                texture.width = (std::fabs(uScale - 1) < constants::EPSILON && std::fabs(vScale - 1) < constants::EPSILON && std::fabs(uOffset) < constants::EPSILON && std::fabs(vOffset) < constants::EPSILON) ? width : 1;
                texture.height = (std::fabs(uScale - 1) < constants::EPSILON && std::fabs(vScale - 1) < constants::EPSILON && std::fabs(uOffset) < constants::EPSILON && std::fabs(vOffset) < constants::EPSILON) ? height : 1;

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

            LAMBDA_HOST_DEVICE int getWidth() const { return width; }

            LAMBDA_HOST_DEVICE int getHeight() const { return height; }

            LAMBDA_HOST_DEVICE Float getMin() const { return min; }

            LAMBDA_HOST_DEVICE Float getMax() const { return max; }

            LAMBDA_HOST_DEVICE Float getAverage() const { return average; }

            LAMBDA_HOST_DEVICE Float getAverage(int x, int y) const {
                switch (type) {
                    case Type::CONSTANT: return average;
                    case Type::SCALE: return scale.texture1->getAverage(x, y) * scale.texture2->getAverage(x, y);
                    case Type::MIX: return utils::interpolate(mix.texture1->getAverage(x, y), mix.texture2->getAverage(x, y), mix.factor->getAverage(x, y));
                    case Type::CHECKER: return average;
                    case Type::PERLIN: return average;
                    case Type::WORLEY: return average;
                    case Type::IMAGE: return getAverageImage(x, y);
                }

                return 0;
            }

            LAMBDA_HOST_DEVICE_NOINLINE Float evaluate(const Intersection & i) const {
                switch (type) {
                    case Type::CONSTANT: return constant.value;
                    case Type::SCALE: return scale.texture1->evaluate(i) * scale.texture2->evaluate(i);
                    case Type::MIX: return utils::interpolate(mix.texture1->evaluate(i), mix.texture2->evaluate(i), mix.factor->evaluate(i));
                    case Type::CHECKER: return evaluateChecker(i);
                    case Type::PERLIN: return evaluatePerlin(i);
                    case Type::WORLEY: return evaluateWorley(i);
                    case Type::IMAGE: return evaluateImage(i);
                }

                return 0;
            }

        private:
            friend class SpectrumTexture;

            enum class Type { CONSTANT, SCALE, MIX, CHECKER, PERLIN, WORLEY, IMAGE };

            Type type;
            bool isConstant;
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
                if (std::fabs(image.uScale - 1) > constants::EPSILON || std::fabs(image.vScale - 1) > constants::EPSILON || std::fabs(image.uOffset) > constants::EPSILON || std::fabs(image.vOffset) > constants::EPSILON) return average;

                x = ((x % image.width) + image.width) % image.width;
                y = ((y % image.height) + image.height) % image.height;

                return image.image[y * image.width + x];
            }

            LAMBDA_HOST_DEVICE Float evaluateChecker(const Intersection & i) const {
                int x = int(std::floor(i.textureCoordinate[0] * checker.uScale + checker.uOffset));
                int y = int(std::floor(i.textureCoordinate[1] * checker.vScale + checker.vOffset));

                return abs(x + y) % 2 == 0 ? checker.value1 : checker.value2;
            }

            LAMBDA_HOST_DEVICE Float evaluatePerlin(const Intersection & i) const {
                Float value = 0, amplitude = 1, factor = 0;

                Vector<Float, 3> point = i.useTransformed ? i.transformedPoint : i.point;

                point *= perlin.frequency;

                for (int j = 0; j < perlin.octaves; j++) {
                    value += utils::perlinNoise(point) * amplitude;
                    factor += amplitude;

                    amplitude *= perlin.roughness;
                    point *= 2;
                }

                return value / factor;
            }

            LAMBDA_HOST_DEVICE Float evaluateWorley(const Intersection & i) const {
                Float value = 0, amplitude = 1, factor = 0;

                Vector<Float, 3> point = i.useTransformed ? i.transformedPoint : i.point;

                point *= worley.frequency;

                for (int j = 0; j < worley.octaves; j++) {
                    value += utils::worleyNoise(point) * amplitude;
                    factor += amplitude;

                    amplitude *= worley.roughness;
                    point *= 2;
                }

                return value / factor;
            }

            LAMBDA_HOST_DEVICE Float evaluateImage(const Intersection & i) const {
                Float u = i.textureCoordinate[0] * image.uScale + image.uOffset;
                Float v = i.textureCoordinate[1] * image.vScale + image.vOffset;

                u = u - std::floor(u);
                v = v - std::floor(v);

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
                texture.isConstant = true;
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
                texture.isConstant = t->isConstant;
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
                texture.isConstant = t1->isConstant && t2->isConstant;

                int width1 = t1->width;
                int width2 = t2->width;

                int height1 = t1->height;
                int height2 = t2->height;

                int maxWidth = std::max(width1, width2);
                int maxHeight = std::max(height1, height2);

                if (t1->isConstant) width1 = maxWidth;
                if (t2->isConstant) width2 = maxWidth;

                if (t1->isConstant) height1 = maxHeight;
                if (t2->isConstant) height2 = maxHeight;

                texture.width = width1 == width2 ? width1 : 1;
                texture.height = height1 == height2 ? height1 : 1;

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
                texture.isConstant = t1->isConstant && t2->isConstant && factor->isConstant;

                int width1 = t1->width;
                int width2 = t2->width;
                int width3 = factor->width;

                int height1 = t1->height;
                int height2 = t2->height;
                int height3 = factor->height;

                int maxWidth = std::max(width1, std::max(width2, width3));
                int maxHeight = std::max(height1, std::max(height2, height3));

                if (t1->isConstant) width1 = maxWidth;
                if (t2->isConstant) width2 = maxWidth;
                if (factor->isConstant) width3 = maxWidth;

                if (t1->isConstant) height1 = maxHeight;
                if (t2->isConstant) height2 = maxHeight;
                if (factor->isConstant) height3 = maxHeight;

                texture.width = width1 == width2 && width2 == width3 ? width1 : 1;
                texture.height = height1 == height2 && height2 == height3 ? height1 : 1;

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
                texture.isConstant = false;
                texture.width = 1;
                texture.height = 1;
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
                texture.isConstant = false;
                texture.width = (std::fabs(uScale - 1) < constants::EPSILON && std::fabs(vScale - 1) < constants::EPSILON && std::fabs(uOffset) < constants::EPSILON && std::fabs(vOffset) < constants::EPSILON) ? width : 1;
                texture.height = (std::fabs(uScale - 1) < constants::EPSILON && std::fabs(vScale - 1) < constants::EPSILON && std::fabs(uOffset) < constants::EPSILON && std::fabs(vOffset) < constants::EPSILON) ? height : 1;

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

                texture.average /= Float(height);

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

            LAMBDA_HOST_DEVICE int getWidth() const { return width; }

            LAMBDA_HOST_DEVICE int getHeight() const { return height; }

            LAMBDA_HOST_DEVICE Float getMin() const { return min; }

            LAMBDA_HOST_DEVICE Float getMax() const { return max; }

            LAMBDA_HOST_DEVICE Float getAverage() const { return average; }

            LAMBDA_HOST_DEVICE Float getAverage(int x, int y) const {
                switch (type) {
                    case Type::CONSTANT: return average;
                    case Type::SCALAR: return scalar.texture->getAverage(x, y);
                    case Type::SCALE: return scale.texture1->getAverage(x, y) * scale.texture2->getAverage(x, y);
                    case Type::MIX: return utils::interpolate(mix.texture1->getAverage(x, y), mix.texture2->getAverage(x, y), mix.factor->getAverage(x, y));
                    case Type::CHECKER: return average;
                    case Type::IMAGE: return getAverageImage(x, y);
                }

                return 0;
            }

            LAMBDA_HOST_DEVICE_NOINLINE Float evaluate(const Intersection & i, Float lambda) const {
                switch (type) {
                    case Type::CONSTANT: return constant.value->get(lambda);
                    case Type::SCALAR: return scalar.texture->evaluate(i);
                    case Type::SCALE: return scale.texture1->evaluate(i, lambda) * scale.texture2->evaluate(i, lambda);
                    case Type::MIX: return utils::interpolate(mix.texture1->evaluate(i, lambda), mix.texture2->evaluate(i, lambda), mix.factor->evaluate(i));
                    case Type::CHECKER: return evaluateChecker(i, lambda);
                    case Type::IMAGE: return evaluateImage(i, lambda);
                }

                return 0;
            }

        private:
            enum class Type { CONSTANT, SCALAR, SCALE, MIX, CHECKER, IMAGE };

            Type type;
            bool isConstant;
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
                if (std::fabs(image.uScale - 1) > constants::EPSILON || std::fabs(image.vScale - 1) > constants::EPSILON || std::fabs(image.uOffset) > constants::EPSILON || std::fabs(image.vOffset) > constants::EPSILON) return average;

                x = ((x % image.width) + image.width) % image.width;
                y = ((y % image.height) + image.height) % image.height;

                Float sum = 0;

                for (int j = 0; j < 3; j++) sum += image.image[(y * image.width + x) * 3 + j];

                return sum / 3;
            }

            LAMBDA_HOST_DEVICE Float evaluateChecker(const Intersection & i, Float lambda) const {
                int x = int(std::floor(i.textureCoordinate[0] * checker.uScale + checker.uOffset));
                int y = int(std::floor(i.textureCoordinate[1] * checker.vScale + checker.vOffset));

                return abs(x + y) % 2 == 0 ? checker.value1->get(lambda) : checker.value2->get(lambda);
            }

            LAMBDA_HOST_DEVICE Float evaluateImage(const Intersection & i, Float lambda) const {
                Float u = i.textureCoordinate[0] * image.uScale + image.uOffset;
                Float v = i.textureCoordinate[1] * image.vScale + image.vOffset;

                u = u - std::floor(u);
                v = v - std::floor(v);

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

                Vector<Float, 3> color;

                for (int j = 0; j < 3; j++) {
                    Float c00 = image.image[(y0 * image.width + x0) * 3 + j];
                    Float c10 = image.image[(y0 * image.width + x1) * 3 + j];
                    Float c01 = image.image[(y1 * image.width + x0) * 3 + j];
                    Float c11 = image.image[(y1 * image.width + x1) * 3 + j];

                    color[j] = utils::interpolate(utils::interpolate(c00, c10, tx), utils::interpolate(c01, c11, tx), ty);
                }

                Float upsamplingScale = std::fmax(color[0], std::fmax(color[1], color[2]));

                if (upsamplingScale <= 1) upsamplingScale = 1;
                else color /= upsamplingScale;

                Vector<Float, 3> coefficients = utils::upsampleRGB(image.space, color);

                return utils::sigmoid((coefficients[0] * lambda + coefficients[1]) * lambda + coefficients[2]) * upsamplingScale;
            }
    };
}