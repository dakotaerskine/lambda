#pragma once

class Spectrum {
    public:
        __host__ __device__ Spectrum() {
            for (int i = 0; i < 9; i++)
                data[i] = 0;
        }
        __host__ __device__ Spectrum(double d) {
            for (int i = 0; i < 9; i++)
                data[i] = d;
        }
        __host__ __device__ Spectrum(const Spectrum & s) {
            for (int i = 0; i < 9; i++)
                data[i] = s.data[i];
        }
        __host__ __device__ const Spectrum & operator=(const Spectrum & s) {
            for (int i = 0; i < 9; i++)
                data[i] = s.data[i];

            return *this;
        }

        __host__ __device__ double & operator[](int i) { return data[i]; }
        __host__ __device__ const double & operator[](int i) const { return data[i]; }

        __host__ __device__ bool operator==(const Spectrum & s) const {
            for (int i = 0; i < 9; i++)
                if (data[i] != s.data[i]) return false;

            return true;
        }
        __host__ __device__ bool operator!=(const Spectrum & s) const {
            for (int i = 0; i < 9; i++)
                if (data[i] != s.data[i]) return true;

            return false;
        }

        __host__ __device__ Spectrum & operator+=(const Spectrum & s) {
            for (int i = 0; i < 9; i++)
                data[i] += s.data[i];

            return *this;
        }
        __host__ __device__ Spectrum & operator-=(const Spectrum & s) {
            for (int i = 0; i < 9; i++)
                data[i] -= s.data[i];

            return *this;
        }
        __host__ __device__ Spectrum & operator*=(double d) {
            for (int i = 0; i < 9; i++)
                data[i] *= d;

            return *this;
        }
        __host__ __device__ Spectrum & operator/=(double d) {
            for (int i = 0; i < 9; i++)
                data[i] /= d;

            return *this;
        }
        __host__ __device__ friend Spectrum operator+(const Spectrum & s1, const Spectrum & s2) {
            Spectrum s3;
            
            for (int i = 0; i < 9; i++)
                s3.data[i] = s1.data[i] + s2.data[i];

            return s3;
        }
        __host__ __device__ friend Spectrum operator-(const Spectrum & s1) {
            Spectrum s2;
            
            for (int i = 0; i < 9; i++)
                s2.data[i] = -s1.data[i];

            return s2;
        }
        __host__ __device__ friend Spectrum operator-(const Spectrum & s1, const Spectrum & s2) {
            Spectrum s3;
            
            for (int i = 0; i < 9; i++)
                s3.data[i] = s1.data[i] - s2.data[i];

            return s3;
        }
        __host__ __device__ friend Spectrum operator*(const Spectrum & s1, double d) {
            Spectrum s2;
            
            for (int i = 0; i < 9; i++)
                s2.data[i] = s1.data[i] * d;

            return s2;
        }
        __host__ __device__ friend Spectrum operator*(double d, const Spectrum & s1) { return s1 * d; }
        __host__ __device__ friend Spectrum operator*(const Spectrum & s1, const Spectrum & s2) {
            Spectrum s3;
            
            for (int i = 0; i < 9; i++)
                s3.data[i] = s1.data[i] * s2.data[i];

            return s3;
        }
        __host__ __device__ friend Spectrum operator/(const Spectrum & s1, double d) {
            Spectrum s2;
            
            for (int i = 0; i < 9; i++)
                s2.data[i] = s1.data[i] / d;

            return s2;
        }

    private:
        double data[9];
};