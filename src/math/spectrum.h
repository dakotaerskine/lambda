#pragma once

#include <cassert>

#include "core/constants.h"
#include "core/platform.h"

class SampledSpectrum {
    public:
        HOST_DEVICE SampledSpectrum() : size(0) {}

        HOST_DEVICE SampledSpectrum(int n) : size(n <= MAX_WAVELENGTH_SAMPLES ? n : MAX_WAVELENGTH_SAMPLES) {
            for (int i = 0; i < size; i++)
                data[i] = 0;
        }

        HOST_DEVICE SampledSpectrum(int n, Float d) : size(n <= MAX_WAVELENGTH_SAMPLES ? n : MAX_WAVELENGTH_SAMPLES) {
            for (int i = 0; i < size; i++)
                data[i] = d;
        }

        HOST_DEVICE Float & operator[](int i) {
            assert(i >= 0 && i < size);
            return data[i];
        }

        HOST_DEVICE const Float & operator[](int i) const {
            assert(i >= 0 && i < size);
            return data[i];
        }

        HOST_DEVICE int getSize() const { return size; }

        HOST_DEVICE bool operator==(const SampledSpectrum & s) const {
            if (size != s.size) return false;

            for (int i = 0; i < size; i++)
                if (data[i] != s.data[i]) return false;

            return true;
        }

        HOST_DEVICE bool operator!=(const SampledSpectrum & s) const {
            if (size != s.size) return true;

            for (int i = 0; i < size; i++)
                if (data[i] != s.data[i]) return true;

            return false;
        }

        HOST_DEVICE SampledSpectrum & operator+=(const SampledSpectrum & s) {
            assert(size == s.size);

            for (int i = 0; i < size; i++)
                data[i] += s.data[i];

            return *this;
        }

        HOST_DEVICE SampledSpectrum & operator-=(const SampledSpectrum & s) {
            assert(size == s.size);

            for (int i = 0; i < size; i++)
                data[i] -= s.data[i];

            return *this;
        }

        HOST_DEVICE SampledSpectrum & operator*=(Float d) {
            for (int i = 0; i < size; i++)
                data[i] *= d;

            return *this;
        }

        HOST_DEVICE SampledSpectrum & operator/=(Float d) {
            assert(d != 0);

            for (int i = 0; i < size; i++)
                data[i] /= d;

            return *this;
        }

        HOST_DEVICE friend SampledSpectrum operator+(const SampledSpectrum & s1, const SampledSpectrum & s2) {
            assert(s1.size == s2.size);

            SampledSpectrum s3(s1.size);

            for (int i = 0; i < s1.size; i++)
                s3.data[i] = s1.data[i] + s2.data[i];

            return s3;
        }

        HOST_DEVICE friend SampledSpectrum operator-(const SampledSpectrum & s1) {
            SampledSpectrum s2(s1.size);

            for (int i = 0; i < s1.size; i++)
                s2.data[i] = -s1.data[i];

            return s2;
        }

        HOST_DEVICE friend SampledSpectrum operator-(const SampledSpectrum & s1, const SampledSpectrum & s2) {
            assert(s1.size == s2.size);

            SampledSpectrum s3(s1.size);

            for (int i = 0; i < s1.size; i++)
                s3.data[i] = s1.data[i] - s2.data[i];

            return s3;
        }

        HOST_DEVICE friend SampledSpectrum operator*(const SampledSpectrum & s1, Float d) {
            SampledSpectrum s2(s1.size);

            for (int i = 0; i < s1.size; i++)
                s2.data[i] = s1.data[i] * d;

            return s2;
        }

        HOST_DEVICE friend SampledSpectrum operator*(Float d, const SampledSpectrum & s1) { return s1 * d; }

        HOST_DEVICE friend SampledSpectrum operator*(const SampledSpectrum & s1, const SampledSpectrum & s2) {
            assert(s1.size == s2.size);

            SampledSpectrum s3(s1.size);

            for (int i = 0; i < s1.size; i++)
                s3.data[i] = s1.data[i] * s2.data[i];

            return s3;
        }

        HOST_DEVICE friend SampledSpectrum operator/(const SampledSpectrum & s1, Float d) {
            assert(d != 0);

            SampledSpectrum s2(s1.size);

            for (int i = 0; i < s1.size; i++)
                s2.data[i] = s1.data[i] / d;

            return s2;
        }

    private:
        int size;
        Float data[MAX_WAVELENGTH_SAMPLES];
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

            int min = index;

            if (min == CIE_LAMBDA_BINS - 1) return data[min];

            int max = min + 1;

            return (max - index) * data[min] + (index - min) * data[max];
        }

    private:
        T data[CIE_LAMBDA_BINS];
};