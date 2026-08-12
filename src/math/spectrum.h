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
                if (fabsF(data[i] - s.data[i]) >= EPSILON) return false;

            return true;
        }

        HOST_DEVICE bool operator!=(const SampledSpectrum & s) const {
            for (int i = 0; i < HERO_COUNT; i++)
                if (fabsF(data[i] - s.data[i]) >= EPSILON) return true;

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
            assert(d != 0);

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
            assert(d != 0);

            SampledSpectrum s2;

            for (int i = 0; i < HERO_COUNT; i++)
                s2.data[i] = s1.data[i] / d;

            return s2;
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

            double index = lambda - CIE_LAMBDA_MIN;

            int min = int(index);

            if (min == CIE_LAMBDA_BINS - 1) return data[min];

            int max = min + 1;

            return T((max - index) * data[min] + (index - min) * data[max]);
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