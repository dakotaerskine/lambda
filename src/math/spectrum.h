#pragma once

#include "core/constants.h"
#include "core/platform.h"
#include "math/complex.h"

namespace lambda {
    class SampledSpectrum {
        public:
            LAMBDA_HOST_DEVICE SampledSpectrum(Float d = 0) { for (int i = 0; i < constants::HERO_COUNT; i++) data[i] = d; }

            LAMBDA_HOST_DEVICE Float & operator[](int i) { return data[i]; }

            LAMBDA_HOST_DEVICE const Float & operator[](int i) const { return data[i]; }

            LAMBDA_HOST_DEVICE bool operator==(const SampledSpectrum & s) const {
                for (int i = 0; i < constants::HERO_COUNT; i++) if (data[i] != s.data[i]) return false;

                return true;
            }

            LAMBDA_HOST_DEVICE bool operator!=(const SampledSpectrum & s) const {
                for (int i = 0; i < constants::HERO_COUNT; i++) if (data[i] != s.data[i]) return true;

                return false;
            }

            LAMBDA_HOST_DEVICE SampledSpectrum & operator+=(const SampledSpectrum & s) {
                for (int i = 0; i < constants::HERO_COUNT; i++) data[i] += s.data[i];

                return *this;
            }

            LAMBDA_HOST_DEVICE SampledSpectrum & operator-=(const SampledSpectrum & s) {
                for (int i = 0; i < constants::HERO_COUNT; i++) data[i] -= s.data[i];

                return *this;
            }

            LAMBDA_HOST_DEVICE SampledSpectrum & operator*=(Float d) {
                for (int i = 0; i < constants::HERO_COUNT; i++) data[i] *= d;

                return *this;
            }

            LAMBDA_HOST_DEVICE SampledSpectrum & operator*=(const SampledSpectrum & s) {
                for (int i = 0; i < constants::HERO_COUNT; i++) data[i] *= s.data[i];

                return *this;
            }

            LAMBDA_HOST_DEVICE SampledSpectrum & operator/=(Float d) {
                for (int i = 0; i < constants::HERO_COUNT; i++) data[i] /= d;

                return *this;
            }

            LAMBDA_HOST_DEVICE friend SampledSpectrum operator+(const SampledSpectrum & s1, const SampledSpectrum & s2) {
                SampledSpectrum s3;

                for (int i = 0; i < constants::HERO_COUNT; i++) s3.data[i] = s1.data[i] + s2.data[i];

                return s3;
            }

            LAMBDA_HOST_DEVICE friend SampledSpectrum operator-(const SampledSpectrum & s1) {
                SampledSpectrum s2;

                for (int i = 0; i < constants::HERO_COUNT; i++) s2.data[i] = -s1.data[i];

                return s2;
            }

            LAMBDA_HOST_DEVICE friend SampledSpectrum operator-(const SampledSpectrum & s1, const SampledSpectrum & s2) {
                SampledSpectrum s3;

                for (int i = 0; i < constants::HERO_COUNT; i++) s3.data[i] = s1.data[i] - s2.data[i];

                return s3;
            }

            LAMBDA_HOST_DEVICE friend SampledSpectrum operator*(const SampledSpectrum & s1, Float d) {
                SampledSpectrum s2;

                for (int i = 0; i < constants::HERO_COUNT; i++) s2.data[i] = s1.data[i] * d;

                return s2;
            }

            LAMBDA_HOST_DEVICE friend SampledSpectrum operator*(Float d, const SampledSpectrum & s1) { return s1 * d; }

            LAMBDA_HOST_DEVICE friend SampledSpectrum operator*(const SampledSpectrum & s1, const SampledSpectrum & s2) {
                SampledSpectrum s3;

                for (int i = 0; i < constants::HERO_COUNT; i++) s3.data[i] = s1.data[i] * s2.data[i];

                return s3;
            }

            LAMBDA_HOST_DEVICE friend SampledSpectrum operator/(const SampledSpectrum & s1, Float d) {
                SampledSpectrum s2;

                for (int i = 0; i < constants::HERO_COUNT; i++) s2.data[i] = s1.data[i] / d;

                return s2;
            }

            LAMBDA_HOST_DEVICE Float min() const {
                Float minimum = data[0];

                for (int i = 1; i < constants::HERO_COUNT; i++) if (data[i] < minimum) minimum = data[i];

                return minimum;
            }

            LAMBDA_HOST_DEVICE Float max() const {
                Float maximum = data[0];

                for (int i = 1; i < constants::HERO_COUNT; i++) if (data[i] > maximum) maximum = data[i];

                return maximum;
            }

            LAMBDA_HOST_DEVICE Float average() const {
                Float sum = 0;

                for (int i = 0; i < constants::HERO_COUNT; i++) sum += data[i];

                return sum / constants::HERO_COUNT;
            }

        private:
            Float data[constants::HERO_COUNT];
    };

    template <typename T>
    requires (std::is_same_v<T, Float> || std::is_same_v<T, Complex>)
    class DenseSpectrum {
        public:
            LAMBDA_HOST_DEVICE DenseSpectrum() { for (int i = 0; i < constants::CIE_LAMBDA_BINS; i++) data[i] = 0; }

            LAMBDA_HOST_DEVICE DenseSpectrum(const T & d) { for (int i = 0; i < constants::CIE_LAMBDA_BINS; i++) data[i] = d; }

            LAMBDA_HOST_DEVICE T & operator[](int i) { return data[i]; }

            LAMBDA_HOST_DEVICE const T & operator[](int i) const { return data[i]; }

            LAMBDA_HOST_DEVICE T get(Float lambda) const {
                Float index = lambda - constants::CIE_LAMBDA_MIN;

                int min = int(index);

                if (min == constants::CIE_LAMBDA_BINS - 1) return data[min];

                int max = min + 1;

                return T((Float(max) - index) * data[min] + (index - Float(min)) * data[max]);
            }

            LAMBDA_HOST_DEVICE T min() const {
                T minimum = data[0];

                for (int i = 1; i < constants::CIE_LAMBDA_BINS; i++) if (data[i] < minimum) minimum = data[i];

                return minimum;
            }

            LAMBDA_HOST_DEVICE T max() const {
                T maximum = data[0];

                for (int i = 1; i < constants::CIE_LAMBDA_BINS; i++) if (data[i] > maximum) maximum = data[i];

                return maximum;
            }

            LAMBDA_HOST_DEVICE T average() const {
                T sum = 0;

                for (int i = 0; i < constants::CIE_LAMBDA_BINS; i++) sum += data[i];

                return sum / constants::CIE_LAMBDA_BINS;
            }

        private:
            T data[constants::CIE_LAMBDA_BINS];
    };
}