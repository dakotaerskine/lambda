#pragma once

#include <cmath>

#include "core/constants.h"
#include "core/platform.h"

namespace lambda {
    class Complex {
        public:
            LAMBDA_HOST_DEVICE Complex(double r = 0, double i = 0) : re(r), im(i) {}

            LAMBDA_HOST_DEVICE double real() const { return re; }

            LAMBDA_HOST_DEVICE double imag() const { return im; }

            LAMBDA_HOST_DEVICE bool operator==(const Complex & c) const { return re == c.re && im == c.im; }

            LAMBDA_HOST_DEVICE bool operator!=(const Complex & c) const { return re != c.re || im != c.im; }

            LAMBDA_HOST_DEVICE Complex & operator+=(const Complex & c) {
                re += c.re;
                im += c.im;
                return *this;
            }

            LAMBDA_HOST_DEVICE Complex & operator-=(const Complex & c) {
                re -= c.re;
                im -= c.im;
                return *this;
            }

            LAMBDA_HOST_DEVICE Complex & operator*=(const Complex & c) {
                double r = re * c.re - im * c.im;
                double i = re * c.im + im * c.re;

                re = r;
                im = i;

                return *this;
            }

            LAMBDA_HOST_DEVICE Complex & operator/=(const Complex & c) {
                double denominator = c.re * c.re + c.im * c.im;

                double r = (re * c.re + im * c.im) / denominator;
                double i = (im * c.re - re * c.im) / denominator;

                re = r;
                im = i;

                return *this;
            }

            LAMBDA_HOST_DEVICE friend Complex operator+(const Complex & c1, const Complex & c2) { return Complex(c1.re + c2.re, c1.im + c2.im); }

            LAMBDA_HOST_DEVICE friend Complex operator-(const Complex & c) { return Complex(-c.re, -c.im); }

            LAMBDA_HOST_DEVICE friend Complex operator-(const Complex & c1, const Complex & c2) { return Complex(c1.re - c2.re, c1.im - c2.im); }

            LAMBDA_HOST_DEVICE friend Complex operator*(const Complex & c1, const Complex & c2) { return Complex(c1.re * c2.re - c1.im * c2.im, c1.re * c2.im + c1.im * c2.re); }

            LAMBDA_HOST_DEVICE friend Complex operator/(const Complex & c1, const Complex & c2) {
                double denominator = c2.re * c2.re + c2.im * c2.im;

                double r = (c1.re * c2.re + c1.im * c2.im) / denominator;
                double i = (c1.im * c2.re - c1.re * c2.im) / denominator;

                return Complex(r, i);
            }

            LAMBDA_HOST_DEVICE friend Float abs(const Complex & c) { return Float(std::sqrt(c.re * c.re + c.im * c.im)); }

            LAMBDA_HOST_DEVICE friend Complex exp(const Complex & c) {
                double r = std::exp(c.re);

                return Complex(r * std::cos(c.im), r * std::sin(c.im));
            }

            LAMBDA_HOST_DEVICE friend Complex sqrt(const Complex & c) {
                if (c.re == 0 && c.im == 0) return Complex(0, 0);

                double u, v;

                if (c.re >= 0) {
                    u = std::sqrt((c.re + std::sqrt(c.re * c.re + c.im * c.im)) / 2);
                    v = c.im / (2 * u);
                }
                else {
                    v = std::sqrt((-c.re + std::sqrt(c.re * c.re + c.im * c.im)) / 2);
                    if (c.im < 0) v = -v;
                    u = c.im / (2 * v);
                }

                return Complex(u, v);
            }

        private:
            double re, im;
    };
}