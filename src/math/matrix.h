#pragma once

#include "core/platform.h"
#include "math/complex.h"
#include "math/vector.h"

namespace lambda {
    template <typename T, int N>
    requires ((std::is_arithmetic_v<T> || std::is_same_v<T, Complex>) && N > 0)
    class Matrix {
        public:
            LAMBDA_HOST_DEVICE Matrix(T d = T(1)) { for (int i = 0; i < N * N; i++) elements[i] = i % (N + 1) == 0 ? d : T(0); }

            template <typename... Ts>
            requires (sizeof...(Ts) == N * N && (std::is_convertible_v<Ts, T> && ...) && N > 1)
            LAMBDA_HOST_DEVICE Matrix(Ts... d) {
                int i = 0;

                ((elements[i++] = T(d)), ...);
            }

            template <int M>
            requires (M >= N)
            LAMBDA_HOST_DEVICE Matrix(const Matrix<T, M> & m) {
                for (int i = 0; i < N * N; i++) {
                    int row = i / N;
                    int col = i % N;

                    elements[i] = T(m.get(row, col));
                }
            }

            LAMBDA_HOST_DEVICE Matrix(const T * d) { for (int i = 0; i < N * N; i++) elements[i] = d[i]; }

            LAMBDA_HOST_DEVICE Matrix(const T d[N][N]) { for (int i = 0; i < N * N; i++) elements[i] = d[i / N][i % N]; }

            template <typename U>
            requires (std::is_convertible_v<U, T>)
            LAMBDA_HOST_DEVICE Matrix(const Matrix<U, N> & m) {
                for (int i = 0; i < N * N; i++) elements[i] = T(m.get(i / N, i % N));
            }

            LAMBDA_HOST_DEVICE T & get(int i, int j) { return elements[i * N + j]; }

            LAMBDA_HOST_DEVICE const T & get(int i, int j) const { return elements[i * N + j]; }

            LAMBDA_HOST_DEVICE bool operator==(const Matrix & m) const {
                for (int i = 0; i < N * N; i++)
                    if (elements[i] != m.elements[i]) return false;

                return true;
            }

            LAMBDA_HOST_DEVICE bool operator!=(const Matrix & m) const {
                for (int i = 0; i < N * N; i++)
                    if (elements[i] != m.elements[i]) return true;

                return false;
            }

            LAMBDA_HOST_DEVICE friend Matrix<T, N> operator+(const Matrix<T, N> & m1, const Matrix<T, N> & m2) {
                Matrix<T, N> m3;

                for (int i = 0; i < N * N; i++) m3.elements[i] = m1.elements[i] + m2.elements[i];

                return m3;
            }

            LAMBDA_HOST_DEVICE friend Matrix<T, N> operator-(const Matrix<T, N> & m1, const Matrix<T, N> & m2) {
                Matrix<T, N> m3;

                for (int i = 0; i < N * N; i++) m3.elements[i] = m1.elements[i] - m2.elements[i];

                return m3;
            }

            LAMBDA_HOST_DEVICE friend Matrix<T, N> operator*(const Matrix<T, N> & m1, const Matrix<T, N> & m2) requires (N == 1) { return Matrix<T, N>(m1.elements[0] * m2.elements[0]); }

            LAMBDA_HOST_DEVICE friend Matrix<T, N> operator*(const Matrix<T, N> & m1, const Matrix<T, N> & m2) requires (N == 2) {
                Matrix<T, N> m3;

                m3.elements[0] = m1.elements[0] * m2.elements[0] + m1.elements[1] * m2.elements[2];
                m3.elements[1] = m1.elements[0] * m2.elements[1] + m1.elements[1] * m2.elements[3];
                m3.elements[2] = m1.elements[2] * m2.elements[0] + m1.elements[3] * m2.elements[2];
                m3.elements[3] = m1.elements[2] * m2.elements[1] + m1.elements[3] * m2.elements[3];

                return m3;
            }

            LAMBDA_HOST_DEVICE friend Matrix<T, N> operator*(const Matrix<T, N> & m1, const Matrix<T, N> & m2) requires (N == 3) {
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

            LAMBDA_HOST_DEVICE friend Matrix<T, N> operator*(const Matrix<T, N> & m1, const Matrix<T, N> & m2) requires (N == 4) {
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

            LAMBDA_HOST_DEVICE friend Matrix<T, N> operator*(const Matrix<T, N> & m1, const Matrix<T, N> & m2) requires (N > 4) {
                Matrix<T, N> m3;

                for (int i = 0; i < N * N; i += N)
                    for (int j = 0; j < N; j++) {
                        T sum = 0;

                        for (int k = 0; k < N; k++) sum += m1.elements[i + k] * m2.elements[k * N + j];

                        m3.elements[i + j] = sum;
                    }

                return m3;
            }

            LAMBDA_HOST_DEVICE friend Matrix<T, N> operator*(const Matrix<T, N> & m1, const T & d) {
                Matrix<T, N> m2;

                for (int i = 0; i < N * N; i++) m2.elements[i] = m1.elements[i] * d;

                return m2;
            }

            LAMBDA_HOST_DEVICE friend Matrix<T, N> operator*(const T & d, const Matrix<T, N> & m) { return m * d; }

            LAMBDA_HOST_DEVICE friend Vector<T, N> operator*(const Matrix<T, N> & m, const Vector<T, N> & v1) {
                Vector<T, N> v2;

                for (int i = 0; i < N; i++) for (int j = 0; j < N; j++) v2[i] += m.elements[i * N + j] * v1[j];

                return v2;
            }

            LAMBDA_HOST_DEVICE friend Vector<T, N> operator*(const Vector<T, N> & v1, const Matrix<T, N> & m) {
                Vector<T, N> v2;

                for (int i = 0; i < N; i++) for (int j = 0; j < N; j++) v2[i] += v1[j] * m.elements[j * N + i];

                return v2;
            }

            LAMBDA_HOST_DEVICE friend Matrix<T, N> operator/(const Matrix<T, N> & m1, const T & d) {
                Matrix<T, N> m2;

                for (int i = 0; i < N * N; i++) m2.elements[i] = m1.elements[i] / d;

                return m2;
            }

            LAMBDA_HOST_DEVICE Matrix<T, N> & operator+=(const Matrix<T, N> & m) {
                for (int i = 0; i < N * N; i++) elements[i] += m.elements[i];

                return *this;
            }

            LAMBDA_HOST_DEVICE Matrix<T, N> & operator-=(const Matrix<T, N> & m) {
                for (int i = 0; i < N * N; i++) elements[i] -= m.elements[i];

                return *this;
            }

            LAMBDA_HOST_DEVICE Matrix<T, N> & operator*=(const T & d) {
                for (int i = 0; i < N * N; i++) elements[i] *= d;

                return *this;
            }

            LAMBDA_HOST_DEVICE Matrix<T, N> & operator*=(const Matrix<T, N> & m) {
                *this = *this * m;

                return *this;
            }

            LAMBDA_HOST_DEVICE Matrix<T, N> & operator/=(const T & d) {
                for (int i = 0; i < N * N; i++) elements[i] /= d;

                return *this;
            }

            LAMBDA_HOST_DEVICE T determinant() const requires (N == 1) { return elements[0]; }

            LAMBDA_HOST_DEVICE T determinant() const requires (N == 2) { return elements[0] * elements[3] - elements[1] * elements[2]; }

            LAMBDA_HOST_DEVICE T determinant() const requires (N == 3) { return elements[0] * (elements[4] * elements[8] - elements[5] * elements[7]) - elements[1] * (elements[3] * elements[8] - elements[5] * elements[6]) + elements[2] * (elements[3] * elements[7] - elements[4] * elements[6]); }

            LAMBDA_HOST_DEVICE T determinant() const requires (N == 4) { return elements[0] * (elements[5] * (elements[10] * elements[15] - elements[11] * elements[14]) - elements[6] * (elements[9] * elements[15] - elements[11] * elements[13]) + elements[7] * (elements[9] * elements[14] - elements[10] * elements[13])) - elements[1] * (elements[4] * (elements[10] * elements[15] - elements[11] * elements[14]) - elements[6] * (elements[8] * elements[15] - elements[11] * elements[12]) + elements[7] * (elements[8] * elements[14] - elements[10] * elements[12])) + elements[2] * (elements[4] * (elements[9] * elements[15] - elements[11] * elements[13]) - elements[5] * (elements[8] * elements[15] - elements[11] * elements[12]) + elements[7] * (elements[8] * elements[13] - elements[9] * elements[12])) - elements[3] * (elements[4] * (elements[9] * elements[14] - elements[10] * elements[13]) - elements[5] * (elements[8] * elements[14] - elements[10] * elements[12]) + elements[6] * (elements[8] * elements[13] - elements[9] * elements[12])); }

            LAMBDA_HOST_DEVICE T determinant() const requires (N > 4) {
                using std::abs;
                using std::fabs;

                Matrix<T, N> m(*this);

                T det = T(1);

                for (int i = 0; i < N; i++) {
                    int pivot = i;

                    for (int j = i + 1; j < N; j++)
                        if (abs(m.elements[j * N + i]) > abs(m.elements[pivot * N + i])) pivot = j;

                    if (abs(m.elements[pivot * N + i]) < constants::EPSILON) return T(0);

                    if (pivot != i) {
                        for (int j = i; j < N; j++) {
                            T temp = m.elements[i * N + j];
                            m.elements[i * N + j] = m.elements[pivot * N + j];
                            m.elements[pivot * N + j] = temp;
                        }

                        det *= T(-1);
                    }

                    det *= m.elements[i * N + i];

                    for (int j = i + 1; j < N; j++) {
                        T factor = m.elements[j * N + i] / m.elements[i * N + i];

                        for (int k = i; k < N; k++) m.elements[j * N + k] -= factor * m.elements[i * N + k];
                    }
                }

                return det;
            }

            LAMBDA_HOST_DEVICE friend Matrix<T, N> transpose(const Matrix<T, N> & m1) {
                Matrix<T, N> m2;

                for (int i = 0; i < N; i++) for (int j = 0; j < N; j++) m2.elements[i * N + j] = m1.elements[j * N + i];

                return m2;
            }

            LAMBDA_HOST_DEVICE friend Matrix<T, N> inverse(const Matrix<T, N> & m) requires (N == 1) {
                using std::abs;
                using std::fabs;

                T det = m.determinant();
                T invDet = T(1) / det;

                return abs(det) > constants::EPSILON ? Matrix<T, N>(1 * invDet) : Matrix<T, N>();
            }

            LAMBDA_HOST_DEVICE friend Matrix<T, N> inverse(const Matrix<T, N> & m) requires (N == 2) {
                using std::abs;
                using std::fabs;

                T det = m.determinant();
                T invDet = T(1) / det;

                return abs(det) > constants::EPSILON ? Matrix<T, N>(m.elements[3] * invDet, -m.elements[1] * invDet, -m.elements[2] * invDet, m.elements[0] * invDet) : Matrix<T, N>();
            }

            LAMBDA_HOST_DEVICE friend Matrix<T, N> inverse(const Matrix<T, N> & m) requires (N == 3) {
                using std::abs;
                using std::fabs;

                T det = m.determinant();
                T invDet = T(1) / det;

                return abs(det) > constants::EPSILON ? Matrix<T, N>((m.elements[4] * m.elements[8] - m.elements[5] * m.elements[7]) * invDet, (m.elements[2] * m.elements[7] - m.elements[1] * m.elements[8]) * invDet, (m.elements[1] * m.elements[5] - m.elements[2] * m.elements[4]) * invDet, (m.elements[5] * m.elements[6] - m.elements[3] * m.elements[8]) * invDet, (m.elements[0] * m.elements[8] - m.elements[2] * m.elements[6]) * invDet, (m.elements[2] * m.elements[3] - m.elements[0] * m.elements[5]) * invDet, (m.elements[3] * m.elements[7] - m.elements[4] * m.elements[6]) * invDet, (m.elements[1] * m.elements[6] - m.elements[0] * m.elements[7]) * invDet, (m.elements[0] * m.elements[4] - m.elements[1] * m.elements[3]) * invDet) : Matrix<T, N>();
            }

            LAMBDA_HOST_DEVICE friend Matrix<T, N> inverse(const Matrix<T, N> & m) requires (N == 4) {
                using std::abs;
                using std::fabs;

                T det = m.determinant();
                T invDet = T(1) / det;

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

                return abs(det) > constants::EPSILON ? Matrix<T, N>((m.elements[5] * c5 - m.elements[6] * c4 + m.elements[7] * c3) * invDet, (-m.elements[1] * c5 + m.elements[2] * c4 - m.elements[3] * c3) * invDet, (m.elements[13] * s5 - m.elements[14] * s4 + m.elements[15] * s3) * invDet, (-m.elements[9] * s5 + m.elements[10] * s4 - m.elements[11] * s3) * invDet, (-m.elements[4] * c5 + m.elements[6] * c2 - m.elements[7] * c1) * invDet, (m.elements[0] * c5 - m.elements[2] * c2 + m.elements[3] * c1) * invDet, (-m.elements[12] * s5 + m.elements[14] * s2 - m.elements[15] * s1) * invDet, (m.elements[8] * s5 - m.elements[10] * s2 + m.elements[11] * s1) * invDet, (m.elements[4] * c4 - m.elements[5] * c2 + m.elements[7] * c0) * invDet, (-m.elements[0] * c4 + m.elements[1] * c2 - m.elements[3] * c0) * invDet, (m.elements[12] * s4 - m.elements[13] * s2 + m.elements[15] * s0) * invDet, (-m.elements[8] * s4 + m.elements[9] * s2 - m.elements[11] * s0) * invDet, (-m.elements[4] * c3 + m.elements[5] * c1 - m.elements[6] * c0) * invDet, (m.elements[0] * c3 - m.elements[1] * c1 + m.elements[2] * c0) * invDet, (-m.elements[12] * s3 + m.elements[13] * s1 - m.elements[14] * s0) * invDet, (m.elements[8] * s3 - m.elements[9] * s1 + m.elements[10] * s0) * invDet) : Matrix<T, N>();
            }

            LAMBDA_HOST_DEVICE friend Matrix<T, N> inverse(const Matrix<T, N> & m1) requires (N > 4) {
                using std::abs;
                using std::fabs;

                T scale = 0;

                for (int i = 0; i < N * N; i++) scale = std::max(scale, abs(m1.elements[i]));

                Matrix<T, N> m2(m1);
                Matrix<T, N> m3;

                for (int i = 0; i < N; i++) {
                    int pivot = i;

                    for (int j = i + 1; j < N; j++)
                        if (abs(m2.elements[j * N + i]) > abs(m2.elements[pivot * N + i])) pivot = j;

                    if (abs(m2.elements[pivot * N + i]) < constants::EPSILON * scale) return Matrix<T, N>();

                    if (pivot != i)
                        for (int j = 0; j < N; j++) {
                            T temp = m2.elements[i * N + j];
                            m2.elements[i * N + j] = m2.elements[pivot * N + j];
                            m2.elements[pivot * N + j] = temp;

                            temp = m3.elements[i * N + j];
                            m3.elements[i * N + j] = m3.elements[pivot * N + j];
                            m3.elements[pivot * N + j] = temp;
                        }

                    T invPivot = T(1) / m2.elements[i * N + i];

                    m2.elements[i * N + i] = T(1);

                    for (int j = i + 1; j < N; j++) m2.elements[i * N + j] *= invPivot;
                    for (int j = 0; j < N; j++) m3.elements[i * N + j] *= invPivot;

                    for (int j = 0; j < N; j++)
                        if (j != i) {
                            T factor = m2.elements[j * N + i];

                            m2.elements[j * N + i] = T(0);

                            for (int k = i + 1; k < N; k++) m2.elements[j * N + k] -= factor * m2.elements[i * N + k];
                            for (int k = 0; k < N; k++) m3.elements[j * N + k] -= factor * m3.elements[i * N + k];
                        }
                }

                return m3;
            }

            LAMBDA_HOST_DEVICE T * data() { return elements; }

            LAMBDA_HOST_DEVICE const T * data() const { return elements; }

        private:
            T elements[N * N];
    };
}