#pragma once

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>

#include <omp.h>

#include "core/constants.h"
#include "core/platform.h"
#include "math/complex.h"
#include "math/matrix.h"
#include "math/random.h"
#include "math/spectrum.h"
#include "math/vector.h"

namespace lambda {
    namespace utils {
        inline bool hasExtension(const std::string & output, const std::string & extension) { return std::ssize(output) >= std::ssize(extension) && std::equal(extension.begin(), extension.end(), output.end() - std::ssize(extension), [](unsigned char a, unsigned char b) { return std::tolower(a) == std::tolower(b); }); }

        inline Float sRGBEncode(Float r) {
            if (r <= Float(0.0031308)) return Float(12.92) * r;
            else return Float(1.055) * std::pow(r, 1 / Float(2.4)) - Float(0.055);
        }

        inline Float sRGBDecode(Float r) {
            if (r <= Float(0.04045)) return r / Float(12.92);
            else return std::pow((r + Float(0.055)) / Float(1.055), Float(2.4));
        }

        template <typename T>
        LAMBDA_HOST_DEVICE inline T clamp(T x, T minValue, T maxValue) {
            if (!(x >= minValue)) return minValue;
            if (!(x <= maxValue)) return maxValue;

            return x;
        }

        template <typename T>
        LAMBDA_HOST_DEVICE inline T interpolate(T a, T b, Float t) { return (1 - t) * a + t * b; }

        template <typename T>
        LAMBDA_HOST_DEVICE inline T smoothStep(T x) { return x * x * (3 - 2 * x); }

        LAMBDA_HOST_DEVICE inline Float randomFloat(Random & state) { return state.next(); }

        LAMBDA_HOST_DEVICE inline Float randomInLinear(Float a, Float b, Random & state) {
            Float u = randomFloat(state);

            if (u == 0 && a == 0) return 0;

            Float x = u * (a + b) / (a + std::sqrt(interpolate(a * a, b * b, u)));

            return std::fmin(x, constants::NEXT_AFTER);
        }

        LAMBDA_HOST_DEVICE inline Vector<Float, 3> randomUnitVector(Random & state) {
            Float z = randomFloat(state) * 2 - 1;
            Float phi = randomFloat(state) * 2 * constants::PI;

            Float r = std::sqrt(1 - z * z);

            return Vector<Float, 3>(r * std::cos(phi), r * std::sin(phi), z);
        }

        LAMBDA_HOST_DEVICE inline Vector<Float, 3> randomInHemisphere(const Vector<Float, 3> & n, Random & state) {
            Float r1 = randomFloat(state);
            Float r2 = randomFloat(state);

            Float phi = 2 * constants::PI * r1;

            Float r = std::sqrt(r2);

            Float x = r * std::cos(phi);
            Float y = r * std::sin(phi);
            Float z = std::sqrt(1 - r2);

            Vector<Float, 3> w = normalize(n);

            Vector<Float, 3> u = std::fabs(w[0]) > std::fabs(w[1]) ? Vector<Float, 3>(0, 0, 1) : Vector<Float, 3>(1, 0, 0);
            u = normalize(cross(u, w));
            Vector<Float, 3> v = cross(w, u);

            return x * u + y * v + z * w;
        }

        LAMBDA_HOST_DEVICE inline Vector<Float, 3> randomInCone(const Vector<Float, 3> & n, Float cosThetaMax, Random & state) {
            Float r1 = randomFloat(state);
            Float r2 = randomFloat(state);

            Float cosTheta = 1 + r1 * (cosThetaMax - 1);
            Float sinTheta = std::sqrt(1 - cosTheta * cosTheta);
            Float phi = 2 * constants::PI * r2;

            Float x = sinTheta * std::cos(phi);
            Float y = sinTheta * std::sin(phi);
            Float z = cosTheta;

            Vector<Float, 3> w = normalize(n);
            Vector<Float, 3> u = std::fabs(w[0]) > std::fabs(w[1]) ? Vector<Float, 3>(0, 0, 1) : Vector<Float, 3>(1, 0, 0);
            u = normalize(cross(u, w));
            Vector<Float, 3> v = cross(w, u);

            return x * u + y * v + z * w;
        }

        LAMBDA_HOST_DEVICE inline Vector<Float, 3> reflected(const Vector<Float, 3> & v, const Vector<Float, 3> & n) { return v - 2 * dot(v, n) * n; }

        LAMBDA_HOST_DEVICE inline Vector<Float, 3> refracted(const Vector<Float, 3> & v, const Vector<Float, 3> & n, Float ratio) {
            Float vn = dot(v, n);

            Float root = 1 - ratio * ratio * (1 - vn * vn);

            return root < 0 ? v - 2 * vn * n : ratio * v - (ratio * vn + std::sqrt(root)) * n;
        }

        LAMBDA_HOST_DEVICE inline Float sigmoid(Float x) { return Float(0.5) + Float(0.5) * x / std::sqrt(1 + x * x); }

        LAMBDA_DEVICE inline Float fade(Float t) { return t * t * t * (t * (6 * t - 15) + 10); }

        LAMBDA_DEVICE inline int permutation(int i) { return constants::PERMUTATION[i & 255]; }

        LAMBDA_DEVICE inline Float gradient(int hash, Float x, Float y, Float z)
        {
            switch(hash % 12) {
                case 0: return  x + y;
                case 1: return -x + y;
                case 2: return  x - y;
                case 3: return -x - y;
                case 4: return  x + z;
                case 5: return -x + z;
                case 6: return  x - z;
                case 7: return -x - z;
                case 8: return  y + z;
                case 9: return -y + z;
                case 10: return  y - z;
                case 11: return -y - z;
                default: return 0;
            }
        }

        LAMBDA_DEVICE inline Float perlin(const Vector<Float, 3> & point) {
            int i = (int)std::floor(point[0]) & 255;
            int j = (int)std::floor(point[1]) & 255;
            int k = (int)std::floor(point[2]) & 255;

            Float tx = point[0] - std::floor(point[0]);
            Float ty = point[1] - std::floor(point[1]);
            Float tz = point[2] - std::floor(point[2]);

            Float u = fade(tx);
            Float v = fade(ty);
            Float w = fade(tz);

            int aaa = permutation(permutation(permutation(i) + j) + k);
            int aba = permutation(permutation(permutation(i) + j + 1) + k);
            int aab = permutation(permutation(permutation(i) + j) + k + 1);
            int abb = permutation(permutation(permutation(i) + j + 1) + k + 1);
            int baa = permutation(permutation(permutation(i + 1) + j) + k);
            int bba = permutation(permutation(permutation(i + 1) + j + 1) + k);
            int bab = permutation(permutation(permutation(i + 1) + j) + k + 1);
            int bbb = permutation(permutation(permutation(i + 1) + j + 1) + k + 1);

            Float x1 = interpolate(gradient(aaa, tx, ty, tz), gradient(baa, tx - 1, ty, tz), u);
            Float x2 = interpolate(gradient(aba, tx, ty - 1, tz), gradient(bba, tx - 1, ty - 1, tz), u);

            Float y1 = interpolate(x1, x2, v);

            x1 = interpolate(gradient(aab, tx, ty, tz - 1), gradient(bab, tx - 1, ty, tz - 1), u);
            x2 = interpolate(gradient(abb, tx, ty - 1, tz - 1), gradient(bbb, tx - 1, ty - 1, tz - 1), u);

            Float y2 = interpolate(x1, x2, v);

            return (interpolate(y1, y2, w) + 1) / 2;
        }

        LAMBDA_DEVICE inline Float hash(int x, int y, int z, int salt) { return Float(permutation(permutation(permutation((x & 255) + salt) + (y & 255)) + (z & 255))) / Float(255); }

        LAMBDA_DEVICE inline Float worley(const Vector<Float, 3> & point) {
            int xi = int(std::floor(point[0]));
            int yi = int(std::floor(point[1]));
            int zi = int(std::floor(point[2]));

            Float minDistanceSquared = constants::MAX;

            for (int xo = -1; xo <= 1; xo++)
                for (int yo = -1; yo <= 1; yo++)
                    for (int zo = -1; zo <= 1; zo++) {
                        int cx = xi + xo;
                        int cy = yi + yo;
                        int cz = zi + zo;

                        Vector<Float, 3> feature(Float(cx) + hash(cx, cy, cz, 0), Float(cy) + hash(cx, cy, cz, 97), Float(cz) + hash(cx, cy, cz, 193));

                        minDistanceSquared = std::fmin(minDistanceSquared, (point - feature).lengthSquared());
                    }

            return clamp(std::sqrt(minDistanceSquared), Float(0), Float(1));
        }
    }
}