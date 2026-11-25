#pragma once

#include "core/platform.h"
#include "math/complex.h"
#include "math/vector.h"

namespace lambda {
    template <typename T, int N>
    requires ((std::is_arithmetic_v<T> || std::is_same_v<T, Complex>) && N > 0)
    class Matrix {
        public:
            LAMBDA_HOST_DEVICE Matrix() {
                for (int i = 0; i < N; i++)
                    for (int j = 0; j < N; j++) elements[i][j] = i == j ? T(1) : T(0);
            }

            template <typename U>
            requires (std::is_convertible_v<U, T>)
            LAMBDA_HOST_DEVICE Matrix(U d) {
                for (int i = 0; i < N; i++)
                    for (int j = 0; j < N; j++) elements[i][j] = i == j ? T(d) : T(0);
            }

            template <typename... Ts>
            requires (sizeof...(Ts) == N * N && (std::is_convertible_v<Ts, T> && ...) && N > 1)
            LAMBDA_HOST_DEVICE Matrix(Ts... d) {
                int i = 0;

                ((elements[i / N][i % N] = T(d), i++), ...);
            }

            template <int M>
            requires (M >= N)
            LAMBDA_HOST_DEVICE Matrix(const Matrix<T, M> & m) {
                for (int i = 0; i < N; i++)
                    for (int j = 0; j < N; j++) elements[i][j] = T(m(i, j));
            }

            template <typename U>
            requires (std::is_convertible_v<U, T>)
            LAMBDA_HOST_DEVICE Matrix(const U (& d)[N][N]) {
                for (int i = 0; i < N; i++)
                    for (int j = 0; j < N; j++) elements[i][j] = T(d[i][j]);
            }

            template <typename U>
            requires (std::is_convertible_v<U, T>)
            LAMBDA_HOST_DEVICE Matrix(const Matrix<U, N> & m) {
                for (int i = 0; i < N; i++)
                    for (int j = 0; j < N; j++) elements[i][j] = T(m(i, j));
            }

            LAMBDA_HOST_DEVICE T & operator()(int i, int j) { return elements[i][j]; }

            LAMBDA_HOST_DEVICE const T & operator()(int i, int j) const { return elements[i][j]; }

            LAMBDA_HOST_DEVICE bool operator==(const Matrix & m) const {
                for (int i = 0; i < N; i++)
                    for (int j = 0; j < N; j++)
                        if (elements[i][j] != m.elements[i][j]) return false;

                return true;
            }

            LAMBDA_HOST_DEVICE bool operator!=(const Matrix & m) const {
                for (int i = 0; i < N; i++)
                    for (int j = 0; j < N; j++)
                        if (elements[i][j] != m.elements[i][j]) return true;

                return false;
            }

            LAMBDA_HOST_DEVICE friend Matrix<T, N> operator+(const Matrix<T, N> & m1, const Matrix<T, N> & m2) {
                Matrix<T, N> m3;

                for (int i = 0; i < N; i++)
                    for (int j = 0; j < N; j++) m3.elements[i][j] = m1.elements[i][j] + m2.elements[i][j];

                return m3;
            }

            LAMBDA_HOST_DEVICE friend Matrix<T, N> operator-(const Matrix<T, N> & m1, const Matrix<T, N> & m2) {
                Matrix<T, N> m3;

                for (int i = 0; i < N; i++)
                    for (int j = 0; j < N; j++) m3.elements[i][j] = m1.elements[i][j] - m2.elements[i][j];

                return m3;
            }

            LAMBDA_HOST_DEVICE friend Matrix<T, N> operator*(const Matrix<T, N> & m1, const Matrix<T, N> & m2) requires (N == 1) { return Matrix<T, N>(m1.elements[0][0] * m2.elements[0][0]); }

            LAMBDA_HOST_DEVICE friend Matrix<T, N> operator*(const Matrix<T, N> & m1, const Matrix<T, N> & m2) requires (N == 2) {
                Matrix<T, N> m3;

                m3.elements[0][0] = m1.elements[0][0] * m2.elements[0][0] + m1.elements[0][1] * m2.elements[1][0];
                m3.elements[0][1] = m1.elements[0][0] * m2.elements[0][1] + m1.elements[0][1] * m2.elements[1][1];
                m3.elements[1][0] = m1.elements[1][0] * m2.elements[0][0] + m1.elements[1][1] * m2.elements[1][0];
                m3.elements[1][1] = m1.elements[1][0] * m2.elements[0][1] + m1.elements[1][1] * m2.elements[1][1];

                return m3;
            }

            LAMBDA_HOST_DEVICE friend Matrix<T, N> operator*(const Matrix<T, N> & m1, const Matrix<T, N> & m2) requires (N == 3) {
                Matrix<T, N> m3;

                m3.elements[0][0] = m1.elements[0][0] * m2.elements[0][0] + m1.elements[0][1] * m2.elements[1][0] + m1.elements[0][2] * m2.elements[2][0];
                m3.elements[0][1] = m1.elements[0][0] * m2.elements[0][1] + m1.elements[0][1] * m2.elements[1][1] + m1.elements[0][2] * m2.elements[2][1];
                m3.elements[0][2] = m1.elements[0][0] * m2.elements[0][2] + m1.elements[0][1] * m2.elements[1][2] + m1.elements[0][2] * m2.elements[2][2];
                m3.elements[1][0] = m1.elements[1][0] * m2.elements[0][0] + m1.elements[1][1] * m2.elements[1][0] + m1.elements[1][2] * m2.elements[2][0];
                m3.elements[1][1] = m1.elements[1][0] * m2.elements[0][1] + m1.elements[1][1] * m2.elements[1][1] + m1.elements[1][2] * m2.elements[2][1];
                m3.elements[1][2] = m1.elements[1][0] * m2.elements[0][2] + m1.elements[1][1] * m2.elements[1][2] + m1.elements[1][2] * m2.elements[2][2];
                m3.elements[2][0] = m1.elements[2][0] * m2.elements[0][0] + m1.elements[2][1] * m2.elements[1][0] + m1.elements[2][2] * m2.elements[2][0];
                m3.elements[2][1] = m1.elements[2][0] * m2.elements[0][1] + m1.elements[2][1] * m2.elements[1][1] + m1.elements[2][2] * m2.elements[2][1];
                m3.elements[2][2] = m1.elements[2][0] * m2.elements[0][2] + m1.elements[2][1] * m2.elements[1][2] + m1.elements[2][2] * m2.elements[2][2];

                return m3;
            }

            LAMBDA_HOST_DEVICE friend Matrix<T, N> operator*(const Matrix<T, N> & m1, const Matrix<T, N> & m2) requires (N == 4) {
                Matrix<T, N> m3;

                m3.elements[0][0] = m1.elements[0][0] * m2.elements[0][0] + m1.elements[0][1] * m2.elements[1][0] + m1.elements[0][2] * m2.elements[2][0] + m1.elements[0][3] * m2.elements[3][0];
                m3.elements[0][1] = m1.elements[0][0] * m2.elements[0][1] + m1.elements[0][1] * m2.elements[1][1] + m1.elements[0][2] * m2.elements[2][1] + m1.elements[0][3] * m2.elements[3][1];
                m3.elements[0][2] = m1.elements[0][0] * m2.elements[0][2] + m1.elements[0][1] * m2.elements[1][2] + m1.elements[0][2] * m2.elements[2][2] + m1.elements[0][3] * m2.elements[3][2];
                m3.elements[0][3] = m1.elements[0][0] * m2.elements[0][3] + m1.elements[0][1] * m2.elements[1][3] + m1.elements[0][2] * m2.elements[2][3] + m1.elements[0][3] * m2.elements[3][3];
                m3.elements[1][0] = m1.elements[1][0] * m2.elements[0][0] + m1.elements[1][1] * m2.elements[1][0] + m1.elements[1][2] * m2.elements[2][0] + m1.elements[1][3] * m2.elements[3][0];
                m3.elements[1][1] = m1.elements[1][0] * m2.elements[0][1] + m1.elements[1][1] * m2.elements[1][1] + m1.elements[1][2] * m2.elements[2][1] + m1.elements[1][3] * m2.elements[3][1];
                m3.elements[1][2] = m1.elements[1][0] * m2.elements[0][2] + m1.elements[1][1] * m2.elements[1][2] + m1.elements[1][2] * m2.elements[2][2] + m1.elements[1][3] * m2.elements[3][2];
                m3.elements[1][3] = m1.elements[1][0] * m2.elements[0][3] + m1.elements[1][1] * m2.elements[1][3] + m1.elements[1][2] * m2.elements[2][3] + m1.elements[1][3] * m2.elements[3][3];
                m3.elements[2][0] = m1.elements[2][0] * m2.elements[0][0] + m1.elements[2][1] * m2.elements[1][0] + m1.elements[2][2] * m2.elements[2][0] + m1.elements[2][3] * m2.elements[3][0];
                m3.elements[2][1] = m1.elements[2][0] * m2.elements[0][1] + m1.elements[2][1] * m2.elements[1][1] + m1.elements[2][2] * m2.elements[2][1] + m1.elements[2][3] * m2.elements[3][1];
                m3.elements[2][2] = m1.elements[2][0] * m2.elements[0][2] + m1.elements[2][1] * m2.elements[1][2] + m1.elements[2][2] * m2.elements[2][2] + m1.elements[2][3] * m2.elements[3][2];
                m3.elements[2][3] = m1.elements[2][0] * m2.elements[0][3] + m1.elements[2][1] * m2.elements[1][3] + m1.elements[2][2] * m2.elements[2][3] + m1.elements[2][3] * m2.elements[3][3];
                m3.elements[3][0] = m1.elements[3][0] * m2.elements[0][0] + m1.elements[3][1] * m2.elements[1][0] + m1.elements[3][2] * m2.elements[2][0] + m1.elements[3][3] * m2.elements[3][0];
                m3.elements[3][1] = m1.elements[3][0] * m2.elements[0][1] + m1.elements[3][1] * m2.elements[1][1] + m1.elements[3][2] * m2.elements[2][1] + m1.elements[3][3] * m2.elements[3][1];
                m3.elements[3][2] = m1.elements[3][0] * m2.elements[0][2] + m1.elements[3][1] * m2.elements[1][2] + m1.elements[3][2] * m2.elements[2][2] + m1.elements[3][3] * m2.elements[3][2];
                m3.elements[3][3] = m1.elements[3][0] * m2.elements[0][3] + m1.elements[3][1] * m2.elements[1][3] + m1.elements[3][2] * m2.elements[2][3] + m1.elements[3][3] * m2.elements[3][3];

                return m3;
            }

            LAMBDA_HOST_DEVICE friend Matrix<T, N> operator*(const Matrix<T, N> & m1, const Matrix<T, N> & m2) requires (N > 4) {
                Matrix<T, N> m3;

                for (int i = 0; i < N; i++)
                    for (int j = 0; j < N; j++) {
                        T sum = 0;

                        for (int k = 0; k < N; k++) sum += m1.elements[i][k] * m2.elements[k][j];

                        m3.elements[i][j] = sum;
                    }

                return m3;
            }

            LAMBDA_HOST_DEVICE friend Matrix<T, N> operator*(const Matrix<T, N> & m1, const T & d) {
                Matrix<T, N> m2;

                for (int i = 0; i < N; i++)
                    for (int j = 0; j < N; j++) m2.elements[i][j] = m1.elements[i][j] * d;

                return m2;
            }

            LAMBDA_HOST_DEVICE friend Matrix<T, N> operator*(const T & d, const Matrix<T, N> & m) { return m * d; }

            LAMBDA_HOST_DEVICE friend Vector<T, N> operator*(const Matrix<T, N> & m, const Vector<T, N> & v1) {
                Vector<T, N> v2;

                for (int i = 0; i < N; i++)
                    for (int j = 0; j < N; j++) v2[i] += m.elements[i][j] * v1[j];

                return v2;
            }

            LAMBDA_HOST_DEVICE friend Vector<T, N> operator*(const Vector<T, N> & v1, const Matrix<T, N> & m) {
                Vector<T, N> v2;

                for (int i = 0; i < N; i++)
                    for (int j = 0; j < N; j++) v2[i] += v1[j] * m.elements[j][i];

                return v2;
            }

            LAMBDA_HOST_DEVICE friend Matrix<T, N> operator/(const Matrix<T, N> & m1, const T & d) {
                Matrix<T, N> m2;

                for (int i = 0; i < N; i++)
                    for (int j = 0; j < N; j++) m2.elements[i][j] = m1.elements[i][j] / d;

                return m2;
            }

            LAMBDA_HOST_DEVICE Matrix<T, N> & operator+=(const Matrix<T, N> & m) {
                for (int i = 0; i < N; i++)
                    for (int j = 0; j < N; j++) elements[i][j] += m.elements[i][j];

                return *this;
            }

            LAMBDA_HOST_DEVICE Matrix<T, N> & operator-=(const Matrix<T, N> & m) {
                for (int i = 0; i < N; i++)
                    for (int j = 0; j < N; j++) elements[i][j] -= m.elements[i][j];

                return *this;
            }

            LAMBDA_HOST_DEVICE Matrix<T, N> & operator*=(const T & d) {
                for (int i = 0; i < N; i++)
                    for (int j = 0; j < N; j++) elements[i][j] *= d;

                return *this;
            }

            LAMBDA_HOST_DEVICE Matrix<T, N> & operator*=(const Matrix<T, N> & m) {
                *this = *this * m;

                return *this;
            }

            LAMBDA_HOST_DEVICE Matrix<T, N> & operator/=(const T & d) {
                for (int i = 0; i < N; i++)
                    for (int j = 0; j < N; j++) elements[i][j] /= d;

                return *this;
            }

            LAMBDA_HOST_DEVICE T determinant() const requires (N == 1) { return elements[0][0]; }

            LAMBDA_HOST_DEVICE T determinant() const requires (N == 2) { return elements[0][0] * elements[1][1] - elements[0][1] * elements[1][0]; }

            LAMBDA_HOST_DEVICE T determinant() const requires (N == 3) { return elements[0][0] * (elements[1][1] * elements[2][2] - elements[1][2] * elements[2][1]) - elements[0][1] * (elements[1][0] * elements[2][2] - elements[1][2] * elements[2][0]) + elements[0][2] * (elements[1][0] * elements[2][1] - elements[1][1] * elements[2][0]); }

            LAMBDA_HOST_DEVICE T determinant() const requires (N == 4) { return elements[0][0] * (elements[1][1] * (elements[2][2] * elements[3][3] - elements[2][3] * elements[3][2]) - elements[1][2] * (elements[2][1] * elements[3][3] - elements[2][3] * elements[3][1]) + elements[1][3] * (elements[2][1] * elements[3][2] - elements[2][2] * elements[3][1])) - elements[0][1] * (elements[1][0] * (elements[2][2] * elements[3][3] - elements[2][3] * elements[3][2]) - elements[1][2] * (elements[2][0] * elements[3][3] - elements[2][3] * elements[3][0]) + elements[1][3] * (elements[2][0] * elements[3][2] - elements[2][2] * elements[3][0])) + elements[0][2] * (elements[1][0] * (elements[2][1] * elements[3][3] - elements[2][3] * elements[3][1]) - elements[1][1] * (elements[2][0] * elements[3][3] - elements[2][3] * elements[3][0]) + elements[1][3] * (elements[2][0] * elements[3][1] - elements[2][1] * elements[3][0])) - elements[0][3] * (elements[1][0] * (elements[2][1] * elements[3][2] - elements[2][2] * elements[3][1]) - elements[1][1] * (elements[2][0] * elements[3][2] - elements[2][2] * elements[3][0]) + elements[1][2] * (elements[2][0] * elements[3][1] - elements[2][1] * elements[3][0])); }

            LAMBDA_HOST_DEVICE T determinant() const requires (N > 4) {
                using std::abs;
                using std::fabs;

                T scale = 0;

                for (int i = 0; i < N; i++)
                    for (int j = 0; j < N; j++) scale = std::max(scale, abs(elements[i][j]));

                Matrix<T, N> m(*this);

                T det = T(1);

                for (int i = 0; i < N; i++) {
                    int pivot = i;

                    for (int j = i + 1; j < N; j++)
                        if (abs(m.elements[j][i]) > abs(m.elements[pivot][i])) pivot = j;

                    if (abs(m.elements[pivot][i]) <= constants::EPSILON_RELATIVE * scale) return T(0);

                    if (pivot != i) {
                        for (int j = i; j < N; j++) {
                            T temp = m.elements[i][j];
                            m.elements[i][j] = m.elements[pivot][j];
                            m.elements[pivot][j] = temp;
                        }

                        det *= T(-1);
                    }

                    det *= m.elements[i][i];

                    for (int j = i + 1; j < N; j++) {
                        T factor = m.elements[j][i] / m.elements[i][i];

                        for (int k = i; k < N; k++) m.elements[j][k] -= factor * m.elements[i][k];
                    }
                }

                return det;
            }

            LAMBDA_HOST_DEVICE friend Matrix<T, N> transpose(const Matrix<T, N> & m1) {
                Matrix<T, N> m2;

                for (int i = 0; i < N; i++)
                    for (int j = 0; j < N; j++) m2.elements[i][j] = m1.elements[j][i];

                return m2;
            }

            LAMBDA_HOST_DEVICE friend Matrix<T, N> inverse(const Matrix<T, N> & m) requires (N == 1) {
                using std::abs;
                using std::fabs;

                T det = m.determinant();
                T invDet = T(1) / det;

                return abs(det) > 0 ? Matrix<T, N>(invDet) : Matrix<T, N>();
            }

            LAMBDA_HOST_DEVICE friend Matrix<T, N> inverse(const Matrix<T, N> & m) requires (N == 2) {
                using std::abs;
                using std::fabs;
                using std::sqrt;

                T det = m.determinant();
                T invDet = T(1) / det;

                T scale = 1;

                for (int i = 0; i < N; i++){
                    T sum = 0;

                    for (int j = 0; j < N; j++) sum += abs(m.elements[i][j]) * abs(m.elements[i][j]);

                    scale *= sqrt(sum);
                }

                return abs(det) > constants::EPSILON_RELATIVE * scale ? Matrix<T, N>(m.elements[1][1] * invDet, -m.elements[0][1] * invDet, -m.elements[1][0] * invDet, m.elements[0][0] * invDet) : Matrix<T, N>();
            }

            LAMBDA_HOST_DEVICE friend Matrix<T, N> inverse(const Matrix<T, N> & m) requires (N == 3) {
                using std::abs;
                using std::fabs;
                using std::sqrt;

                T det = m.determinant();
                T invDet = T(1) / det;

                T scale = 1;

                for (int i = 0; i < N; i++){
                    T sum = 0;

                    for (int j = 0; j < N; j++) sum += abs(m.elements[i][j]) * abs(m.elements[i][j]);

                    scale *= sqrt(sum);
                }

                return abs(det) > constants::EPSILON_RELATIVE * scale ? Matrix<T, N>((m.elements[1][1] * m.elements[2][2] - m.elements[1][2] * m.elements[2][1]) * invDet, (m.elements[0][2] * m.elements[2][1] - m.elements[0][1] * m.elements[2][2]) * invDet, (m.elements[0][1] * m.elements[1][2] - m.elements[0][2] * m.elements[1][1]) * invDet, (m.elements[1][2] * m.elements[2][0] - m.elements[1][0] * m.elements[2][2]) * invDet, (m.elements[0][0] * m.elements[2][2] - m.elements[0][2] * m.elements[2][0]) * invDet, (m.elements[0][2] * m.elements[1][0] - m.elements[0][0] * m.elements[1][2]) * invDet, (m.elements[1][0] * m.elements[2][1] - m.elements[1][1] * m.elements[2][0]) * invDet, (m.elements[0][1] * m.elements[2][0] - m.elements[0][0] * m.elements[2][1]) * invDet, (m.elements[0][0] * m.elements[1][1] - m.elements[0][1] * m.elements[1][0]) * invDet) : Matrix<T, N>();
            }

            LAMBDA_HOST_DEVICE friend Matrix<T, N> inverse(const Matrix<T, N> & m) requires (N == 4) {
                using std::abs;
                using std::fabs;
                using std::sqrt;

                T det = m.determinant();
                T invDet = T(1) / det;

                T s0 = m.elements[0][0] * m.elements[1][1] - m.elements[0][1] * m.elements[1][0];
                T s1 = m.elements[0][0] * m.elements[1][2] - m.elements[0][2] * m.elements[1][0];
                T s2 = m.elements[0][0] * m.elements[1][3] - m.elements[0][3] * m.elements[1][0];
                T s3 = m.elements[0][1] * m.elements[1][2] - m.elements[0][2] * m.elements[1][1];
                T s4 = m.elements[0][1] * m.elements[1][3] - m.elements[0][3] * m.elements[1][1];
                T s5 = m.elements[0][2] * m.elements[1][3] - m.elements[0][3] * m.elements[1][2];

                T c0 = m.elements[2][0] * m.elements[3][1] - m.elements[2][1] * m.elements[3][0];
                T c1 = m.elements[2][0] * m.elements[3][2] - m.elements[2][2] * m.elements[3][0];
                T c2 = m.elements[2][0] * m.elements[3][3] - m.elements[2][3] * m.elements[3][0];
                T c3 = m.elements[2][1] * m.elements[3][2] - m.elements[2][2] * m.elements[3][1];
                T c4 = m.elements[2][1] * m.elements[3][3] - m.elements[2][3] * m.elements[3][1];
                T c5 = m.elements[2][2] * m.elements[3][3] - m.elements[2][3] * m.elements[3][2];

                T scale = 1;

                for (int i = 0; i < N; i++){
                    T sum = 0;

                    for (int j = 0; j < N; j++) sum += abs(m.elements[i][j]) * abs(m.elements[i][j]);

                    scale *= sqrt(sum);
                }

                return abs(det) > constants::EPSILON_RELATIVE * scale ? Matrix<T, N>((m.elements[1][1] * c5 - m.elements[1][2] * c4 + m.elements[1][3] * c3) * invDet, (-m.elements[0][1] * c5 + m.elements[0][2] * c4 - m.elements[0][3] * c3) * invDet, (m.elements[3][1] * s5 - m.elements[3][2] * s4 + m.elements[3][3] * s3) * invDet, (-m.elements[2][1] * s5 + m.elements[2][2] * s4 - m.elements[2][3] * s3) * invDet, (-m.elements[1][0] * c5 + m.elements[1][2] * c2 - m.elements[1][3] * c1) * invDet, (m.elements[0][0] * c5 - m.elements[0][2] * c2 + m.elements[0][3] * c1) * invDet, (-m.elements[3][0] * s5 + m.elements[3][2] * s2 - m.elements[3][3] * s1) * invDet, (m.elements[2][0] * s5 - m.elements[2][2] * s2 + m.elements[2][3] * s1) * invDet, (m.elements[1][0] * c4 - m.elements[1][1] * c2 + m.elements[1][3] * c0) * invDet, (-m.elements[0][0] * c4 + m.elements[0][1] * c2 - m.elements[0][3] * c0) * invDet, (m.elements[3][0] * s4 - m.elements[3][1] * s2 + m.elements[3][3] * s0) * invDet, (-m.elements[2][0] * s4 + m.elements[2][1] * s2 - m.elements[2][3] * s0) * invDet, (-m.elements[1][0] * c3 + m.elements[1][1] * c1 - m.elements[1][2] * c0) * invDet, (m.elements[0][0] * c3 - m.elements[0][1] * c1 + m.elements[0][2] * c0) * invDet, (-m.elements[3][0] * s3 + m.elements[3][1] * s1 - m.elements[3][2] * s0) * invDet, (m.elements[2][0] * s3 - m.elements[2][1] * s1 + m.elements[2][2] * s0) * invDet) : Matrix<T, N>();
            }

            LAMBDA_HOST_DEVICE friend Matrix<T, N> inverse(const Matrix<T, N> & m1) requires (N > 4) {
                using std::abs;
                using std::fabs;

                T scale = 0;

                for (int i = 0; i < N; i++)
                    for (int j = 0; j < N; j++) scale = std::max(scale, abs(m1.elements[i][j]));

                Matrix<T, N> m2(m1);
                Matrix<T, N> m3;

                for (int i = 0; i < N; i++) {
                    int pivot = i;

                    for (int j = i + 1; j < N; j++)
                        if (abs(m2.elements[j][i]) > abs(m2.elements[pivot][i])) pivot = j;

                    if (abs(m2.elements[pivot][i]) <= constants::EPSILON_RELATIVE * scale) return Matrix<T, N>();

                    if (pivot != i)
                        for (int j = 0; j < N; j++) {
                            T temp = m2.elements[i][j];
                            m2.elements[i][j] = m2.elements[pivot][j];
                            m2.elements[pivot][j] = temp;

                            temp = m3.elements[i][j];
                            m3.elements[i][j] = m3.elements[pivot][j];
                            m3.elements[pivot][j] = temp;
                        }

                    T invPivot = T(1) / m2.elements[i][i];

                    m2.elements[i][i] = T(1);

                    for (int j = i + 1; j < N; j++) m2.elements[i][j] *= invPivot;
                    for (int j = 0; j < N; j++) m3.elements[i][j] *= invPivot;

                    for (int j = 0; j < N; j++)
                        if (j != i) {
                            T factor = m2.elements[j][i];

                            m2.elements[j][i] = T(0);

                            for (int k = i + 1; k < N; k++) m2.elements[j][k] -= factor * m2.elements[i][k];
                            for (int k = 0; k < N; k++) m3.elements[j][k] -= factor * m3.elements[i][k];
                        }
                }

                return m3;
            }

            LAMBDA_HOST_DEVICE T * data() { return &elements[0][0]; }

            LAMBDA_HOST_DEVICE const T * data() const { return &elements[0][0]; }

        private:
            T elements[N][N];
    };
}