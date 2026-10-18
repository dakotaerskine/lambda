#pragma once

#include "core/constants.h"
#include "core/platform.h"
#include "core/utils.h"
#include "math/matrix.h"
#include "math/random.h"
#include "math/spectrum.h"
#include "math/vector.h"

namespace lambda {
    class Film {
        public:
            Film() : space(ColorSpace::SRGB), width(0), height(0), lambdaMin(0), lambdaMax(0), wavelengthCDFMin(0), wavelengthCDFMax(0), buffer(nullptr) {}

            Film(ColorSpace _space, int _width, int _height, Float _lambdaMin, Float _lambdaMax, Float * _buffer) : space(_space), width(_width), height(_height), lambdaMin(_lambdaMin), lambdaMax(_lambdaMax), wavelengthCDFMin(std::tanh(constants::WAVELENGTH_PDF_WIDTH * (_lambdaMin - constants::WAVELENGTH_PDF_CENTER))), wavelengthCDFMax(std::tanh(constants::WAVELENGTH_PDF_WIDTH * (_lambdaMax - constants::WAVELENGTH_PDF_CENTER))), buffer(_buffer) {}

            LAMBDA_HOST_DEVICE void setPixel(int i, int j, const Vector<Float, 3> & value) { for (int k = 0; k < 3; k++) buffer[(j * width + i) * 3 + k] = value[k]; }

            LAMBDA_HOST_DEVICE ColorSpace getSpace() const { return space; }

            LAMBDA_HOST_DEVICE int getWidth() const { return width; }

            LAMBDA_HOST_DEVICE int getHeight() const { return height; }

            LAMBDA_HOST_DEVICE Float getLambdaMin() const { return lambdaMin; }

            LAMBDA_HOST_DEVICE Float getLambdaMax() const { return lambdaMax; }

            LAMBDA_HOST_DEVICE Vector<Float, 3> getPixel(int i, int j) const { return Vector<Float, 3>(buffer[(j * width + i) * 3 + 0], buffer[(j * width + i) * 3 + 1], buffer[(j * width + i) * 3 + 2]); }

            LAMBDA_HOST_DEVICE SampledSpectrum sampleLambdas(Random & state) const {
                Float u = utils::randomFloat(state);

                SampledSpectrum lambdas;

                for (int i = 0; i < constants::HERO_COUNT; i++) {
                    Float v = u + Float(i) / constants::HERO_COUNT;

                    if (v >= 1) v -= 1;

                    lambdas[i] = constants::WAVELENGTH_PDF_CENTER + std::atanh(utils::interpolate(wavelengthCDFMin, wavelengthCDFMax, v)) / constants::WAVELENGTH_PDF_WIDTH;
                }

                return lambdas;
            }

            LAMBDA_HOST_DEVICE Vector<Float, 3> toXYZ(const SampledSpectrum & s, const SampledSpectrum & lambdas) const {
                Vector<Float, 3> xyz;

                for (int i = 0; i < constants::HERO_COUNT; i++) {
                    Float lambda = lambdas[i];
                    Float radiance = s[i];

                    Float weight = std::cosh(constants::WAVELENGTH_PDF_WIDTH * (lambda - constants::WAVELENGTH_PDF_CENTER));

                    weight *= weight * (wavelengthCDFMax - wavelengthCDFMin) / (constants::WAVELENGTH_PDF_WIDTH * constants::HERO_COUNT);

                    xyz += Vector<Float, 3>(utils::xyz31(lambda, 0), utils::xyz31(lambda, 1), utils::xyz31(lambda, 2)) * radiance * weight;
                }

                return xyz / constants::CIE_Y_INTEGRAL;
            }

        private:
            ColorSpace space;
            int width, height;
            Float lambdaMin, lambdaMax, wavelengthCDFMin, wavelengthCDFMax;
            Float * buffer;
    };
}