#pragma once

#include <cassert>
#include <cmath>

#include "core/constants.h"
#include "core/platform.h"

template <typename T>
class Vector {
    public:
        HOST_DEVICE Vector() { data[0] = data[1] = data[2] = 0; }

        HOST_DEVICE Vector(T d0, T d1, T d2) {
            data[0] = d0;
            data[1] = d1;
            data[2] = d2;
        }

        HOST_DEVICE T & operator[](int i) {
            assert(i >= 0 && i < 3);
            return data[i];
        }

        HOST_DEVICE const T & operator[](int i) const {
            assert(i >= 0 && i < 3);
            return data[i];
        }

        HOST_DEVICE bool operator==(const Vector & v) const { return std::fabs(data[0] - v.data[0]) < EPSILON && std::fabs(data[1] - v.data[1]) < EPSILON && std::fabs(data[2] - v.data[2]) < EPSILON; }
        HOST_DEVICE bool operator!=(const Vector & v) const { return std::fabs(data[0] - v.data[0]) >= EPSILON || std::fabs(data[1] - v.data[1]) >= EPSILON || std::fabs(data[2] - v.data[2]) >= EPSILON; }

        HOST_DEVICE Vector & operator+=(const Vector & v) {
            data[0] += v.data[0];
            data[1] += v.data[1];
            data[2] += v.data[2];
            return *this;
        }

        HOST_DEVICE Vector & operator-=(const Vector & v) {
            data[0] -= v.data[0];
            data[1] -= v.data[1];
            data[2] -= v.data[2];
            return *this;
        }

        HOST_DEVICE Vector & operator*=(T d) {
            data[0] *= d;
            data[1] *= d;
            data[2] *= d;
            return *this;
        }

        HOST_DEVICE Vector & operator/=(T d) {
            assert(std::fabs(d) > EPSILON);
            data[0] /= d;
            data[1] /= d;
            data[2] /= d;
            return *this;
        }

        HOST_DEVICE friend Vector<T> operator+(const Vector<T> & v1, const Vector<T> & v2) { return Vector<T>(v1.data[0] + v2.data[0], v1.data[1] + v2.data[1], v1.data[2] + v2.data[2]); }
        HOST_DEVICE friend Vector<T> operator-(const Vector<T> & v) { return Vector<T>(-v.data[0], -v.data[1], -v.data[2]); }
        HOST_DEVICE friend Vector<T> operator-(const Vector<T> & v1, const Vector<T> & v2) { return Vector<T>(v1.data[0] - v2.data[0], v1.data[1] - v2.data[1], v1.data[2] - v2.data[2]); }
        HOST_DEVICE friend Vector<T> operator*(const Vector<T> & v, T d) { return Vector<T>(v.data[0] * d, v.data[1] * d, v.data[2] * d); }
        HOST_DEVICE friend Vector<T> operator*(T d, const Vector<T> & v) { return v * d; }
        HOST_DEVICE friend Vector<T> operator*(const Vector<T> & v1, const Vector<T> & v2) { return Vector<T>(v1.data[0] * v2.data[0], v1.data[1] * v2.data[1], v1.data[2] * v2.data[2]); }

        HOST_DEVICE friend Vector<T> operator/(const Vector<T> & v, T d) {
            assert(std::fabs(d) > EPSILON);
            return Vector<T>(v.data[0] / d, v.data[1] / d, v.data[2] / d);
        }

        HOST_DEVICE T length() const { return std::sqrt(data[0] * data[0] + data[1] * data[1] + data[2] * data[2]); }
        HOST_DEVICE T lengthSquared() const { return data[0] * data[0] + data[1] * data[1] + data[2] * data[2]; }

        HOST_DEVICE friend Vector<T> normalize(const Vector<T> & v) {
            T len = v.length();
            assert(std::fabs(len) > EPSILON);
            return Vector<T>(v.data[0] / len, v.data[1] / len, v.data[2] / len);
        }

        HOST_DEVICE friend T dot(const Vector<T> & v1, const Vector<T> & v2) { return v1.data[0] * v2.data[0] + v1.data[1] * v2.data[1] + v1.data[2] * v2.data[2]; }
        HOST_DEVICE friend Vector<T> cross(const Vector<T> & v1, const Vector<T> & v2) { return Vector<T>(v1.data[1] * v2.data[2] - v1.data[2] * v2.data[1], v1.data[2] * v2.data[0] - v1.data[0] * v2.data[2], v1.data[0] * v2.data[1] - v1.data[1] * v2.data[0]); }

        HOST_DEVICE friend Vector<T> minV(const Vector<T> & v1, const Vector<T> & v2) { return Vector<T>(std::fmin(v1.data[0], v2.data[0]), std::fmin(v1.data[1], v2.data[1]), std::fmin(v1.data[2], v2.data[2])); }
        HOST_DEVICE friend Vector<T> maxV(const Vector<T> & v1, const Vector<T> & v2) { return Vector<T>(std::fmax(v1.data[0], v2.data[0]), std::fmax(v1.data[1], v2.data[1]), std::fmax(v1.data[2], v2.data[2])); }

    private:
        T data[3];
};