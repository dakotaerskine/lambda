#ifndef VECTOR_H
#define VECTOR_H

#include <cassert>
#include <cmath>

class Vector {
    public:
        Vector() { data[0] = data[1] = data[2] = 0; }
        Vector(const Vector & v) {
            data[0] = v.data[0];
            data[1] = v.data[1];
            data[2] = v.data[2];
        }
        Vector(double d0, double d1, double d2) {
            data[0] = d0;
            data[1] = d1;
            data[2] = d2;
        }
        const Vector & operator=(const Vector & v) {
            data[0] = v.data[0];
            data[1] = v.data[1];
            data[2] = v.data[2];
            return *this;
        }

        double & operator[](int i) {
            assert (i >= 0 && i < 3); 
            return data[i];
        }
        const double & operator[](int i) const {
            assert (i >= 0 && i < 3); 
            return data[i];
        }

        bool operator==(const Vector & v) const { return data[0] == v.data[0] && data[1] == v.data[1] && data[2] == v.data[2]; }
        bool operator!=(const Vector & v) const { return data[0] != v.data[0] || data[1] != v.data[1] || data[2] != v.data[2]; }

        Vector & operator+=(const Vector & v) {
            data[0] += v.data[0];
            data[1] += v.data[1];
            data[2] += v.data[2];
            return *this;
        }
        Vector & operator-=(const Vector & v) {
            data[0] -= v.data[0];
            data[1] -= v.data[1];
            data[2] -= v.data[2];
            return *this;
        }
        Vector & operator*=(double d) {
            data[0] *= d;
            data[1] *= d;
            data[2] *= d;
            return *this;
        }
        Vector & operator/=(double d) {
            data[0] /= d;
            data[1] /= d;
            data[2] /= d;
            return *this;
        }
        friend Vector operator+(const Vector & v1, const Vector & v2) { return Vector(v1.data[0] + v2.data[0], v1.data[1] + v2.data[1], v1.data[2] + v2.data[2]); }
        friend Vector operator-(const Vector & v) { return Vector(-v.data[0], -v.data[1], -v.data[2]); }
        friend Vector operator-(const Vector & v1, const Vector & v2) { return Vector(v1.data[0] - v2.data[0], v1.data[1] - v2.data[1], v1.data[2] - v2.data[2]); }
        friend Vector operator*(const Vector & v, double d) { return Vector(v.data[0] * d, v.data[1] * d, v.data[2] * d); }
        friend Vector operator*(double d, const Vector & v) { return v * d; }
        friend Vector operator*(const Vector & v1, const Vector & v2) { return Vector(v1.data[0] * v2.data[0], v1.data[1] * v2.data[1], v1.data[2] * v2.data[2]); }
        friend Vector operator/(const Vector & v, double d) {
            assert(d != 0);
            return Vector(v.data[0] / d, v.data[1] / d, v.data[2] / d);
        }

        double length() const { return sqrt(data[0] * data[0] + data[1] * data[1] + data[2] * data[2]); }
        double lengthSquared() const { return data[0] * data[0] + data[1] * data[1] + data[2] * data[2]; }
        
        void normalize() { *this /= length(); }

        double dot(const Vector & v) const { return data[0] * v.data[0] + data[1] * v.data[1] + data[2] * v.data[2]; }
        Vector cross(const Vector & v) const { return Vector(data[1] * v.data[2] - data[2] * v.data[1], data[2] * v.data[0] - data[0] * v.data[2], data[0] * v.data[1] - data[1] * v.data[0]); }

    private:
        double data[3];
};

#endif