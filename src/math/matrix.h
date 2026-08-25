#pragma once

#include <cassert>

#include "core/platform.h"

template <typename T>
class Matrix2 {
    public:
        HOST_DEVICE Matrix2() {
            for (int i = 0; i < 4; i++)
                elements[i] = T(0);
        }

        HOST_DEVICE Matrix2(const T & d0, const T & d1, const T & d2, const T & d3) {
            elements[0] = d0;
            elements[1] = d1;
            elements[2] = d2;
            elements[3] = d3;
        }

        HOST_DEVICE Matrix2(const T d[2][2]) {
            for (int i = 0; i < 4; i++)
                elements[i] = d[i / 2][i % 2];
        }

        HOST_DEVICE T & get(int i, int j) {
            assert(i >= 0 && i < 2 && j >= 0 && j < 2);
            return elements[i * 2 + j];
        }

        HOST_DEVICE const T & get(int i, int j) const {
            assert(i >= 0 && i < 2 && j >= 0 && j < 2);
            return elements[i * 2 + j];
        }

        HOST_DEVICE friend Matrix2<T> operator+(const Matrix2<T> & m1, const Matrix2<T> & m2) {
            Matrix2<T> m3;

            for (int i = 0; i < 4; i++)
                m3.elements[i] = m1.elements[i] + m2.elements[i];

            return m3;
        }

        HOST_DEVICE friend Matrix2<T> operator-(const Matrix2<T> & m1, const Matrix2<T> & m2) {
            Matrix2<T> m3;

            for (int i = 0; i < 4; i++)
                m3.elements[i] = m1.elements[i] - m2.elements[i];

            return m3;
        }

        HOST_DEVICE friend Matrix2<T> operator*(const Matrix2<T> & m1, const Matrix2<T> & m2) {
            Matrix2<T> m3;

            m3.elements[0] = m1.elements[0] * m2.elements[0] + m1.elements[1] * m2.elements[2];
            m3.elements[1] = m1.elements[0] * m2.elements[1] + m1.elements[1] * m2.elements[3];
            m3.elements[2] = m1.elements[2] * m2.elements[0] + m1.elements[3] * m2.elements[2];
            m3.elements[3] = m1.elements[2] * m2.elements[1] + m1.elements[3] * m2.elements[3];

            return m3;
        }

        HOST_DEVICE friend Matrix2<T> operator*(const Matrix2<T> & m1, const T & d) {
            Matrix2<T> m2;

            for (int i = 0; i < 4; i++)
                m2.elements[i] = m1.elements[i] * d;

            return m2;
        }

        HOST_DEVICE friend Matrix2<T> operator*(const T & d, const Matrix2<T> & m) { return m * d; }

        HOST_DEVICE Matrix2 & operator+=(const Matrix2 & m) {
            for (int i = 0; i < 4; i++)
                elements[i] += m.elements[i];

            return *this;
        }

        HOST_DEVICE Matrix2 & operator-=(const Matrix2 & m) {
            for (int i = 0; i < 4; i++)
                elements[i] -= m.elements[i];

            return *this;
        }

        HOST_DEVICE Matrix2 & operator*=(const T & d) {
            for (int i = 0; i < 4; i++)
                elements[i] *= d;

            return *this;
        }

        HOST_DEVICE Matrix2 & operator*=(const Matrix2 & m) {
            *this = *this * m;
            return *this;
        }

        HOST_DEVICE Matrix2 & operator/=(const T & d) {
            for (int i = 0; i < 4; i++)
                elements[i] /= d;

            return *this;
        }

        HOST_DEVICE T determinant() const { return elements[0] * elements[3] - elements[1] * elements[2]; }

        HOST_DEVICE friend Matrix2<T> transpose(const Matrix2<T> & m) { return Matrix2(m.elements[0], m.elements[2], m.elements[1], m.elements[3]); }

        HOST_DEVICE friend Matrix2<T> inverse(const Matrix2<T> & m) {
            T det = m.determinant();

            assert(fabs(det) > EPSILON);

            return Matrix2<T>(m.elements[3] / det, -m.elements[1] / det, -m.elements[2] / det, m.elements[0] / det);
        }

        HOST_DEVICE T * data() { return elements; }

    private:
        T elements[4];
};

template <typename T>
class Matrix3 {
    public:
        HOST_DEVICE Matrix3() {
            for (int i = 0; i < 9; i++)
                elements[i] = T(0);
        }

        HOST_DEVICE Matrix3(const T & d0, const T & d1, const T & d2, const T & d3, const T & d4, const T & d5, const T & d6, const T & d7, const T & d8) {
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

        HOST_DEVICE Matrix3(const T d[3][3]) {
            for (int i = 0; i < 9; i++)
                elements[i] = d[i / 3][i % 3];
        }

        HOST_DEVICE T & get(int i, int j) {
            assert(i >= 0 && i < 3 && j >= 0 && j < 3);
            return elements[i * 3 + j];
        }

        HOST_DEVICE const T & get(int i, int j) const {
            assert(i >= 0 && i < 3 && j >= 0 && j < 3);
            return elements[i * 3 + j];
        }

        HOST_DEVICE friend Matrix3<T> operator+(const Matrix3<T> & m1, const Matrix3<T> & m2) {
            Matrix3<T> m3;

            for (int i = 0; i < 9; i++)
                m3.elements[i] = m1.elements[i] + m2.elements[i];

            return m3;
        }

        HOST_DEVICE friend Matrix3<T> operator-(const Matrix3<T> & m1, const Matrix3<T> & m2) {
            Matrix3<T> m3;

            for (int i = 0; i < 9; i++)
                m3.elements[i] = m1.elements[i] - m2.elements[i];

            return m3;
        }

        HOST_DEVICE friend Matrix3<T> operator*(const Matrix3<T> & m1, const Matrix3<T> & m2) {
            Matrix3<T> m3;

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

        HOST_DEVICE friend Matrix3<T> operator*(const Matrix3<T> & m1, const T & d) {
            Matrix3<T> m2;

            for (int i = 0; i < 9; i++)
                m2.elements[i] = m1.elements[i] * d;

            return m2;
        }

        HOST_DEVICE friend Matrix3<T> operator*(const T & d, const Matrix3<T> & m) { return m * d; }

        HOST_DEVICE Matrix3 & operator+=(const Matrix3 & m) {
            for (int i = 0; i < 9; i++)
                elements[i] += m.elements[i];

            return *this;
        }

        HOST_DEVICE Matrix3 & operator-=(const Matrix3 & m) {
            for (int i = 0; i < 9; i++)
                elements[i] -= m.elements[i];

            return *this;
        }

        HOST_DEVICE Matrix3 & operator*=(const T & d) {
            for (int i = 0; i < 9; i++)
                elements[i] *= d;

            return *this;
        }

        HOST_DEVICE Matrix3 & operator*=(const Matrix3 & m) {
            *this = *this * m;
            return *this;
        }

        HOST_DEVICE Matrix3 & operator/=(const T & d) {
            for (int i = 0; i < 9; i++)
                elements[i] /= d;

            return *this;
        }

        HOST_DEVICE T determinant() const { return elements[0] * (elements[4] * elements[8] - elements[5] * elements[7]) - elements[1] * (elements[3] * elements[8] - elements[5] * elements[6]) + elements[2] * (elements[3] * elements[7] - elements[4] * elements[6]); }

        HOST_DEVICE friend Matrix3<T> transpose(const Matrix3<T> & m) { return Matrix3(m.elements[0], m.elements[3], m.elements[6], m.elements[1], m.elements[4], m.elements[7], m.elements[2], m.elements[5], m.elements[8]); }

        HOST_DEVICE friend Matrix3<T> inverse(const Matrix3<T> & m) {
            T det = m.determinant();

            assert(fabs(det) > EPSILON);

            return Matrix3<T>((m.elements[4] * m.elements[8] - m.elements[5] * m.elements[7]) / det, (m.elements[2] * m.elements[7] - m.elements[1] * m.elements[8]) / det, (m.elements[1] * m.elements[5] - m.elements[2] * m.elements[4]) / det, (m.elements[5] * m.elements[6] - m.elements[3] * m.elements[8]) / det, (m.elements[0] * m.elements[8] - m.elements[2] * m.elements[6]) / det, (m.elements[2] * m.elements[3] - m.elements[0] * m.elements[5]) / det, (m.elements[3] * m.elements[7] - m.elements[4] * m.elements[6]) / det, (m.elements[1] * m.elements[6] - m.elements[0] * m.elements[7]) / det, (m.elements[0] * m.elements[4] - m.elements[1] * m.elements[3]) / det);
        }

        HOST_DEVICE T * data() { return elements; }

    private:
        T elements[9];
};

template <typename T>
class Matrix4 {
    public:
        HOST_DEVICE Matrix4() {
            for (int i = 0; i < 16; i++)
                elements[i] = T(0);
        }

        HOST_DEVICE Matrix4(const T & d0, const T & d1, const T & d2, const T & d3, const T & d4, const T & d5, const T & d6, const T & d7, const T & d8, const T & d9, const T & d10, const T & d11, const T & d12, const T & d13, const T & d14, const T & d15) {
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

        HOST_DEVICE Matrix4(const T d[4][4]) {
            for (int i = 0; i < 16; i++)
                elements[i] = d[i / 4][i % 4];
        }

        HOST_DEVICE T & get(int i, int j) {
            assert(i >= 0 && i < 4 && j >= 0 && j < 4);
            return elements[i * 4 + j];
        }

        HOST_DEVICE const T & get(int i, int j) const {
            assert(i >= 0 && i < 4 && j >= 0 && j < 4);
            return elements[i * 4 + j];
        }

        HOST_DEVICE friend Matrix4<T> operator+(const Matrix4<T> & m1, const Matrix4<T> & m2) {
            Matrix4<T> m3;

            for (int i = 0; i < 16; i++)
                m3.elements[i] = m1.elements[i] + m2.elements[i];

            return m3;
        }

        HOST_DEVICE friend Matrix4<T> operator-(const Matrix4<T> & m1, const Matrix4<T> & m2) {
            Matrix4<T> m3;

            for (int i = 0; i < 16; i++)
                m3.elements[i] = m1.elements[i] - m2.elements[i];

            return m3;
        }

        HOST_DEVICE friend Matrix4<T> operator*(const Matrix4<T> & m1, const Matrix4<T> & m2) {
            Matrix4<T> m3;

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

        HOST_DEVICE friend Matrix4<T> operator*(const Matrix4<T> & m1, const T & d) {
            Matrix4<T> m2;

            for (int i = 0; i < 16; i++)
                m2.elements[i] = m1.elements[i] * d;

            return m2;
        }

        HOST_DEVICE friend Matrix4<T> operator*(const T & d, const Matrix4<T> & m) { return m * d; }

        HOST_DEVICE Matrix4 & operator+=(const Matrix4 & m) {
            for (int i = 0; i < 16; i++)
                elements[i] += m.elements[i];

            return *this;
        }

        HOST_DEVICE Matrix4 & operator-=(const Matrix4 & m) {
            for (int i = 0; i < 16; i++)
                elements[i] -= m.elements[i];

            return *this;
        }

        HOST_DEVICE Matrix4 & operator*=(const T & d) {
            for (int i = 0; i < 16; i++)
                elements[i] *= d;

            return *this;
        }

        HOST_DEVICE Matrix4 & operator*=(const Matrix4 & m) {
            *this = *this * m;
            return *this;
        }

        HOST_DEVICE Matrix4 & operator/=(const T & d) {
            for (int i = 0; i < 16; i++)
                elements[i] /= d;

            return *this;
        }

        HOST_DEVICE T determinant() const { return elements[0] * (elements[5] * (elements[10] * elements[15] - elements[11] * elements[14]) - elements[6] * (elements[9] * elements[15] - elements[11] * elements[13]) + elements[7] * (elements[9] * elements[14] - elements[10] * elements[13])) - elements[1] * (elements[4] * (elements[10] * elements[15] - elements[11] * elements[14]) - elements[6] * (elements[8] * elements[15] - elements[11] * elements[12]) + elements[7] * (elements[8] * elements[14] - elements[10] * elements[12])) + elements[2] * (elements[4] * (elements[9] * elements[15] - elements[11] * elements[13]) - elements[5] * (elements[8] * elements[15] - elements[11] * elements[12]) + elements[7] * (elements[8] * elements[13] - elements[9] * elements[12])) - elements[3] * (elements[4] * (elements[9] * elements[14] - elements[10] * elements[13]) - elements[5] * (elements[8] * elements[14] - elements[10] * elements[12]) + elements[6] * (elements[8] * elements[13] - elements[9] * elements[12])); }

        HOST_DEVICE friend Matrix4<T> transpose(const Matrix4<T> & m) { return Matrix4<T>(m.elements[0], m.elements[4], m.elements[8], m.elements[12], m.elements[1], m.elements[5], m.elements[9], m.elements[13], m.elements[2], m.elements[6], m.elements[10], m.elements[14], m.elements[3], m.elements[7], m.elements[11], m.elements[15]); }

        HOST_DEVICE friend Matrix4<T> inverse(const Matrix4<T> & m) {
            T det = m.determinant();

            assert(fabs(det) > EPSILON);

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

            return Matrix4<T>((m.elements[5] * c5 - m.elements[6] * c4 + m.elements[7] * c3) / det, (-m.elements[1] * c5 + m.elements[2] * c4 - m.elements[3] * c3) / det, (m.elements[13] * s5 - m.elements[14] * s4 + m.elements[15] * s3) / det, (-m.elements[9] * s5 + m.elements[10] * s4 - m.elements[11] * s3) / det, (-m.elements[4] * c5 + m.elements[6] * c2 - m.elements[7] * c1) / det, (m.elements[0] * c5 - m.elements[2] * c2 + m.elements[3] * c1) / det, (-m.elements[12] * s5 + m.elements[14] * s2 - m.elements[15] * s1) / det, (m.elements[8] * s5 - m.elements[10] * s2 + m.elements[11] * s1) / det, (m.elements[4] * c4 - m.elements[5] * c2 + m.elements[7] * c0) / det, (-m.elements[0] * c4 + m.elements[1] * c2 - m.elements[3] * c0) / det, (m.elements[12] * s4 - m.elements[13] * s2 + m.elements[15] * s0) / det, (-m.elements[8] * s4 + m.elements[9] * s2 - m.elements[11] * s0) / det, (-m.elements[4] * c3 + m.elements[5] * c1 - m.elements[6] * c0) / det, (m.elements[0] * c3 - m.elements[1] * c1 + m.elements[2] * c0) / det, (-m.elements[12] * s3 + m.elements[13] * s1 - m.elements[14] * s0) / det, (m.elements[8] * s3 - m.elements[9] * s1 + m.elements[10] * s0) / det);
        }

        HOST_DEVICE T * data() { return elements; }

    private:
        T elements[16];
};