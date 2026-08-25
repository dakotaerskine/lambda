#pragma once

#include <cassert>

#include "core/constants.h"
#include "core/platform.h"

class SampledSpectrum {
    public:
        HOST_DEVICE SampledSpectrum() {}

        HOST_DEVICE SampledSpectrum(Float d) {
            for (int i = 0; i < HERO_COUNT; i++)
                data[i] = d;
        }

        HOST_DEVICE Float & operator[](int i) {
            assert(i >= 0 && i < HERO_COUNT);
            return data[i];
        }

        HOST_DEVICE const Float & operator[](int i) const {
            assert(i >= 0 && i < HERO_COUNT);
            return data[i];
        }

        HOST_DEVICE bool operator==(const SampledSpectrum & s) const {
            for (int i = 0; i < HERO_COUNT; i++)
                if (fabs(data[i] - s.data[i]) >= EPSILON) return false;

            return true;
        }

        HOST_DEVICE bool operator!=(const SampledSpectrum & s) const {
            for (int i = 0; i < HERO_COUNT; i++)
                if (fabs(data[i] - s.data[i]) >= EPSILON) return true;

            return false;
        }

        HOST_DEVICE SampledSpectrum & operator+=(const SampledSpectrum & s) {
            for (int i = 0; i < HERO_COUNT; i++)
                data[i] += s.data[i];

            return *this;
        }

        HOST_DEVICE SampledSpectrum & operator-=(const SampledSpectrum & s) {
            for (int i = 0; i < HERO_COUNT; i++)
                data[i] -= s.data[i];

            return *this;
        }

        HOST_DEVICE SampledSpectrum & operator*=(Float d) {
            for (int i = 0; i < HERO_COUNT; i++)
                data[i] *= d;

            return *this;
        }

        HOST_DEVICE SampledSpectrum & operator*=(const SampledSpectrum & s) {
            for (int i = 0; i < HERO_COUNT; i++)
                data[i] *= s.data[i];

            return *this;
        }

        HOST_DEVICE SampledSpectrum & operator/=(Float d) {
            assert(fabs(d) > EPSILON);

            for (int i = 0; i < HERO_COUNT; i++)
                data[i] /= d;

            return *this;
        }

        HOST_DEVICE friend SampledSpectrum operator+(const SampledSpectrum & s1, const SampledSpectrum & s2) {
            SampledSpectrum s3;

            for (int i = 0; i < HERO_COUNT; i++)
                s3.data[i] = s1.data[i] + s2.data[i];

            return s3;
        }

        HOST_DEVICE friend SampledSpectrum operator-(const SampledSpectrum & s1) {
            SampledSpectrum s2;

            for (int i = 0; i < HERO_COUNT; i++)
                s2.data[i] = -s1.data[i];

            return s2;
        }

        HOST_DEVICE friend SampledSpectrum operator-(const SampledSpectrum & s1, const SampledSpectrum & s2) {
            SampledSpectrum s3;

            for (int i = 0; i < HERO_COUNT; i++)
                s3.data[i] = s1.data[i] - s2.data[i];

            return s3;
        }

        HOST_DEVICE friend SampledSpectrum operator*(const SampledSpectrum & s1, Float d) {
            SampledSpectrum s2;

            for (int i = 0; i < HERO_COUNT; i++)
                s2.data[i] = s1.data[i] * d;

            return s2;
        }

        HOST_DEVICE friend SampledSpectrum operator*(Float d, const SampledSpectrum & s1) { return s1 * d; }

        HOST_DEVICE friend SampledSpectrum operator*(const SampledSpectrum & s1, const SampledSpectrum & s2) {
            SampledSpectrum s3;

            for (int i = 0; i < HERO_COUNT; i++)
                s3.data[i] = s1.data[i] * s2.data[i];

            return s3;
        }

        HOST_DEVICE friend SampledSpectrum operator/(const SampledSpectrum & s1, Float d) {
            assert(fabs(d) > EPSILON);

            SampledSpectrum s2;

            for (int i = 0; i < HERO_COUNT; i++)
                s2.data[i] = s1.data[i] / d;

            return s2;
        }

        HOST_DEVICE Float max() const {
            Float maximum = data[0];

            for (int i = 1; i < HERO_COUNT; i++)
                if (data[i] > maximum) maximum = data[i];

            return maximum;
        }

        HOST_DEVICE Float average() const {
            Float sum = 0;

            for (int i = 0; i < HERO_COUNT; i++)
                sum += data[i];

            return sum / HERO_COUNT;
        }

    private:
        Float data[HERO_COUNT];
};

template <typename T>
class DenseSpectrum {
    public:
        HOST_DEVICE DenseSpectrum() {
            for (int i = 0; i < CIE_LAMBDA_BINS; i++)
                data[i] = 0;
        }

        HOST_DEVICE DenseSpectrum(const T & d) {
            for (int i = 0; i < CIE_LAMBDA_BINS; i++)
                data[i] = d;
        }

        HOST_DEVICE T & operator[](int i) {
            assert(i >= 0 && i < CIE_LAMBDA_BINS);
            return data[i];
        }

        HOST_DEVICE T operator()(Float lambda) const {
            assert(lambda >= CIE_LAMBDA_MIN && lambda < CIE_LAMBDA_MAX);

            Float index = lambda - CIE_LAMBDA_MIN;

            int min = int(index);

            if (min == CIE_LAMBDA_BINS - 1) return data[min];

            int max = min + 1;

            if constexpr (std::is_same_v<T, Complex>) return T(double(max - index) * data[min] + double(index - min) * data[max]);
            else return T((max - index) * data[min] + (index - min) * data[max]);

            return T(0);
        }

        HOST_DEVICE T min() const {
            T minimum = data[0];

            for (int i = 1; i < CIE_LAMBDA_BINS; i++)
                if (data[i] < minimum) minimum = data[i];

            return minimum;
        }

        HOST_DEVICE T max() const {
            T maximum = data[0];

            for (int i = 1; i < CIE_LAMBDA_BINS; i++)
                if (data[i] > maximum) maximum = data[i];

            return maximum;
        }

        HOST_DEVICE T average() const {
            T sum = 0;

            for (int i = 0; i < CIE_LAMBDA_BINS; i++)
                sum += data[i];

            return sum / CIE_LAMBDA_BINS;
        }

    private:
        T data[CIE_LAMBDA_BINS];
};