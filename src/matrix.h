#pragma once

#include <cassert>

#include "platform.h"

class Matrix {
    public:
        HOST_DEVICE Matrix() {
            for (int i = 0; i < 4; i++)
                data[i] = Complex(0, 0);
        }

        HOST_DEVICE Matrix(const Complex d[4]) {
            for (int i = 0; i < 4; i++)
                data[i] = d[i];
        }

        HOST_DEVICE Complex & get(int i, int j) {
            assert(i >= 0 && i < 2 && j >= 0 && j < 2);
            return data[i * 2 + j];
        }

        HOST_DEVICE const Complex & get(int i, int j) const {
            assert(i >= 0 && i < 2 && j >= 0 && j < 2);
            return data[i * 2 + j];
        }

        HOST_DEVICE friend Matrix operator+(const Matrix & m1, const Matrix & m2) {
            Matrix m3;

            for (int i = 0; i < 4; i++)
                m3.data[i] = m1.data[i] + m2.data[i];

            return m3;
        }

        HOST_DEVICE friend Matrix operator-(const Matrix & m1, const Matrix & m2) {
            Matrix m3;

            for (int i = 0; i < 4; i++)
                m3.data[i] = m1.data[i] - m2.data[i];

            return m3;
        }

        HOST_DEVICE friend Matrix operator*(const Matrix & m1, const Matrix & m2) {
            Matrix m3;

            m3.data[0] = m1.data[0] * m2.data[0] + m1.data[1] * m2.data[2];
            m3.data[1] = m1.data[0] * m2.data[1] + m1.data[1] * m2.data[3];
            m3.data[2] = m1.data[2] * m2.data[0] + m1.data[3] * m2.data[2];
            m3.data[3] = m1.data[2] * m2.data[1] + m1.data[3] * m2.data[3];

            return m3;
        }

        HOST_DEVICE friend Matrix operator*(const Matrix & m1, const Complex & d) {
            Matrix m2;

            for (int i = 0; i < 4; i++)
                m2.data[i] = m1.data[i] * d;

            return m2;
        }

        HOST_DEVICE friend Matrix operator*(const Complex & d, const Matrix & m) { return m * d; }

        HOST_DEVICE Matrix & operator+=(const Matrix & m) {
            for (int i = 0; i < 4; i++)
                data[i] += m.data[i];

            return *this;
        }

        HOST_DEVICE Matrix & operator-=(const Matrix & m) {
            for (int i = 0; i < 4; i++)
                data[i] -= m.data[i];

            return *this;
        }

        HOST_DEVICE Matrix & operator*=(const Complex & d) {
            for (int i = 0; i < 4; i++)
                data[i] *= d;

            return *this;
        }

        HOST_DEVICE Matrix & operator*=(const Matrix & m) {
            *this = *this * m;
            return *this;
        }

        HOST_DEVICE Matrix & operator/=(const Complex & d) {
            for (int i = 0; i < 4; i++)
                data[i] /= d;

            return *this;
        }

    private:
        Complex data[4];
};