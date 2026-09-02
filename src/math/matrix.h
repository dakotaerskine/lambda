#pragma once

#include "core/platform.h"
#include "math/complex.h"

template <typename T, int N>
requires ((std::is_arithmetic_v<T> || std::is_same_v<T, Complex>) && N >= 2 && N <= 4)
class Matrix {
    public:
        HOST_DEVICE Matrix() {
            for (int i = 0; i < N * N; i++)
                elements[i] = T(0);
        }

        HOST_DEVICE Matrix(const T & d0, const T & d1, const T & d2, const T & d3) requires (N == 2) {
            elements[0] = d0;
            elements[1] = d1;
            elements[2] = d2;
            elements[3] = d3;
        }

        HOST_DEVICE Matrix(const T & d0, const T & d1, const T & d2, const T & d3, const T & d4, const T & d5, const T & d6, const T & d7, const T & d8) requires (N == 3) {
            elements[0] = d0;
            elements[1] = d1;
            elements[2] = d2;
            elements[3] = d3;
            elements[4] = d4;
            elements[5] = d5;
            elements[6] = d6;
            elements[7] = d7;
            elements[8] = d8;
        }

        HOST_DEVICE Matrix(const T & d0, const T & d1, const T & d2, const T & d3, const T & d4, const T & d5, const T & d6, const T & d7, const T & d8, const T & d9, const T & d10, const T & d11, const T & d12, const T & d13, const T & d14, const T & d15) requires (N == 4) {
            elements[0] = d0;
            elements[1] = d1;
            elements[2] = d2;
            elements[3] = d3;
            elements[4] = d4;
            elements[5] = d5;
            elements[6] = d6;
            elements[7] = d7;
            elements[8] = d8;
            elements[9] = d9;
            elements[10] = d10;
            elements[11] = d11;
            elements[12] = d12;
            elements[13] = d13;
            elements[14] = d14;
            elements[15] = d15;
        }

        HOST_DEVICE Matrix(const T * d) {
            for (int i = 0; i < N * N; i++)
                elements[i] = d[i];
        }

        HOST_DEVICE Matrix(const T d[N][N]) {
            for (int i = 0; i < N * N; i++)
                elements[i] = d[i / N][i % N];
        }

        HOST_DEVICE T & get(int i, int j) { return elements[i * N + j]; }

        HOST_DEVICE const T & get(int i, int j) const { return elements[i * N + j]; }

        HOST_DEVICE friend Matrix<T, N> operator+(const Matrix<T, N> & m1, const Matrix<T, N> & m2) {
            Matrix<T, N> m3;

            for (int i = 0; i < N * N; i++)
                m3.elements[i] = m1.elements[i] + m2.elements[i];

            return m3;
        }

        HOST_DEVICE friend Matrix<T, N> operator-(const Matrix<T, N> & m1, const Matrix<T, N> & m2) {
            Matrix<T, N> m3;

            for (int i = 0; i < N * N; i++)
                m3.elements[i] = m1.elements[i] - m2.elements[i];

            return m3;
        }

        HOST_DEVICE friend Matrix<T, N> operator*(const Matrix<T, N> & m1, const Matrix<T, N> & m2) requires (N == 2) {
            Matrix<T, N> m3;

            m3.elements[0] = m1.elements[0] * m2.elements[0] + m1.elements[1] * m2.elements[2];
            m3.elements[1] = m1.elements[0] * m2.elements[1] + m1.elements[1] * m2.elements[3];
            m3.elements[2] = m1.elements[2] * m2.elements[0] + m1.elements[3] * m2.elements[2];
            m3.elements[3] = m1.elements[2] * m2.elements[1] + m1.elements[3] * m2.elements[3];

            return m3;
        }

        HOST_DEVICE friend Matrix<T, N> operator*(const Matrix<T, N> & m1, const Matrix<T, N> & m2) requires (N == 3) {
            Matrix<T, N> m3;

            m3.elements[0] = m1.elements[0] * m2.elements[0] + m1.elements[1] * m2.elements[3] + m1.elements[2] * m2.elements[6];
            m3.elements[1] = m1.elements[0] * m2.elements[1] + m1.elements[1] * m2.elements[4] + m1.elements[2] * m2.elements[7];
            m3.elements[2] = m1.elements[0] * m2.elements[2] + m1.elements[1] * m2.elements[5] + m1.elements[2] * m2.elements[8];
            m3.elements[3] = m1.elements[3] * m2.elements[0] + m1.elements[4] * m2.elements[3] + m1.elements[5] * m2.elements[6];
            m3.elements[4] = m1.elements[3] * m2.elements[1] + m1.elements[4] * m2.elements[4] + m1.elements[5] * m2.elements[7];
            m3.elements[5] = m1.elements[3] * m2.elements[2] + m1.elements[4] * m2.elements[5] + m1.elements[5] * m2.elements[8];
            m3.elements[6] = m1.elements[6] * m2.elements[0] + m1.elements[7] * m2.elements[3] + m1.elements[8] * m2.elements[6];
            m3.elements[7] = m1.elements[6] * m2.elements[1] + m1.elements[7] * m2.elements[4] + m1.elements[8] * m2.elements[7];
            m3.elements[8] = m1.elements[6] * m2.elements[2] + m1.elements[7] * m2.elements[5] + m1.elements[8] * m2.elements[8];

            return m3;
        }

        HOST_DEVICE friend Matrix<T, N> operator*(const Matrix<T, N> & m1, const Matrix<T, N> & m2) requires (N == 4) {
            Matrix<T, N> m3;

            m3.elements[0] = m1.elements[0] * m2.elements[0] + m1.elements[1] * m2.elements[4] + m1.elements[2] * m2.elements[8] + m1.elements[3] * m2.elements[12];
            m3.elements[1] = m1.elements[0] * m2.elements[1] + m1.elements[1] * m2.elements[5] + m1.elements[2] * m2.elements[9] + m1.elements[3] * m2.elements[13];
            m3.elements[2] = m1.elements[0] * m2.elements[2] + m1.elements[1] * m2.elements[6] + m1.elements[2] * m2.elements[10] + m1.elements[3] * m2.elements[14];
            m3.elements[3] = m1.elements[0] * m2.elements[3] + m1.elements[1] * m2.elements[7] + m1.elements[2] * m2.elements[11] + m1.elements[3] * m2.elements[15];
            m3.elements[4] = m1.elements[4] * m2.elements[0] + m1.elements[5] * m2.elements[4] + m1.elements[6] * m2.elements[8] + m1.elements[7] * m2.elements[12];
            m3.elements[5] = m1.elements[4] * m2.elements[1] + m1.elements[5] * m2.elements[5] + m1.elements[6] * m2.elements[9] + m1.elements[7] * m2.elements[13];
            m3.elements[6] = m1.elements[4] * m2.elements[2] + m1.elements[5] * m2.elements[6] + m1.elements[6] * m2.elements[10] + m1.elements[7] * m2.elements[14];
            m3.elements[7] = m1.elements[4] * m2.elements[3] + m1.elements[5] * m2.elements[7] + m1.elements[6] * m2.elements[11] + m1.elements[7] * m2.elements[15];
            m3.elements[8] = m1.elements[8] * m2.elements[0] + m1.elements[9] * m2.elements[4] + m1.elements[10] * m2.elements[8] + m1.elements[11] * m2.elements[12];
            m3.elements[9] = m1.elements[8] * m2.elements[1] + m1.elements[9] * m2.elements[5] + m1.elements[10] * m2.elements[9] + m1.elements[11] * m2.elements[13];
            m3.elements[10] = m1.elements[8] * m2.elements[2] + m1.elements[9] * m2.elements[6] + m1.elements[10] * m2.elements[10] + m1.elements[11] * m2.elements[14];
            m3.elements[11] = m1.elements[8] * m2.elements[3] + m1.elements[9] * m2.elements[7] + m1.elements[10] * m2.elements[11] + m1.elements[11] * m2.elements[15];
            m3.elements[12] = m1.elements[12] * m2.elements[0] + m1.elements[13] * m2.elements[4] + m1.elements[14] * m2.elements[8] + m1.elements[15] * m2.elements[12];
            m3.elements[13] = m1.elements[12] * m2.elements[1] + m1.elements[13] * m2.elements[5] + m1.elements[14] * m2.elements[9] + m1.elements[15] * m2.elements[13];
            m3.elements[14] = m1.elements[12] * m2.elements[2] + m1.elements[13] * m2.elements[6] + m1.elements[14] * m2.elements[10] + m1.elements[15] * m2.elements[14];
            m3.elements[15] = m1.elements[12] * m2.elements[3] + m1.elements[13] * m2.elements[7] + m1.elements[14] * m2.elements[11] + m1.elements[15] * m2.elements[15];

            return m3;
        }

        HOST_DEVICE friend Matrix<T, N> operator*(const Matrix<T, N> & m1, const T & d) {
            Matrix<T, N> m2;

            for (int i = 0; i < N * N; i++)
                m2.elements[i] = m1.elements[i] * d;

            return m2;
        }

        HOST_DEVICE friend Matrix<T, N> operator*(const T & d, const Matrix<T, N> & m) { return m * d; }

        HOST_DEVICE friend Matrix<T, N> operator/(const Matrix<T, N> & m1, const T & d) {
            Matrix<T, N> m2;

            for (int i = 0; i < N * N; i++)
                m2.elements[i] = m1.elements[i] / d;

            return m2;
        }

        HOST_DEVICE Matrix<T, N> & operator+=(const Matrix<T, N> & m) {
            for (int i = 0; i < N * N; i++)
                elements[i] += m.elements[i];

            return *this;
        }

        HOST_DEVICE Matrix<T, N> & operator-=(const Matrix<T, N> & m) {
            for (int i = 0; i < N * N; i++)
                elements[i] -= m.elements[i];

            return *this;
        }

        HOST_DEVICE Matrix<T, N> & operator*=(const T & d) {
            for (int i = 0; i < N * N; i++)
                elements[i] *= d;

            return *this;
        }

        HOST_DEVICE Matrix<T, N> & operator*=(const Matrix<T, N> & m) {
            *this = *this * m;
            return *this;
        }

        HOST_DEVICE Matrix<T, N> & operator/=(const T & d) {
            for (int i = 0; i < N * N; i++)
                elements[i] /= d;

            return *this;
        }

        HOST_DEVICE T determinant() const requires (N == 2) { return elements[0] * elements[3] - elements[1] * elements[2]; }

        HOST_DEVICE T determinant() const requires (N == 3) { return elements[0] * (elements[4] * elements[8] - elements[5] * elements[7]) - elements[1] * (elements[3] * elements[8] - elements[5] * elements[6]) + elements[2] * (elements[3] * elements[7] - elements[4] * elements[6]); }

        HOST_DEVICE T determinant() const requires (N == 4) { return elements[0] * (elements[5] * (elements[10] * elements[15] - elements[11] * elements[14]) - elements[6] * (elements[9] * elements[15] - elements[11] * elements[13]) + elements[7] * (elements[9] * elements[14] - elements[10] * elements[13])) - elements[1] * (elements[4] * (elements[10] * elements[15] - elements[11] * elements[14]) - elements[6] * (elements[8] * elements[15] - elements[11] * elements[12]) + elements[7] * (elements[8] * elements[14] - elements[10] * elements[12])) + elements[2] * (elements[4] * (elements[9] * elements[15] - elements[11] * elements[13]) - elements[5] * (elements[8] * elements[15] - elements[11] * elements[12]) + elements[7] * (elements[8] * elements[13] - elements[9] * elements[12])) - elements[3] * (elements[4] * (elements[9] * elements[14] - elements[10] * elements[13]) - elements[5] * (elements[8] * elements[14] - elements[10] * elements[12]) + elements[6] * (elements[8] * elements[13] - elements[9] * elements[12])); }

        HOST_DEVICE friend Matrix<T, N> transpose(const Matrix<T, N> & m1) {
            Matrix<T, N> m2;

            for (int i = 0; i < N; i++)
                for (int j = 0; j < N; j++)
                    m2.elements[i * N + j] = m1.elements[j * N + i];

            return m2;
        }

        HOST_DEVICE friend Matrix<T, N> inverse(const Matrix<T, N> & m) requires (N == 2) {
            T det = m.determinant();

            return Matrix<T, N>(m.elements[3] / det, -m.elements[1] / det, -m.elements[2] / det, m.elements[0] / det);
        }

        HOST_DEVICE friend Matrix<T, N> inverse(const Matrix<T, N> & m) requires (N == 3) {
            T det = m.determinant();

            return Matrix<T, N>((m.elements[4] * m.elements[8] - m.elements[5] * m.elements[7]) / det, (m.elements[2] * m.elements[7] - m.elements[1] * m.elements[8]) / det, (m.elements[1] * m.elements[5] - m.elements[2] * m.elements[4]) / det, (m.elements[5] * m.elements[6] - m.elements[3] * m.elements[8]) / det, (m.elements[0] * m.elements[8] - m.elements[2] * m.elements[6]) / det, (m.elements[2] * m.elements[3] - m.elements[0] * m.elements[5]) / det, (m.elements[3] * m.elements[7] - m.elements[4] * m.elements[6]) / det, (m.elements[1] * m.elements[6] - m.elements[0] * m.elements[7]) / det, (m.elements[0] * m.elements[4] - m.elements[1] * m.elements[3]) / det);
        }

        HOST_DEVICE friend Matrix<T, N> inverse(const Matrix<T, N> & m) requires (N == 4) {
            T det = m.determinant();

            T s0 = m.elements[0] * m.elements[5] - m.elements[1] * m.elements[4];
            T s1 = m.elements[0] * m.elements[6] - m.elements[2] * m.elements[4];
            T s2 = m.elements[0] * m.elements[7] - m.elements[3] * m.elements[4];
            T s3 = m.elements[1] * m.elements[6] - m.elements[2] * m.elements[5];
            T s4 = m.elements[1] * m.elements[7] - m.elements[3] * m.elements[5];
            T s5 = m.elements[2] * m.elements[7] - m.elements[3] * m.elements[6];

            T c0 = m.elements[8] * m.elements[13] - m.elements[9] * m.elements[12];
            T c1 = m.elements[8] * m.elements[14] - m.elements[10] * m.elements[12];
            T c2 = m.elements[8] * m.elements[15] - m.elements[11] * m.elements[12];
            T c3 = m.elements[9] * m.elements[14] - m.elements[10] * m.elements[13];
            T c4 = m.elements[9] * m.elements[15] - m.elements[11] * m.elements[13];
            T c5 = m.elements[10] * m.elements[15] - m.elements[11] * m.elements[14];

            return Matrix<T, N>((m.elements[5] * c5 - m.elements[6] * c4 + m.elements[7] * c3) / det, (-m.elements[1] * c5 + m.elements[2] * c4 - m.elements[3] * c3) / det, (m.elements[13] * s5 - m.elements[14] * s4 + m.elements[15] * s3) / det, (-m.elements[9] * s5 + m.elements[10] * s4 - m.elements[11] * s3) / det, (-m.elements[4] * c5 + m.elements[6] * c2 - m.elements[7] * c1) / det, (m.elements[0] * c5 - m.elements[2] * c2 + m.elements[3] * c1) / det, (-m.elements[12] * s5 + m.elements[14] * s2 - m.elements[15] * s1) / det, (m.elements[8] * s5 - m.elements[10] * s2 + m.elements[11] * s1) / det, (m.elements[4] * c4 - m.elements[5] * c2 + m.elements[7] * c0) / det, (-m.elements[0] * c4 + m.elements[1] * c2 - m.elements[3] * c0) / det, (m.elements[12] * s4 - m.elements[13] * s2 + m.elements[15] * s0) / det, (-m.elements[8] * s4 + m.elements[9] * s2 - m.elements[11] * s0) / det, (-m.elements[4] * c3 + m.elements[5] * c1 - m.elements[6] * c0) / det, (m.elements[0] * c3 - m.elements[1] * c1 + m.elements[2] * c0) / det, (-m.elements[12] * s3 + m.elements[13] * s1 - m.elements[14] * s0) / det, (m.elements[8] * s3 - m.elements[9] * s1 + m.elements[10] * s0) / det);
        }

        HOST_DEVICE T * data() { return elements; }

        HOST_DEVICE const T * data() const { return elements; }

    private:
        T elements[N * N];
};