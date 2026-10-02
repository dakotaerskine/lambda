#pragma once

#include <cmath>

#include "core/constants.h"
#include "core/platform.h"
#include "math/complex.h"

namespace lambda {
    template <typename T, int N>
    requires ((std::is_arithmetic_v<T> || std::is_same_v<T, Complex>) && N > 0)
    class Vector {
        public:
            LAMBDA_HOST_DEVICE Vector() { for (int i = 0; i < N; i++) elements[i] = T(); }

            template <typename... Ts>
            requires (sizeof...(Ts) == N && (std::is_convertible_v<Ts, T> && ...))
            LAMBDA_HOST_DEVICE Vector(Ts... d) {
                int i = 0;

                ((elements[i++] = T(d)), ...);
            }

            template <int M>
            requires (M >= N)
            LAMBDA_HOST_DEVICE Vector(const Vector<T, M> & v) { for (int i = 0; i < N; i++) elements[i] = T(v[i]); }

            template <typename... Ts, int M>
            requires (M > 0 && M < N && sizeof...(Ts) + M == N && (std::is_convertible_v<Ts, T> && ...))
            LAMBDA_HOST_DEVICE Vector(const Vector<T, M> & v, Ts... d) {
                int i;

                for (i = 0; i < M; i++) elements[i] = T(v[i]);

                ((elements[i++] = T(d)), ...);
            }

            template <typename U>
            requires (std::is_convertible_v<U, T>)
            LAMBDA_HOST_DEVICE Vector(const Vector<U, N> & v) {
                for (int i = 0; i < N; i++) elements[i] = T(v[i]);
            }

            LAMBDA_HOST_DEVICE Vector(const T * d) { for (int i = 0; i < N; i++) elements[i] = d[i]; }

            LAMBDA_HOST_DEVICE T & operator[](int i) { return elements[i]; }

            LAMBDA_HOST_DEVICE const T & operator[](int i) const { return elements[i]; }

            LAMBDA_HOST_DEVICE bool operator==(const Vector & v) const {
                for (int i = 0; i < N; i++)
                    if (elements[i] != v.elements[i]) return false;

                return true;
            }

            LAMBDA_HOST_DEVICE bool operator!=(const Vector & v) const {
                for (int i = 0; i < N; i++)
                    if (elements[i] != v.elements[i]) return true;

                return false;
            }

            LAMBDA_HOST_DEVICE Vector & operator+=(const Vector & v) {
                for (int i = 0; i < N; i++) elements[i] += v.elements[i];

                return *this;
            }

            LAMBDA_HOST_DEVICE Vector & operator-=(const Vector & v) {
                for (int i = 0; i < N; i++) elements[i] -= v.elements[i];

                return *this;
            }

            LAMBDA_HOST_DEVICE Vector & operator*=(T d) {
                for (int i = 0; i < N; i++) elements[i] *= d;

                return *this;
            }

            LAMBDA_HOST_DEVICE Vector & operator*=(const Vector & v) {
                for (int i = 0; i < N; i++) elements[i] *= v.elements[i];

                return *this;
            }

            LAMBDA_HOST_DEVICE Vector & operator/=(T d) {
                for (int i = 0; i < N; i++) elements[i] /= d;

                return *this;
            }

            LAMBDA_HOST_DEVICE friend Vector<T, N> operator+(const Vector<T, N> & v1, const Vector<T, N> & v2) {
                Vector<T, N> v3;

                for (int i = 0; i < N; i++) v3.elements[i] = v1.elements[i] + v2.elements[i];

                return v3;
            }

            LAMBDA_HOST_DEVICE friend Vector<T, N> operator-(const Vector<T, N> & v1) {
                Vector<T, N> v2;

                for (int i = 0; i < N; i++) v2.elements[i] = -v1.elements[i];

                return v2;
            }

            LAMBDA_HOST_DEVICE friend Vector<T, N> operator-(const Vector<T, N> & v1, const Vector<T, N> & v2) {
                Vector<T, N> v3;

                for (int i = 0; i < N; i++) v3.elements[i] = v1.elements[i] - v2.elements[i];

                return v3;
            }

            LAMBDA_HOST_DEVICE friend Vector<T, N> operator*(const Vector<T, N> & v1, T d) {
                Vector<T, N> v2;

                for (int i = 0; i < N; i++) v2.elements[i] = v1.elements[i] * d;

                return v2;
            }

            LAMBDA_HOST_DEVICE friend Vector<T, N> operator*(T d, const Vector<T, N> & v) { return v * d; }

            LAMBDA_HOST_DEVICE friend Vector<T, N> operator*(const Vector<T, N> & v1, const Vector<T, N> & v2) {
                Vector<T, N> v3;

                for (int i = 0; i < N; i++) v3.elements[i] = v1.elements[i] * v2.elements[i];

                return v3;
            }

            LAMBDA_HOST_DEVICE friend Vector<T, N> operator/(const Vector<T, N> & v1, T d) {
                Vector<T, N> v2;

                for (int i = 0; i < N; i++) v2.elements[i] = v1.elements[i] / d;

                return v2;
            }

            LAMBDA_HOST_DEVICE T lengthSquared() const {
                T sum = 0;

                for (int i = 0; i < N; i++) sum += elements[i] * elements[i];

                return sum;
            }

            LAMBDA_HOST_DEVICE T length() const {
                using std::sqrt;

                return sqrt(lengthSquared());
            }

            LAMBDA_HOST_DEVICE friend Vector<T, N> normalize(const Vector<T, N> & v1) { return v1 / v1.length(); }

            LAMBDA_HOST_DEVICE friend T dot(const Vector<T, N> & v1, const Vector<T, N> & v2) {
                T sum = 0;

                for (int i = 0; i < N; i++) sum += v1.elements[i] * v2.elements[i];

                return sum;
            }

            LAMBDA_HOST_DEVICE friend Vector<T, N> cross(const Vector<T, N> & v1, const Vector<T, N> & v2) requires (N == 3) {
                Vector<T, N> v3;

                v3.elements[0] = v1.elements[1] * v2.elements[2] - v1.elements[2] * v2.elements[1];
                v3.elements[1] = v1.elements[2] * v2.elements[0] - v1.elements[0] * v2.elements[2];
                v3.elements[2] = v1.elements[0] * v2.elements[1] - v1.elements[1] * v2.elements[0];

                return v3;
            }

            LAMBDA_HOST_DEVICE friend Vector<T, N> cross(const Vector<T, N> & v1, const Vector<T, N> & v2) requires (N == 7) {
                Vector<T, N> v3;

                v3.elements[0] = v1.elements[1] * v2.elements[3] - v1.elements[3] * v2.elements[1] + v1.elements[2] * v2.elements[6] - v1.elements[6] * v2.elements[2] + v1.elements[4] * v2.elements[5] - v1.elements[5] * v2.elements[4];
                v3.elements[1] = v1.elements[2] * v2.elements[4] - v1.elements[4] * v2.elements[2] + v1.elements[3] * v2.elements[0] - v1.elements[0] * v2.elements[3] + v1.elements[5] * v2.elements[6] - v1.elements[6] * v2.elements[5];
                v3.elements[2] = v1.elements[3] * v2.elements[5] - v1.elements[5] * v2.elements[3] + v1.elements[4] * v2.elements[1] - v1.elements[1] * v2.elements[4] + v1.elements[6] * v2.elements[0] - v1.elements[0] * v2.elements[6];
                v3.elements[3] = v1.elements[4] * v2.elements[6] - v1.elements[6] * v2.elements[4] + v1.elements[5] * v2.elements[2] - v1.elements[2] * v2.elements[5] + v1.elements[0] * v2.elements[1] - v1.elements[1] * v2.elements[0];
                v3.elements[4] = v1.elements[5] * v2.elements[0] - v1.elements[0] * v2.elements[5] + v1.elements[6] * v2.elements[3] - v1.elements[3] * v2.elements[6] + v1.elements[1] * v2.elements[2] - v1.elements[2] * v2.elements[1];
                v3.elements[5] = v1.elements[6] * v2.elements[1] - v1.elements[1] * v2.elements[6] + v1.elements[0] * v2.elements[4] - v1.elements[4] * v2.elements[0] + v1.elements[2] * v2.elements[3] - v1.elements[3] * v2.elements[2];
                v3.elements[6] = v1.elements[0] * v2.elements[2] - v1.elements[2] * v2.elements[0] + v1.elements[1] * v2.elements[5] - v1.elements[5] * v2.elements[1] + v1.elements[3] * v2.elements[4] - v1.elements[4] * v2.elements[3];

                return v3;
            }

            LAMBDA_HOST_DEVICE friend Vector<T, N> min(const Vector<T, N> & v1, const Vector<T, N> & v2) {
                Vector<T, N> v3;

                for (int i = 0; i < N; i++) v3.elements[i] = v1.elements[i] < v2.elements[i] ? v1.elements[i] : v2.elements[i];

                return v3;
            }

            LAMBDA_HOST_DEVICE friend Vector<T, N> max(const Vector<T, N> & v1, const Vector<T, N> & v2) {
                Vector<T, N> v3;

                for (int i = 0; i < N; i++) v3.elements[i] = v1.elements[i] > v2.elements[i] ? v1.elements[i] : v2.elements[i];

                return v3;
            }

            LAMBDA_HOST_DEVICE T * data() { return elements; }

            LAMBDA_HOST_DEVICE const T * data() const { return elements; }

        private:
            T elements[N];
    };
}