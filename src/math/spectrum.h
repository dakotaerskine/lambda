#pragma once

#include "core/constants.h"
#include "core/platform.h"
#include "math/complex.h"

namespace lambda {
    template <typename T>
    requires (std::is_same_v<T, Float> || std::is_same_v<T, Complex>)
    class LinearSpectrum {
        public:
            LAMBDA_HOST_DEVICE LinearSpectrum(int _numSamples = 0, Float * _lambdas = nullptr, T * _values = nullptr) : numSamples(_numSamples), lambdas(_lambdas), values(_values) {}

            LAMBDA_HOST_DEVICE T operator[](Float lambda) const {
                if (numSamples <= 0 || !lambdas || !values || lambda < lambdas[0] || lambda > lambdas[numSamples - 1]) return T(0);
                else if (numSamples == 1) return values[0];

                int min = 0, max = numSamples - 1;

                while (max - min > 1) {
                    int mid = (min + max) / 2;

                    if (lambdas[mid] < lambda) min = mid;
                    else max = mid;
                }

                Float t = (lambda - lambdas[min]) / (lambdas[max] - lambdas[min]);

                return T((1 - t) * values[min] + t * values[max]);
            }

            LAMBDA_HOST_DEVICE bool contains(Float lambda) const { return numSamples > 0 && lambdas && values && lambda >= lambdas[0] && lambda <= lambdas[numSamples - 1]; }

            LAMBDA_HOST_DEVICE T min() const {
                using std::fmin;

                if (numSamples <= 0 || !lambdas || !values) return T(0);

                T minimum = values[0];

                for (int i = 1; i < numSamples; i++)
                    minimum = fmin(minimum, values[i]);

                return minimum;
            }

            LAMBDA_HOST_DEVICE T max() const {
                using std::fmax;

                if (numSamples <= 0 || !lambdas || !values) return T(0);

                T maximum = values[0];

                for (int i = 1; i < numSamples; i++)
                    maximum = fmax(maximum, values[i]);

                return maximum;
            }

            LAMBDA_HOST_DEVICE T average() const {
                if (numSamples <= 1 || !lambdas || !values) return T(0);

                T sum = 0;

                for (int i = 0; i < numSamples - 1; i++)
                    sum += Float(0.5) * (lambdas[i + 1] - lambdas[i]) * (values[i] + values[i + 1]);

                return sum / (lambdas[numSamples - 1] - lambdas[0]);
            }

        private:
            int numSamples;
            Float * lambdas;
            T * values;
    };

    template <typename T>
    requires (std::is_same_v<T, Float> || std::is_same_v<T, Complex>)
    class SampledSpectrum {
        public:
            LAMBDA_HOST_DEVICE SampledSpectrum(T d = 0) { for (int i = 0; i < constants::HERO_COUNT; i++) data[i] = d; }

            LAMBDA_HOST_DEVICE T & operator[](int i) { return data[i]; }

            LAMBDA_HOST_DEVICE const T & operator[](int i) const { return data[i]; }

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

            LAMBDA_HOST_DEVICE SampledSpectrum & operator*=(T d) {
                for (int i = 0; i < constants::HERO_COUNT; i++) data[i] *= d;

                return *this;
            }

            LAMBDA_HOST_DEVICE SampledSpectrum & operator*=(const SampledSpectrum & s) {
                for (int i = 0; i < constants::HERO_COUNT; i++) data[i] *= s.data[i];

                return *this;
            }

            LAMBDA_HOST_DEVICE SampledSpectrum & operator/=(T d) {
                for (int i = 0; i < constants::HERO_COUNT; i++) data[i] /= d;

                return *this;
            }

            LAMBDA_HOST_DEVICE SampledSpectrum & operator/=(const SampledSpectrum & s) {
                for (int i = 0; i < constants::HERO_COUNT; i++) data[i] /= s.data[i];

                return *this;
            }

            LAMBDA_HOST_DEVICE friend SampledSpectrum<T> operator+(const SampledSpectrum<T> & s1, const SampledSpectrum<T> & s2) {
                SampledSpectrum<T> s3;

                for (int i = 0; i < constants::HERO_COUNT; i++) s3.data[i] = s1.data[i] + s2.data[i];

                return s3;
            }

            LAMBDA_HOST_DEVICE friend SampledSpectrum<T> operator-(const SampledSpectrum<T> & s1) {
                SampledSpectrum<T> s2;

                for (int i = 0; i < constants::HERO_COUNT; i++) s2.data[i] = -s1.data[i];

                return s2;
            }

            LAMBDA_HOST_DEVICE friend SampledSpectrum<T> operator-(const SampledSpectrum<T> & s1, const SampledSpectrum<T> & s2) {
                SampledSpectrum<T> s3;

                for (int i = 0; i < constants::HERO_COUNT; i++) s3.data[i] = s1.data[i] - s2.data[i];

                return s3;
            }

            LAMBDA_HOST_DEVICE friend SampledSpectrum<T> operator*(const SampledSpectrum<T> & s1, T d) {
                SampledSpectrum<T> s2;

                for (int i = 0; i < constants::HERO_COUNT; i++) s2.data[i] = s1.data[i] * d;

                return s2;
            }

            LAMBDA_HOST_DEVICE friend SampledSpectrum<T> operator*(T d, const SampledSpectrum<T> & s1) { return s1 * d; }

            LAMBDA_HOST_DEVICE friend SampledSpectrum<T> operator*(const SampledSpectrum<T> & s1, const SampledSpectrum<T> & s2) {
                SampledSpectrum<T> s3;

                for (int i = 0; i < constants::HERO_COUNT; i++) s3.data[i] = s1.data[i] * s2.data[i];

                return s3;
            }

            LAMBDA_HOST_DEVICE friend SampledSpectrum<T> operator/(const SampledSpectrum<T> & s1, T d) {
                SampledSpectrum<T> s2;

                for (int i = 0; i < constants::HERO_COUNT; i++) s2.data[i] = s1.data[i] / d;

                return s2;
            }

            LAMBDA_HOST_DEVICE friend SampledSpectrum<T> operator/(const SampledSpectrum<T> & s1, const SampledSpectrum<T> & s2) {
                SampledSpectrum<T> s3;

                for (int i = 0; i < constants::HERO_COUNT; i++) s3.data[i] = s1.data[i] / s2.data[i];

                return s3;
            }

            LAMBDA_HOST_DEVICE T min() const {
                using std::fmin;

                T minimum = data[0];

                for (int i = 1; i < constants::HERO_COUNT; i++) minimum = fmin(minimum, data[i]);

                return minimum;
            }

            LAMBDA_HOST_DEVICE T max() const {
                using std::fmax;

                T maximum = data[0];

                for (int i = 1; i < constants::HERO_COUNT; i++) maximum = fmax(maximum, data[i]);

                return maximum;
            }

            LAMBDA_HOST_DEVICE T average() const {
                T sum = 0;

                for (int i = 0; i < constants::HERO_COUNT; i++) sum += data[i];

                return sum / constants::HERO_COUNT;
            }

        private:
            T data[constants::HERO_COUNT];
    };
}