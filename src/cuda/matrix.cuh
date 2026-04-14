#pragma once

#include <thrust/complex.h>

#include "vector.cuh"

class Matrix {
    public:
        __host__ __device__ Matrix() {
            for (int i = 0; i < 4; i++)
                data[i] = thrust::complex<double>(0, 0);
        }
        __host__ __device__ Matrix(const Matrix & m) {
            for (int i = 0; i < 4; i++)
                data[i] = m.data[i];
        }
        __host__ __device__ Matrix(const thrust::complex<double> d[4]) {
            for (int i = 0; i < 4; i++)
                data[i] = d[i];
        }
        __host__ __device__ Matrix & operator=(const Matrix & m) {
            for (int i = 0; i < 4; i++)
                data[i] = m.data[i];

            return (*this);
        }

        __host__ __device__ thrust::complex<double> & get(int i, int j) { return data[i * 2 + j]; }
        __host__ __device__ const thrust::complex<double> & get(int i, int j) const { return data[i * 2 + j]; }

        __host__ __device__ friend Matrix operator+(const Matrix & m1, const Matrix & m2) {
            Matrix m3;

            for (int i = 0; i < 4; i++)
                m3.data[i] = m1.data[i] + m2.data[i];

            return m3; 
        }
        __host__ __device__ friend Matrix operator-(const Matrix & m1, const Matrix & m2) {
            Matrix m3;
            
            for (int i = 0; i < 4; i++)
                m3.data[i] = m1.data[i] - m2.data[i];

            return m3; 
        }
        __host__ __device__ friend Matrix operator*(const Matrix & m1, const Matrix & m2) {
            Matrix m3;

            m3.data[0] = m1.data[0] * m2.data[0] + m1.data[1] * m2.data[2];
            m3.data[1] = m1.data[0] * m2.data[1] + m1.data[1] * m2.data[3];
            m3.data[2] = m1.data[2] * m2.data[0] + m1.data[3] * m2.data[2];
            m3.data[3] = m1.data[2] * m2.data[1] + m1.data[3] * m2.data[3];

            return m3;
        }
        __host__ __device__ friend Matrix operator*(const Matrix & m1, const thrust::complex<double> & d) {
            Matrix m2;
            
            for (int i = 0; i < 4; i++)
                m2.data[i] = m1.data[i] * d;

            return m2; 
        }
        __host__ __device__ friend Matrix operator*(const thrust::complex<double> & d, const Matrix & m) { return m * d; }
        __host__ __device__ Matrix & operator+=(const Matrix & m) {
            for (int i = 0; i < 4; i++)
                data[i] += m.data[i];

            return *this;
        }
        __host__ __device__ Matrix & operator-=(const Matrix & m) {
            for (int i = 0; i < 4; i++)
                data[i] -= m.data[i];

            return *this;
        }
        __host__ __device__ Matrix & operator*=(const thrust::complex<double> & d) {
            for (int i = 0; i < 4; i++)
                data[i] *= d;

            return *this;
        }
        __host__ __device__ Matrix & operator*=(const Matrix & m) {
            *this = *this * m;
            return *this;
        }
        __host__ __device__ Matrix & operator/=(const thrust::complex<double> & d) {
            for (int i = 0; i < 4; i++)
                data[i] /= d;

            return *this;
        }

    private:
        thrust::complex<double> data[4];
};