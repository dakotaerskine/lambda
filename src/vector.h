#pragma once

#include <cassert>
#include <cmath>

#include "platform.h"

class Vector {
    public:
        HOST_DEVICE Vector() { data[0] = data[1] = data[2] = 0; }

        HOST_DEVICE Vector(const Vector & v) {
            data[0] = v.data[0];
            data[1] = v.data[1];
            data[2] = v.data[2];
        }

        HOST_DEVICE Vector(Float d0, Float d1, Float d2) {
            data[0] = d0;
            data[1] = d1;
            data[2] = d2;
        }

        HOST_DEVICE Vector & operator=(const Vector & v) {
            data[0] = v.data[0];
            data[1] = v.data[1];
            data[2] = v.data[2];
            return *this;
        }

        HOST_DEVICE Float & operator[](int i) {
            assert(i >= 0 && i < 3);
            return data[i];
        }
        
        HOST_DEVICE const Float & operator[](int i) const {
            assert(i >= 0 && i < 3);
            return data[i];
        }

        HOST_DEVICE bool operator==(const Vector & v) const { return data[0] == v.data[0] && data[1] == v.data[1] && data[2] == v.data[2]; }
        HOST_DEVICE bool operator!=(const Vector & v) const { return data[0] != v.data[0] || data[1] != v.data[1] || data[2] != v.data[2]; }

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

        HOST_DEVICE Vector & operator*=(Float d) {
            data[0] *= d;
            data[1] *= d;
            data[2] *= d;
            return *this;
        }

        HOST_DEVICE Vector & operator/=(Float d) {
            assert(d != 0);
            data[0] /= d;
            data[1] /= d;
            data[2] /= d;
            return *this;
        }

        HOST_DEVICE friend Vector operator+(const Vector & v1, const Vector & v2) { return Vector(v1.data[0] + v2.data[0], v1.data[1] + v2.data[1], v1.data[2] + v2.data[2]); }
        HOST_DEVICE friend Vector operator-(const Vector & v) { return Vector(-v.data[0], -v.data[1], -v.data[2]); }
        HOST_DEVICE friend Vector operator-(const Vector & v1, const Vector & v2) { return Vector(v1.data[0] - v2.data[0], v1.data[1] - v2.data[1], v1.data[2] - v2.data[2]); }
        HOST_DEVICE friend Vector operator*(const Vector & v, Float d) { return Vector(v.data[0] * d, v.data[1] * d, v.data[2] * d); }
        HOST_DEVICE friend Vector operator*(Float d, const Vector & v) { return v * d; }
        HOST_DEVICE friend Vector operator*(const Vector & v1, const Vector & v2) { return Vector(v1.data[0] * v2.data[0], v1.data[1] * v2.data[1], v1.data[2] * v2.data[2]); }

        HOST_DEVICE friend Vector operator/(const Vector & v, Float d) {
            assert(d != 0);
            return Vector(v.data[0] / d, v.data[1] / d, v.data[2] / d);
        }

        HOST_DEVICE Float length() const { return sqrt(data[0] * data[0] + data[1] * data[1] + data[2] * data[2]); }
        HOST_DEVICE Float lengthSquared() const { return data[0] * data[0] + data[1] * data[1] + data[2] * data[2]; }

        HOST_DEVICE Vector normalize() const {
            Float len = length();
            assert(len != 0);
            return Vector(data[0] / len, data[1] / len, data[2] / len);
        }

        HOST_DEVICE Float dot(const Vector & v) const { return data[0] * v.data[0] + data[1] * v.data[1] + data[2] * v.data[2]; }
        HOST_DEVICE Vector cross(const Vector & v) const { return Vector(data[1] * v.data[2] - data[2] * v.data[1], data[2] * v.data[0] - data[0] * v.data[2], data[0] * v.data[1] - data[1] * v.data[0]); }

    private:
        Float data[3];
};