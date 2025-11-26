#ifndef SPECTRUM_H
#define SPECTRUM_H

#include <cassert>
#include <vector>

#include "vector.h"

class Spectrum {
    public:
        Spectrum() : size(1), data(new double[1]) {}
        Spectrum(int n) : size(n), data(new double[n]) {}
        Spectrum(int n, double d) : size(n), data(new double[n]) {
            for (int i = 0; i < size; i++)
                data[i] = d;
        }
        Spectrum(const Spectrum & s) : size(s.size), data(new double[s.size]) {
            for (int i = 0; i < size; i++)
                data[i] = s.data[i];
        }
        const Spectrum & operator=(const Spectrum & s) {
            if (this != &s) {
                delete [] data;

                size = s.size;
                data = new double[s.size];
                
                for (int i = 0; i < size; i++)
                    data[i] = s.data[i];
            }

            return *this;
        }

        ~Spectrum() {
            delete [] data;
        }

        double & operator[](int i) {
            assert (i >= 0 && i < size); 
            return data[i];
        }
        const double & operator[](int i) const {
            assert (i >= 0 && i < size);
            return data[i];
        }

        double getSize() const {
            return size;
        }

        bool operator==(const Spectrum & s) const {
            assert(size == s.size);

            for (int i = 0; i < size; i++)
                if (data[i] != s.data[i]) return false;

            return true;
        }
        bool operator!=(const Spectrum & s) const {
            assert(size == s.size);

            for (int i = 0; i < size; i++)
                if (data[i] != s.data[i]) return true;

            return false;
        }

        Spectrum & operator+=(const Spectrum & s) {
            assert(size == s.size);
            
            for (int i = 0; i < size; i++)
                data[i] += s.data[i];

            return *this;
        }
        Spectrum & operator-=(const Spectrum & s) {
            assert(size == s.size);
            
            for (int i = 0; i < size; i++)
                data[i] -= s[i];

            return *this;
        }
        Spectrum & operator*=(double d) {
            for (int i = 0; i < size; i++)
                data[i] *= d;

            return *this;
        }
        Spectrum & operator/=(double d) {
            for (int i = 0; i < size; i++)
                data[i] /= d;

            return *this;
        }
        friend Spectrum operator+(const Spectrum & s1, const Spectrum & s2) {
            assert(s1.size == s2.size);
            Spectrum s3(s1.size);
            
            for (int i = 0; i < s1.size; i++)
                s3.data[i] = s1.data[i] + s2.data[i];

            return s3;
        }
        friend Spectrum operator-(const Spectrum & s1) {
            Spectrum s2(s1.size);
            
            for (int i = 0; i < s1.size; i++)
                s2.data[i] = -s1.data[i];

            return s2;
        }
        friend Spectrum operator-(const Spectrum & s1, const Spectrum & s2) {
            assert(s1.size == s2.size);
            Spectrum s3(s1.size);
            
            for (int i = 0; i < s1.size; i++)
                s3.data[i] = s1.data[i] - s2.data[i];

            return s3;
        }
        friend Spectrum operator*(const Spectrum & s1, double d) {
            Spectrum s2(s1.size);
            
            for (int i = 0; i < s1.size; i++)
                s2.data[i] = s1.data[i] * d;

            return s2;
        }
        friend Spectrum operator*(double d, const Spectrum & s1) { return s1 * d; }
        friend Spectrum operator*(const Spectrum & s1, const Spectrum & s2) {
            assert(s1.size == s2.size);
            Spectrum s3(s1.size);
            
            for (int i = 0; i < s1.size; i++)
                s3.data[i] = s1.data[i] * s2.data[i];

            return s3;
        }
        friend Spectrum operator/(const Spectrum & s1, double d) {
            Spectrum s2(s1.size);
            
            for (int i = 0; i < s1.size; i++)
                s2.data[i] = s1.data[i] / d;

            return s2;
        }

    private:
        int size;
        double * data;
};

#endif