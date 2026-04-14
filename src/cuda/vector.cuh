#pragma once

#include <cmath>

class Vector {
    public:
        __host__ __device__ Vector() { data[0] = data[1] = data[2] = 0; }
        __host__ __device__ Vector(const Vector & v) {
            data[0] = v.data[0];
            data[1] = v.data[1];
            data[2] = v.data[2];
        }
        __host__ __device__ Vector(double d0, double d1, double d2) {
            data[0] = d0;
            data[1] = d1;
            data[2] = d2;
        }
        __host__ __device__ const Vector & operator=(const Vector & v) {
            data[0] = v.data[0];
            data[1] = v.data[1];
            data[2] = v.data[2];
            return *this;
        }

        __host__ __device__ double & operator[](int i) { return data[i]; }
        __host__ __device__ const double & operator[](int i) const { return data[i]; }

        __host__ __device__ bool operator==(const Vector & v) const { return data[0] == v.data[0] && data[1] == v.data[1] && data[2] == v.data[2]; }
        __host__ __device__ bool operator!=(const Vector & v) const { return data[0] != v.data[0] || data[1] != v.data[1] || data[2] != v.data[2]; }

        __host__ __device__ Vector & operator+=(const Vector & v) {
            data[0] += v.data[0];
            data[1] += v.data[1];
            data[2] += v.data[2];
            return *this;
        }
        __host__ __device__ Vector & operator-=(const Vector & v) {
            data[0] -= v.data[0];
            data[1] -= v.data[1];
            data[2] -= v.data[2];
            return *this;
        }
        __host__ __device__ Vector & operator*=(double d) {
            data[0] *= d;
            data[1] *= d;
            data[2] *= d;
            return *this;
        }
        __host__ __device__ Vector & operator/=(double d) {
            data[0] /= d;
            data[1] /= d;
            data[2] /= d;
            return *this;
        }
        __host__ __device__ friend Vector operator+(const Vector & v1, const Vector & v2) { return Vector(v1.data[0] + v2.data[0], v1.data[1] + v2.data[1], v1.data[2] + v2.data[2]); }
        __host__ __device__ friend Vector operator-(const Vector & v) { return Vector(-v.data[0], -v.data[1], -v.data[2]); }
        __host__ __device__ friend Vector operator-(const Vector & v1, const Vector & v2) { return Vector(v1.data[0] - v2.data[0], v1.data[1] - v2.data[1], v1.data[2] - v2.data[2]); }
        __host__ __device__ friend Vector operator*(const Vector & v, double d) { return Vector(v.data[0] * d, v.data[1] * d, v.data[2] * d); }
        __host__ __device__ friend Vector operator*(double d, const Vector & v) { return v * d; }
        __host__ __device__ friend Vector operator*(const Vector & v1, const Vector & v2) { return Vector(v1.data[0] * v2.data[0], v1.data[1] * v2.data[1], v1.data[2] * v2.data[2]); }
        __host__ __device__ friend Vector operator/(const Vector & v, double d) { return Vector(v.data[0] / d, v.data[1] / d, v.data[2] / d); }

        __host__ __device__ double length() const { return sqrt(data[0] * data[0] + data[1] * data[1] + data[2] * data[2]); }
        __host__ __device__ double lengthSquared() const { return data[0] * data[0] + data[1] * data[1] + data[2] * data[2]; }
        
        __host__ __device__ void normalize() { *this /= length(); }

        __host__ __device__ double dot(const Vector & v) const { return data[0] * v.data[0] + data[1] * v.data[1] + data[2] * v.data[2]; }
        __host__ __device__ Vector cross(const Vector & v) const { return Vector(data[1] * v.data[2] - data[2] * v.data[1], data[2] * v.data[0] - data[0] * v.data[2], data[0] * v.data[1] - data[1] * v.data[0]); }

    private:
        double data[3];
};