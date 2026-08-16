#pragma once

#include <cassert>

#include "core/platform.h"
#include "math/vector.h"

template <typename T>
class Matrix2 {
    public:
        HOST_DEVICE Matrix2() {
            for (int i = 0; i < 4; i++)
                data[i] = T(0);
        }

        HOST_DEVICE Matrix2(const T & d0, const T & d1, const T & d2, const T & d3) {
            data[0] = d0;
            data[1] = d1;
            data[2] = d2;
            data[3] = d3;
        }

        HOST_DEVICE T & get(int i, int j) {
            assert(i >= 0 && i < 2 && j >= 0 && j < 2);
            return data[i * 2 + j];
        }

        HOST_DEVICE const T & get(int i, int j) const {
            assert(i >= 0 && i < 2 && j >= 0 && j < 2);
            return data[i * 2 + j];
        }

        HOST_DEVICE friend Matrix2<T> operator+(const Matrix2<T> & m1, const Matrix2<T> & m2) {
            Matrix2<T> m3;

            for (int i = 0; i < 4; i++)
                m3.data[i] = m1.data[i] + m2.data[i];

            return m3;
        }

        HOST_DEVICE friend Matrix2<T> operator-(const Matrix2<T> & m1, const Matrix2<T> & m2) {
            Matrix2<T> m3;

            for (int i = 0; i < 4; i++)
                m3.data[i] = m1.data[i] - m2.data[i];

            return m3;
        }

        HOST_DEVICE friend Matrix2<T> operator*(const Matrix2<T> & m1, const Matrix2<T> & m2) {
            Matrix2<T> m3;

            m3.data[0] = m1.data[0] * m2.data[0] + m1.data[1] * m2.data[2];
            m3.data[1] = m1.data[0] * m2.data[1] + m1.data[1] * m2.data[3];
            m3.data[2] = m1.data[2] * m2.data[0] + m1.data[3] * m2.data[2];
            m3.data[3] = m1.data[2] * m2.data[1] + m1.data[3] * m2.data[3];

            return m3;
        }

        HOST_DEVICE friend Matrix2<T> operator*(const Matrix2<T> & m1, const T & d) {
            Matrix2<T> m2;

            for (int i = 0; i < 4; i++)
                m2.data[i] = m1.data[i] * d;

            return m2;
        }

        HOST_DEVICE friend Matrix2<T> operator*(const T & d, const Matrix2<T> & m) { return m * d; }

        HOST_DEVICE Matrix2 & operator+=(const Matrix2 & m) {
            for (int i = 0; i < 4; i++)
                data[i] += m.data[i];

            return *this;
        }

        HOST_DEVICE Matrix2 & operator-=(const Matrix2 & m) {
            for (int i = 0; i < 4; i++)
                data[i] -= m.data[i];

            return *this;
        }

        HOST_DEVICE Matrix2 & operator*=(const T & d) {
            for (int i = 0; i < 4; i++)
                data[i] *= d;

            return *this;
        }

        HOST_DEVICE Matrix2 & operator*=(const Matrix2 & m) {
            *this = *this * m;
            return *this;
        }

        HOST_DEVICE Matrix2 & operator/=(const T & d) {
            for (int i = 0; i < 4; i++)
                data[i] /= d;

            return *this;
        }

        HOST_DEVICE T determinant() const { return data[0] * data[3] - data[1] * data[2]; }

        HOST_DEVICE friend Matrix2<T> transpose(const Matrix2<T> & m) { return Matrix2(m.data[0], m.data[2], m.data[1], m.data[3]); }

        HOST_DEVICE friend Matrix2<T> inverse(const Matrix2<T> & m) {
            T det = m.determinant();

            assert(det != 0);

            return Matrix2<T>(m.data[3] / det, -m.data[1] / det, -m.data[2] / det, m.data[0] / det);
        }

    private:
        T data[4];
};

template <typename T>
class Matrix4 {
    public:
        HOST_DEVICE Matrix4() {
            for (int i = 0; i < 16; i++)
                data[i] = T(0);
        }

        HOST_DEVICE Matrix4(const T & d0, const T & d1, const T & d2, const T & d3, const T & d4, const T & d5, const T & d6, const T & d7, const T & d8, const T & d9, const T & d10, const T & d11, const T & d12, const T & d13, const T & d14, const T & d15) {
            data[0] = d0;
            data[1] = d1;
            data[2] = d2;
            data[3] = d3;
            data[4] = d4;
            data[5] = d5;
            data[6] = d6;
            data[7] = d7;
            data[8] = d8;
            data[9] = d9;
            data[10] = d10;
            data[11] = d11;
            data[12] = d12;
            data[13] = d13;
            data[14] = d14;
            data[15] = d15;
        }

        HOST_DEVICE T & get(int i, int j) {
            assert(i >= 0 && i < 4 && j >= 0 && j < 4);
            return data[i * 4 + j];
        }

        HOST_DEVICE const T & get(int i, int j) const {
            assert(i >= 0 && i < 4 && j >= 0 && j < 4);
            return data[i * 4 + j];
        }

        HOST_DEVICE friend Matrix4<T> operator+(const Matrix4<T> & m1, const Matrix4<T> & m2) {
            Matrix4<T> m3;

            for (int i = 0; i < 16; i++)
                m3.data[i] = m1.data[i] + m2.data[i];

            return m3;
        }

        HOST_DEVICE friend Matrix4<T> operator-(const Matrix4<T> & m1, const Matrix4<T> & m2) {
            Matrix4<T> m3;

            for (int i = 0; i < 16; i++)
                m3.data[i] = m1.data[i] - m2.data[i];

            return m3;
        }

        HOST_DEVICE friend Matrix4<T> operator*(const Matrix4<T> & m1, const Matrix4<T> & m2) {
            Matrix4<T> m3;

            m3.data[0] = m1.data[0] * m2.data[0] + m1.data[1] * m2.data[4] + m1.data[2] * m2.data[8] + m1.data[3] * m2.data[12];
            m3.data[1] = m1.data[0] * m2.data[1] + m1.data[1] * m2.data[5] + m1.data[2] * m2.data[9] + m1.data[3] * m2.data[13];
            m3.data[2] = m1.data[0] * m2.data[2] + m1.data[1] * m2.data[6] + m1.data[2] * m2.data[10] + m1.data[3] * m2.data[14];
            m3.data[3] = m1.data[0] * m2.data[3] + m1.data[1] * m2.data[7] + m1.data[2] * m2.data[11] + m1.data[3] * m2.data[15];
            m3.data[4] = m1.data[4] * m2.data[0] + m1.data[5] * m2.data[4] + m1.data[6] * m2.data[8] + m1.data[7] * m2.data[12];
            m3.data[5] = m1.data[4] * m2.data[1] + m1.data[5] * m2.data[5] + m1.data[6] * m2.data[9] + m1.data[7] * m2.data[13];
            m3.data[6] = m1.data[4] * m2.data[2] + m1.data[5] * m2.data[6] + m1.data[6] * m2.data[10] + m1.data[7] * m2.data[14];
            m3.data[7] = m1.data[4] * m2.data[3] + m1.data[5] * m2.data[7] + m1.data[6] * m2.data[11] + m1.data[7] * m2.data[15];
            m3.data[8] = m1.data[8] * m2.data[0] + m1.data[9] * m2.data[4] + m1.data[10] * m2.data[8] + m1.data[11] * m2.data[12];
            m3.data[9] = m1.data[8] * m2.data[1] + m1.data[9] * m2.data[5] + m1.data[10] * m2.data[9] + m1.data[11] * m2.data[13];
            m3.data[10] = m1.data[8] * m2.data[2] + m1.data[9] * m2.data[6] + m1.data[10] * m2.data[10] + m1.data[11] * m2.data[14];
            m3.data[11] = m1.data[8] * m2.data[3] + m1.data[9] * m2.data[7] + m1.data[10] * m2.data[11] + m1.data[11] * m2.data[15];
            m3.data[12] = m1.data[12] * m2.data[0] + m1.data[13] * m2.data[4] + m1.data[14] * m2.data[8] + m1.data[15] * m2.data[12];
            m3.data[13] = m1.data[12] * m2.data[1] + m1.data[13] * m2.data[5] + m1.data[14] * m2.data[9] + m1.data[15] * m2.data[13];
            m3.data[14] = m1.data[12] * m2.data[2] + m1.data[13] * m2.data[6] + m1.data[14] * m2.data[10] + m1.data[15] * m2.data[14];
            m3.data[15] = m1.data[12] * m2.data[3] + m1.data[13] * m2.data[7] + m1.data[14] * m2.data[11] + m1.data[15] * m2.data[15];

            return m3;
        }

        HOST_DEVICE friend Matrix4<T> operator*(const Matrix4<T> & m1, const T & d) {
            Matrix4<T> m2;

            for (int i = 0; i < 16; i++)
                m2.data[i] = m1.data[i] * d;

            return m2;
        }

        HOST_DEVICE friend Matrix4<T> operator*(const T & d, const Matrix4<T> & m) { return m * d; }

        HOST_DEVICE Matrix4 & operator+=(const Matrix4 & m) {
            for (int i = 0; i < 16; i++)
                data[i] += m.data[i];

            return *this;
        }

        HOST_DEVICE Matrix4 & operator-=(const Matrix4 & m) {
            for (int i = 0; i < 16; i++)
                data[i] -= m.data[i];

            return *this;
        }

        HOST_DEVICE Matrix4 & operator*=(const T & d) {
            for (int i = 0; i < 16; i++)
                data[i] *= d;

            return *this;
        }

        HOST_DEVICE Matrix4 & operator*=(const Matrix4 & m) {
            *this = *this * m;
            return *this;
        }

        HOST_DEVICE Matrix4 & operator/=(const T & d) {
            for (int i = 0; i < 16; i++)
                data[i] /= d;

            return *this;
        }

        HOST_DEVICE T determinant() const { return data[0] * (data[5] * (data[10] * data[15] - data[11] * data[14]) - data[6] * (data[9] * data[15] - data[11] * data[13]) + data[7] * (data[9] * data[14] - data[10] * data[13])) - data[1] * (data[4] * (data[10] * data[15] - data[11] * data[14]) - data[6] * (data[8] * data[15] - data[11] * data[12]) + data[7] * (data[8] * data[14] - data[10] * data[12])) + data[2] * (data[4] * (data[9] * data[15] - data[11] * data[13]) - data[5] * (data[8] * data[15] - data[11] * data[12]) + data[7] * (data[8] * data[13] - data[9] * data[12])) - data[3] * (data[4] * (data[9] * data[14] - data[10] * data[13]) - data[5] * (data[8] * data[14] - data[10] * data[12]) + data[6] * (data[8] * data[13] - data[9] * data[12])); }

        HOST_DEVICE friend Matrix4<T> transpose(const Matrix4<T> & m) { return Matrix4<T>(m.data[0], m.data[4], m.data[8], m.data[12], m.data[1], m.data[5], m.data[9], m.data[13], m.data[2], m.data[6], m.data[10], m.data[14], m.data[3], m.data[7], m.data[11], m.data[15]); }

        HOST_DEVICE friend Matrix4<T> inverse(const Matrix4<T> & m) {
            T det = m.determinant();

            assert(det != 0);

            T s0 = m.data[0] * m.data[5] - m.data[1] * m.data[4];
            T s1 = m.data[0] * m.data[6] - m.data[2] * m.data[4];
            T s2 = m.data[0] * m.data[7] - m.data[3] * m.data[4];
            T s3 = m.data[1] * m.data[6] - m.data[2] * m.data[5];
            T s4 = m.data[1] * m.data[7] - m.data[3] * m.data[5];
            T s5 = m.data[2] * m.data[7] - m.data[3] * m.data[6];

            T c0 = m.data[8] * m.data[13] - m.data[9] * m.data[12];
            T c1 = m.data[8] * m.data[14] - m.data[10] * m.data[12];
            T c2 = m.data[8] * m.data[15] - m.data[11] * m.data[12];
            T c3 = m.data[9] * m.data[14] - m.data[10] * m.data[13];
            T c4 = m.data[9] * m.data[15] - m.data[11] * m.data[13];
            T c5 = m.data[10] * m.data[15] - m.data[11] * m.data[14];

            return Matrix4<T>((m.data[5] * c5 - m.data[6] * c4 + m.data[7] * c3) / det, (-m.data[1] * c5 + m.data[2] * c4 - m.data[3] * c3) / det, (m.data[13] * s5 - m.data[14] * s4 + m.data[15] * s3) / det, (-m.data[9] * s5 + m.data[10] * s4 - m.data[11] * s3) / det, (-m.data[4] * c5 + m.data[6] * c2 - m.data[7] * c1) / det, (m.data[0] * c5 - m.data[2] * c2 + m.data[3] * c1) / det, (-m.data[12] * s5 + m.data[14] * s2 - m.data[15] * s1) / det, (m.data[8] * s5 - m.data[10] * s2 + m.data[11] * s1) / det, (m.data[4] * c4 - m.data[5] * c2 + m.data[7] * c0) / det, (-m.data[0] * c4 + m.data[1] * c2 - m.data[3] * c0) / det, (m.data[12] * s4 - m.data[13] * s2 + m.data[15] * s0) / det, (-m.data[8] * s4 + m.data[9] * s2 - m.data[11] * s0) / det, (-m.data[4] * c3 + m.data[5] * c1 - m.data[6] * c0) / det, (m.data[0] * c3 - m.data[1] * c1 + m.data[2] * c0) / det, (-m.data[12] * s3 + m.data[13] * s1 - m.data[14] * s0) / det, (m.data[8] * s3 - m.data[9] * s1 + m.data[10] * s0) / det);
        }

    private:
        T data[16];
};