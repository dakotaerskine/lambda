#ifndef MATRIX_H
#define MATRIX_H

#include <cassert>
#include <complex>

#include "vector.h"

class Matrix {
    public:
        Matrix() {}
        Matrix(const Matrix & m) {
            for (int i = 0; i < 4; i++)
                data[i] = m.data[i];
        }
        Matrix(const std::complex<double> d[4]) {
            for (int i = 0; i < 4; i++)
                data[i] = d[i];
        }
        Matrix & operator=(const Matrix & m) {
            for (int i = 0; i < 4; i++)
                data[i] = m.data[i];

            return (*this);
        }

        std::complex<double> & get(int i, int j) {
            assert (i >= 0 && i < 2 && j >= 0 && j < 2);
            return data[i * 2 + j];
        }
        const std::complex<double> & get(int i, int j) const {
            assert (i >= 0 && i < 2 && j >= 0 && j < 2);
            return data[i * 2 + j];
        }

        friend Matrix operator+(const Matrix & m1, const Matrix & m2) {
            Matrix m3;

            for (int i = 0; i < 4; i++)
                m3.data[i] = m1.data[i] + m2.data[i];

            return m3; 
        }
        friend Matrix operator-(const Matrix & m1, const Matrix & m2) {
            Matrix m3;
            
            for (int i = 0; i < 4; i++)
                m3.data[i] = m1.data[i] - m2.data[i];

            return m3; 
        }
        friend Matrix operator*(const Matrix & m1, const Matrix & m2) {
            Matrix m3;

            m3.data[0] = m1.data[0] * m2.data[0] + m1.data[1] * m2.data[2];
            m3.data[1] = m1.data[0] * m2.data[1] + m1.data[1] * m2.data[3];
            m3.data[2] = m1.data[2] * m2.data[0] + m1.data[3] * m2.data[2];
            m3.data[3] = m1.data[2] * m2.data[1] + m1.data[3] * m2.data[3];

            return m3;
        }
        friend Matrix operator*(const Matrix & m1, const std::complex<double> & d) {
            Matrix m2;
            
            for (int i = 0; i < 4; i++)
                m2.data[i] = m1.data[i] * d;

            return m2; 
        }
        friend Matrix operator*(const std::complex<double> & d, const Matrix & m) { return m * d; }
        Matrix & operator+=(const Matrix & m) {
            for (int i = 0; i < 4; i++)
                data[i] += m.data[i];

            return *this;
        }
        Matrix & operator-=(const Matrix & m) {
            for (int i = 0; i < 4; i++)
                data[i] -= m.data[i];

            return *this;
        }
        Matrix & operator*=(const std::complex<double> & d) {
            for (int i = 0; i < 4; i++)
                data[i] *= d;

            return *this;
        }
        Matrix & operator*=(const Matrix & m) {
            *this = *this * m;
            return *this;
        }
        Matrix & operator/=(const std::complex<double> & d) {
            for (int i = 0; i < 4; i++)
                data[i] /= d;

            return *this;
        }

    private:
        std::complex<double> data[4];
};

#endif