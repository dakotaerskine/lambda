#pragma once

#include <cassert>

#include "constants.h"
#include "platform.h"

class Spectrum {
    public:
        HOST_DEVICE Spectrum() : size(0) {}

        HOST_DEVICE Spectrum(int n) : size(n <= MAX_WAVELENGTH_SAMPLES ? n : MAX_WAVELENGTH_SAMPLES) {
            for (int i = 0; i < size; i++)
                data[i] = 0;
        }

        HOST_DEVICE Spectrum(int n, Float d) : size(n <= MAX_WAVELENGTH_SAMPLES ? n : MAX_WAVELENGTH_SAMPLES) {
            for (int i = 0; i < size; i++)
                data[i] = d;
        }

        HOST_DEVICE Spectrum(const Spectrum & s) {
            size = s.size;

            for (int i = 0; i < size; i++)
                data[i] = s.data[i];
        }

        HOST_DEVICE Spectrum & operator=(const Spectrum & s) {
            size = s.size;

            for (int i = 0; i < size; i++)
                data[i] = s.data[i];

            return *this;
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

        HOST_DEVICE bool operator==(const Spectrum & s) const {
            if (size != s.size) return false;

            for (int i = 0; i < size; i++)
                if (data[i] != s.data[i]) return false;

            return true;
        }

        HOST_DEVICE bool operator!=(const Spectrum & s) const {
            if (size != s.size) return true;

            for (int i = 0; i < size; i++)
                if (data[i] != s.data[i]) return true;

            return false;
        }

        HOST_DEVICE Spectrum & operator+=(const Spectrum & s) {
            assert(size == s.size);

            for (int i = 0; i < size; i++)
                data[i] += s.data[i];

            return *this;
        }

        HOST_DEVICE Spectrum & operator-=(const Spectrum & s) {
            assert(size == s.size);

            for (int i = 0; i < size; i++)
                data[i] -= s.data[i];

            return *this;
        }

        HOST_DEVICE Spectrum & operator*=(Float d) {
            for (int i = 0; i < size; i++)
                data[i] *= d;

            return *this;
        }

        HOST_DEVICE Spectrum & operator/=(Float d) {
            assert(d != 0);

            for (int i = 0; i < size; i++)
                data[i] /= d;

            return *this;
        }

        HOST_DEVICE friend Spectrum operator+(const Spectrum & s1, const Spectrum & s2) {
            assert(s1.size == s2.size);

            Spectrum s3(s1.size);

            for (int i = 0; i < s1.size; i++)
                s3.data[i] = s1.data[i] + s2.data[i];

            return s3;
        }

        HOST_DEVICE friend Spectrum operator-(const Spectrum & s1) {
            Spectrum s2(s1.size);

            for (int i = 0; i < s1.size; i++)
                s2.data[i] = -s1.data[i];

            return s2;
        }

        HOST_DEVICE friend Spectrum operator-(const Spectrum & s1, const Spectrum & s2) {
            assert(s1.size == s2.size);

            Spectrum s3(s1.size);

            for (int i = 0; i < s1.size; i++)
                s3.data[i] = s1.data[i] - s2.data[i];

            return s3;
        }

        HOST_DEVICE friend Spectrum operator*(const Spectrum & s1, Float d) {
            Spectrum s2(s1.size);

            for (int i = 0; i < s1.size; i++)
                s2.data[i] = s1.data[i] * d;

            return s2;
        }

        HOST_DEVICE friend Spectrum operator*(Float d, const Spectrum & s1) { return s1 * d; }

        HOST_DEVICE friend Spectrum operator*(const Spectrum & s1, const Spectrum & s2) {
            assert(s1.size == s2.size);

            Spectrum s3(s1.size);

            for (int i = 0; i < s1.size; i++)
                s3.data[i] = s1.data[i] * s2.data[i];

            return s3;
        }
        
        HOST_DEVICE friend Spectrum operator/(const Spectrum & s1, Float d) {
            assert(d != 0);

            Spectrum s2(s1.size);

            for (int i = 0; i < s1.size; i++)
                s2.data[i] = s1.data[i] / d;

            return s2;
        }

    private:
        int size;
        Float data[MAX_WAVELENGTH_SAMPLES];
};