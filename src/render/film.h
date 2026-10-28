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
            Film() : space(ColorSpace::SRGB), width(0), height(0), lambdaMin(0), lambdaMax(0), visibleMin(0), visibleMax(0), visibleWeight(0), wavelengthCDFMin(0), wavelengthCDFMax(0), buffer(nullptr) {}

            Film(ColorSpace _space, int _width, int _height, Float _lambdaMin, Float _lambdaMax, Float * _buffer) : space(_space), width(_width), height(_height), lambdaMin(_lambdaMin), lambdaMax(_lambdaMax), visibleMin(std::fmax(_lambdaMin, Float(constants::CIE_LAMBDA_MIN))), visibleMax(std::fmin(_lambdaMax, Float(constants::CIE_LAMBDA_MAX))), buffer(_buffer) {
                visibleWeight = std::fmax(visibleMax - visibleMin, Float(0)) / (_lambdaMax - _lambdaMin);

                wavelengthCDFMin = std::tanh(constants::WAVELENGTH_PDF_WIDTH * (visibleMin - constants::WAVELENGTH_PDF_CENTER));
                wavelengthCDFMax = std::tanh(constants::WAVELENGTH_PDF_WIDTH * (visibleMax - constants::WAVELENGTH_PDF_CENTER));
            }

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

                    if (v < visibleWeight) lambdas[i] = utils::clamp(constants::WAVELENGTH_PDF_CENTER + std::atanh(utils::clamp(utils::interpolate(wavelengthCDFMin, wavelengthCDFMax, v / visibleWeight), -constants::NEXT_AFTER, constants::NEXT_AFTER)) / constants::WAVELENGTH_PDF_WIDTH, std::fmax(lambdaMin, Float(constants::CIE_LAMBDA_MIN)), std::fmin(lambdaMax, Float(constants::CIE_LAMBDA_MAX)));
                    else {
                        lambdas[i] = utils::interpolate(lambdaMin, lambdaMax - (visibleMax - visibleMin), (v - visibleWeight) / (1 - visibleWeight));

                        if (lambdas[i] >= visibleMin) lambdas[i] += visibleMax - visibleMin;
                    }
                }

                return lambdas;
            }

            LAMBDA_HOST_DEVICE Vector<Float, 3> toXYZ(const SampledSpectrum & s, const SampledSpectrum & lambdas) const {
                Vector<Float, 3> xyz;

                for (int i = 0; i < constants::HERO_COUNT; i++) {
                    Float lambda = lambdas[i];
                    Float radiance = s[i];

                    Float probabilityDensity = 0;

                    if (lambda >= visibleMin && lambda <= visibleMax) {
                        probabilityDensity = 1 / std::cosh(constants::WAVELENGTH_PDF_WIDTH * (lambda - constants::WAVELENGTH_PDF_CENTER));

                        probabilityDensity *= visibleWeight * probabilityDensity * constants::WAVELENGTH_PDF_WIDTH / (wavelengthCDFMax - wavelengthCDFMin);
                    }
                    else probabilityDensity = (1 - visibleWeight) / ((lambdaMax - lambdaMin) - (visibleMax - visibleMin));

                    xyz += Vector<Float, 3>(utils::xyz31(lambda, 0), utils::xyz31(lambda, 1), utils::xyz31(lambda, 2)) * radiance / (constants::HERO_COUNT * probabilityDensity);
                }

                return xyz / constants::CIE_Y_INTEGRAL;
            }

        private:
            ColorSpace space;
            int width, height;
            Float lambdaMin, lambdaMax, visibleMin, visibleMax, visibleWeight, wavelengthCDFMin, wavelengthCDFMax;
            Float * buffer;
    };
}