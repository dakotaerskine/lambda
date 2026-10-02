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
        LAMBDA_HOST_DEVICE inline Float clamp(Float x, Float min, Float max) { return std::fmin(std::fmax(x, min), max); }

        template <typename T>
        LAMBDA_HOST_DEVICE inline T interpolate(T a, T b, Float t) { return (1 - t) * a + t * b; }

        LAMBDA_HOST_DEVICE inline Float randomFloat(Random & state) { return state.next(); }

        LAMBDA_HOST_DEVICE inline Float randomInLinear(Float a, Float b, Random & state) {
            Float u = randomFloat(state);

            if (u == 0 && a == 0) return 0;

            Float x = u * (a + b) / (a + std::sqrt(interpolate(a * a, b * b, u)));

            return std::fmin(x, 1 - constants::EPSILON);
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

        LAMBDA_HOST_DEVICE inline Float powerHeuristic(Float pdfA, Float pdfB) {
            pdfA *= pdfA;
            pdfB *= pdfB;

            return pdfA / (pdfA + pdfB);
        }

        LAMBDA_HOST_DEVICE inline Vector<Float, 3> reflected(const Vector<Float, 3> & v, const Vector<Float, 3> & n) {
            return v - 2 * dot(v, n) * n;
        }

        LAMBDA_HOST_DEVICE inline Vector<Float, 3> refracted(const Vector<Float, 3> & v, const Vector<Float, 3> & n, Float ratio) {
            Float vn = dot(v, n);
            Float root = 1 - ratio * ratio * (1 - vn * vn);

            return root < 0 ? v - 2 * vn * n : ratio * v - (ratio * vn + std::sqrt(root)) * n;
        }

        LAMBDA_HOST_DEVICE inline Vector<Float, 3> ggxNormal(const Vector<Float, 3> & v, const Vector<Float, 3> & normal, const Vector<Float, 3> & tangent, Float alphaX, Float alphaY, Random & state) {
            Vector<Float, 3> bitangent = cross(normal, tangent);

            Vector<Float, 3> vLocal = Vector<Float, 3>(dot(v, tangent), dot(v, bitangent), dot(v, normal));

            Vector<Float, 3> vh = normalize(Vector<Float, 3>(alphaX * vLocal[0], alphaY * vLocal[1], vLocal[2]));

            Float lengthSquared = vh[0] * vh[0] + vh[1] * vh[1];

            Vector<Float, 3> t1 = lengthSquared > 0 ? Vector<Float, 3>(-vh[1], vh[0], 0) / std::sqrt(lengthSquared) : Vector<Float, 3>(1, 0, 0);
            Vector<Float, 3> t2 = cross(vh, t1);

            Float r = std::sqrt(randomFloat(state));
            Float phi = 2 * constants::PI * randomFloat(state);

            Float t1Disk = r * std::cos(phi);
            Float t2Disk = r * std::sin(phi);

            Float s = Float(0.5) * (1 + vh[2]);

            t2Disk = (1 - s) * std::sqrt(std::fmax(1 - t1Disk * t1Disk, Float(0))) + s * t2Disk;

            Vector<Float, 3> nh = t1Disk * t1 + t2Disk * t2 + std::sqrt(std::fmax(1 - t1Disk * t1Disk - t2Disk * t2Disk, Float(0))) * vh;

            Vector<Float, 3> nLocal = normalize(Vector<Float, 3>(alphaX * nh[0], alphaY * nh[1], std::fmax(nh[2], Float(0))));

            return nLocal[0] * tangent + nLocal[1] * bitangent + nLocal[2] * normal;
        }

        LAMBDA_HOST_DEVICE inline Float ggxD(const Vector<Float, 3> & mLocal, Float alphaX, Float alphaY) {
            if (mLocal[2] < constants::EPSILON) return 0;

            Float x2 = mLocal[0] * mLocal[0] / (alphaX * alphaX);
            Float y2 = mLocal[1] * mLocal[1] / (alphaY * alphaY);
            Float z2 = mLocal[2] * mLocal[2];

            Float denominator = x2 + y2 + z2;

            return 1 / (constants::PI * alphaX * alphaY * denominator * denominator);
        }

        LAMBDA_HOST_DEVICE inline Float ggxLambda(const Vector<Float, 3> & vLocal, Float alphaX, Float alphaY) {
            if (vLocal[2] < constants::EPSILON) return 0;

            Float x2 = vLocal[0] * vLocal[0] * (alphaX * alphaX);
            Float y2 = vLocal[1] * vLocal[1] * (alphaY * alphaY);
            Float z2 = vLocal[2] * vLocal[2];

            return Float(0.5) * (std::sqrt(1 + (x2 + y2) / z2) - 1);
        }

        LAMBDA_HOST_DEVICE inline Float ggxG1(const Vector<Float, 3> & vLocal, Float alphaX, Float alphaY) { return 1 / (1 + ggxLambda(vLocal, alphaX, alphaY)); }

        LAMBDA_HOST_DEVICE inline Float ggxG2(const Vector<Float, 3> & vInLocal, const Vector<Float, 3> & vOutLocal, Float alphaX, Float alphaY) { return 1 / (1 + ggxLambda(vInLocal, alphaX, alphaY) + ggxLambda(vOutLocal, alphaX, alphaY)); }

        LAMBDA_HOST_DEVICE inline Matrix<Float, 4> translationMatrix(const Vector<Float, 3> & translation) { return Matrix<Float, 4>(1, 0, 0, translation[0], 0, 1, 0, translation[1], 0, 0, 1, translation[2], 0, 0, 0, 1); }

        LAMBDA_HOST_DEVICE inline Matrix<Float, 4> rotationMatrix(const Vector<Float, 3> & rotation) {
            Float sinX = std::sin(rotation[0]), cosX = std::cos(rotation[0]);
            Float sinY = std::sin(rotation[1]), cosY = std::cos(rotation[1]);
            Float sinZ = std::sin(rotation[2]), cosZ = std::cos(rotation[2]);

            Matrix<Float, 4> rotateMatrixX = Matrix<Float, 4>(1, 0, 0, 0, 0, cosX, -sinX, 0, 0, sinX, cosX, 0, 0, 0, 0, 1);
            Matrix<Float, 4> rotateMatrixY = Matrix<Float, 4>(cosY, 0, sinY, 0, 0, 1, 0, 0, -sinY, 0, cosY, 0, 0, 0, 0, 1);
            Matrix<Float, 4> rotateMatrixZ = Matrix<Float, 4>(cosZ, -sinZ, 0, 0, sinZ, cosZ, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);

            return rotateMatrixZ * rotateMatrixY * rotateMatrixX;
        }

        LAMBDA_HOST_DEVICE inline Matrix<Float, 4> scaleMatrix(const Vector<Float, 3> & scale) { return Matrix<Float, 4>(scale[0], 0, 0, 0, 0, scale[1], 0, 0, 0, 0, scale[2], 0, 0, 0, 0, 1); }

        LAMBDA_HOST_DEVICE inline Float fade(Float t) { return t * t * t * (t * (6 * t - 15) + 10); }

        LAMBDA_HOST_DEVICE inline int permutation(int i) { return constants::PERMUTATION[i & 255]; }

        LAMBDA_HOST_DEVICE inline Float gradient(int hash, Float x, Float y, Float z)
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

        LAMBDA_HOST_DEVICE inline Float perlinNoise(const Vector<Float, 3> & point) {
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

        LAMBDA_HOST_DEVICE inline Float worleyHash(int x, int y, int z, int salt) {
            int h = permutation(permutation(permutation((x & 255) + salt) + (y & 255)) + (z & 255));
            return Float(h) / Float(255);
        }

        LAMBDA_HOST_DEVICE inline Float worleyNoise(const Vector<Float, 3> & point) {
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

                        Vector<Float, 3> feature(Float(cx) + worleyHash(cx, cy, cz, 0), Float(cy) + worleyHash(cx, cy, cz, 97), Float(cz) + worleyHash(cx, cy, cz, 193));

                        minDistanceSquared = std::fmin(minDistanceSquared, (point - feature).lengthSquared());
                    }

            return clamp(std::sqrt(minDistanceSquared), Float(0), Float(1));
        }

        LAMBDA_HOST_DEVICE inline Matrix<Complex, 2> interfaceMatrixS(const Complex & n1, const Complex & n2, const Complex & cos1, const Complex & cos2) {
        Complex r = (n1 * cos1 - n2 * cos2) / (n1 * cos1 + n2 * cos2);

        Matrix<Complex, 2> interfaceMatrix(1, r, r, 1);

        interfaceMatrix /= Complex(2) * n1 * cos1 / (n1 * cos1 + n2 * cos2);

        return interfaceMatrix;
        }

        LAMBDA_HOST_DEVICE inline Matrix<Complex, 2> interfaceMatrixP(const Complex & n1, const Complex & n2, const Complex & cos1, const Complex & cos2) {
        Complex r = (n2 * cos1 - n1 * cos2) / (n2 * cos1 + n1 * cos2);

        Matrix<Complex, 2> interfaceMatrix(1, r, r, 1);

        interfaceMatrix /= Complex(2) * n1 * cos1 / (n2 * cos1 + n1 * cos2);

        return interfaceMatrix;
        }

        LAMBDA_HOST_DEVICE inline Matrix<Complex, 2> propagationMatrix(const Complex & phi) { return Matrix<Complex, 2>(exp(Complex(0, -1) * phi), 0, 0, exp(Complex(0, 1) * phi)); }

        LAMBDA_HOST_DEVICE inline double xyz31(int lambda, int i) { return constants::CIE_XYZ_1931[(lambda - constants::CIE_LAMBDA_MIN) * 3 + i]; }

        LAMBDA_HOST_DEVICE inline Float xyz31(Float lambda, int i) {
            int min = int(lambda);

            if (min == constants::CIE_LAMBDA_MAX) return Float(xyz31(min, i));

            int max = min + 1;

            return interpolate(Float(xyz31(min, i)), Float(xyz31(max, i)), lambda - Float(min));
        }

        inline Matrix<double, 3> toXYZMatrix(ColorSpace space) {
            if (space == ColorSpace::SRGB) return Matrix<double, 3>(constants::SRGB_TO_XYZ);
            else if (space == ColorSpace::REC2020) return Matrix<double, 3>(constants::REC2020_TO_XYZ);
            else if (space == ColorSpace::ACES2065) return Matrix<double, 3>(constants::ACES2065_TO_XYZ);

            return Matrix<double, 3>();
        }

        inline Matrix<double, 3> toXYZMatrix(double rx, double ry, double gx, double gy, double bx, double by, double wx, double wy) {
            double xr = rx / ry;
            double yr = 1;
            double zr = (1 - rx - ry) / ry;
            double xg = gx / gy;
            double yg = 1;
            double zg = (1 - gx - gy) / gy;
            double xb = bx / by;
            double yb = 1;
            double zb = (1 - bx - by) / by;
            double xw = wx / wy;
            double yw = 1;
            double zw = (1 - wx - wy) / wy;

            double determinant = xr * (yg * zb - yb * zg) - xg * (yr * zb - yb * zr) + xb * (yr * zg - yg * zr);

            double sr = (xw * (yg * zb - yb * zg) - xg * (yw * zb - yb * zw) + xb * (yw * zg - yg * zw)) / determinant;
            double sg = (xr * (yw * zb - yb * zw) - xw * (yr * zb - yb * zr) + xb * (yr * zw - yw * zr)) / determinant;
            double sb = (xr * (yg * zw - yw * zg) - xg * (yr * zw - yw * zr) + xw * (yr * zg - yg * zr)) / determinant;

            return Matrix<double, 3>(sr * xr, sg * xg, sb * xb, sr * yr, sg * yg, sb * yb, sr * zr, sg * zg, sb * zb);
        }

        inline Matrix<double, 3> toXYZMatrix(const double chromaticities[8]) { return toXYZMatrix(chromaticities[0], chromaticities[1], chromaticities[2], chromaticities[3], chromaticities[4], chromaticities[5], chromaticities[6], chromaticities[7]); }

        inline Matrix<double, 3> toRGBMatrix(ColorSpace space) {
            if (space == ColorSpace::SRGB) return Matrix<double, 3>(constants::XYZ_TO_SRGB);
            else if (space == ColorSpace::REC2020) return Matrix<double, 3>(constants::XYZ_TO_REC2020);
            else if (space == ColorSpace::ACES2065) return Matrix<double, 3>(constants::XYZ_TO_ACES2065);

            return Matrix<double, 3>();
        }

        LAMBDA_HOST_DEVICE inline Vector<Float, 3> spectrumToXYZ(const SampledSpectrum & s, const SampledSpectrum & lambdas, Float lambdaRange) {
            Vector<Float, 3> xyz;

            Float weight = lambdaRange / constants::HERO_COUNT;

            for (int i = 0; i < constants::HERO_COUNT; i++) {
                Float lambda = lambdas[i];
                Float radiance = s[i];

                xyz += Vector<Float, 3>(xyz31(lambda, 0), xyz31(lambda, 1), xyz31(lambda, 2)) * radiance * weight;
            }

            return xyz / constants::CIE_Y_INTEGRAL;
        }

        inline Float linearToSRGB(Float r) {
            if (r <= 0.0031308) return Float(12.92) * r;
            else return Float(1.055) * std::pow(r, 1 / Float(2.4)) - Float(0.055);
        }

        inline Float sRGBToLinear(Float r) {
            if (r <= 0.04045) return r / Float(12.92);
            else return std::pow((r + Float(0.055)) / Float(1.055), Float(2.4));
        }

        inline Float toneMap(Float value) { return linearToSRGB(clamp(value, 0, 1)); }

        inline uint8_t quantize(Float value, Random & state) {
            Float dither = randomFloat(state) - Float(0.5);

            return uint8_t(clamp(int(std::round(value * 255 + dither)), 0, 255));
        }

        inline bool hasExtension(const std::string & output, const std::string & extension) { return std::ssize(output) >= std::ssize(extension) && std::equal(extension.begin(), extension.end(), output.end() - std::ssize(extension), [](unsigned char a, unsigned char b) { return std::tolower(a) == std::tolower(b); }); }

        LAMBDA_HOST_DEVICE inline Float sigmoid(Float x) { return Float(0.5) + Float(0.5) * x / std::sqrt(1 + x * x); }

        inline double smoothStep(double x) { return x * x * (3 - 2 * x); }

        inline double illuminant(ColorSpace space, int i) {
            if (space == ColorSpace::SRGB) return constants::CIE_D65[i];
            else if (space == ColorSpace::REC2020) return constants::CIE_D65[i];
            else if (space == ColorSpace::ACES2065) return constants::CIE_D60[i];

            return 0;
        }

        inline double cieLabTransform(double t) {
            double delta = 6.0 / 29;

            if (t > delta * delta * delta) return std::cbrt(t);
            else return t / (delta * delta * 3) + 4.0 / 29;
        }

        inline Vector<double, 3> cieLab(ColorSpace space, const Vector<double, 3> & color, const Vector<double, 3> & whitepoint) {
            Vector<double, 3> xyz = toXYZMatrix(space) * color;

            return Vector<double, 3>(116 * cieLabTransform(xyz[1] / whitepoint[1]) - 16, 500 * (cieLabTransform(xyz[0] / whitepoint[0]) - cieLabTransform(xyz[1] / whitepoint[1])), 200 * (cieLabTransform(xyz[1] / whitepoint[1]) - cieLabTransform(xyz[2] / whitepoint[2])));
        }

        LAMBDA_HOST_DEVICE inline Float upsamplingScale(ColorSpace space, int i) {
            if (space == ColorSpace::SRGB) return Float(constants::UPSAMPLING_SCALE_SRGB[i]);
            else if (space == ColorSpace::REC2020) return Float(constants::UPSAMPLING_SCALE_REC2020[i]);
            else if (space == ColorSpace::ACES2065) return Float(constants::UPSAMPLING_SCALE_ACES2065[i]);

            return Float(0);
        }

        LAMBDA_HOST_DEVICE inline Float upsamplingLUT(ColorSpace space, int caseIndex, int x, int y, int z, int k) {
            int i = (((caseIndex * constants::UPSAMPLING_RESOLUTION + x) * constants::UPSAMPLING_RESOLUTION + y) * constants::UPSAMPLING_RESOLUTION + z) * 3 + k;

            if (space == ColorSpace::SRGB) return Float(constants::UPSAMPLING_LUT_SRGB[i]);
            else if (space == ColorSpace::REC2020) return Float(constants::UPSAMPLING_LUT_REC2020[i]);
            else if (space == ColorSpace::ACES2065) return Float(constants::UPSAMPLING_LUT_ACES2065[i]);

            return Float(0);
        }

        LAMBDA_HOST_DEVICE inline int upsamplingInterval(ColorSpace space, Float x) {
            int left = 0, lastInterval = constants::UPSAMPLING_RESOLUTION - 2, size = lastInterval;

            while (size > 0) {
                int half = size >> 1;
                int middle = left + half + 1;

                if (upsamplingScale(space, middle) <= x) {
                    left = middle;
                    size -= half + 1;
                }
                else size = half;
            }

            return left < lastInterval ? left : lastInterval;
        }

        LAMBDA_HOST_DEVICE inline Vector<Float, 3> upsampleRGB(ColorSpace space, const Vector<Float, 3> & color) {
            Float r = clamp(color[0], Float(0), Float(1));
            Float g = clamp(color[1], Float(0), Float(1));
            Float b = clamp(color[2], Float(0), Float(1));

            int caseIndex;

            Float x, y, z;

            if (r >= g && r >= b) {
                caseIndex = 0;
                x = r;
                y = b;
                z = g;
            }
            else if (g >= r && g >= b) {
                caseIndex = 1;
                x = g;
                y = r;
                z = b;
            }
            else {
                caseIndex = 2;
                x = b;
                y = g;
                z = r;
            }

            if (x < constants::EPSILON) return Vector<Float, 3>(upsamplingLUT(space, 0, 0, 0, 0, 0), upsamplingLUT(space, 0, 0, 0, 0, 1), upsamplingLUT(space, 0, 0, 0, 0, 2));

            Float gridScale = (constants::UPSAMPLING_RESOLUTION - 1) / x;

            y *= gridScale;
            z *= gridScale;

            int x0 = upsamplingInterval(space, x);
            int x1 = x0 + 1;
            int y0 = (int)clamp(std::floor(y), Float(0), constants::UPSAMPLING_RESOLUTION - 2);
            int y1 = y0 + 1;
            int z0 = (int)clamp(std::floor(z), Float(0), constants::UPSAMPLING_RESOLUTION - 2);
            int z1 = z0 + 1;

            Float tx = (x - upsamplingScale(space, x0)) / (upsamplingScale(space, x1) - upsamplingScale(space, x0));
            Float ty = y - Float(y0);
            Float tz = z - Float(z0);

            Vector<Float, 3> coefficients;

            for (int j = 0; j < 3; j++) {
                Float c000 = upsamplingLUT(space, caseIndex, x0, y0, z0, j);
                Float c100 = upsamplingLUT(space, caseIndex, x1, y0, z0, j);
                Float c010 = upsamplingLUT(space, caseIndex, x0, y1, z0, j);
                Float c110 = upsamplingLUT(space, caseIndex, x1, y1, z0, j);
                Float c001 = upsamplingLUT(space, caseIndex, x0, y0, z1, j);
                Float c101 = upsamplingLUT(space, caseIndex, x1, y0, z1, j);
                Float c011 = upsamplingLUT(space, caseIndex, x0, y1, z1, j);
                Float c111 = upsamplingLUT(space, caseIndex, x1, y1, z1, j);

                coefficients[j] = interpolate(interpolate(interpolate(c000, c100, tx), interpolate(c010, c110, tx), ty), interpolate(interpolate(c001, c101, tx), interpolate(c011, c111, tx), ty), tz);
            }

            return coefficients;
        }

        inline Float chromaticity(ColorSpace space, int i) {
            if (space == ColorSpace::SRGB) return Float(constants::CHROMATICITIES_SRGB[i]);
            else if (space == ColorSpace::REC2020) return Float(constants::CHROMATICITIES_REC2020[i]);
            else if (space == ColorSpace::ACES2065) return Float(constants::CHROMATICITIES_ACES2065[i]);

            return Float(0);
        }

        LAMBDA_HOST_DEVICE inline Float triangleSign(Float px, Float py, Float ax, Float ay, Float bx, Float by) { return (px - bx) * (ay - by) - (ax - bx) * (py - by); }

        LAMBDA_HOST_DEVICE inline bool pointInTriangle(Float px, Float py, Float ax, Float ay, Float bx, Float by, Float cx, Float cy) {
            Float d1 = triangleSign(px, py, ax, ay, bx, by);
            Float d2 = triangleSign(px, py, bx, by, cx, cy);
            Float d3 = triangleSign(px, py, cx, cy, ax, ay);

            bool hasNeg = (d1 < -constants::EPSILON) || (d2 < -constants::EPSILON) || (d3 < -constants::EPSILON);
            bool hasPos = (d1 > constants::EPSILON) || (d2 > constants::EPSILON) || (d3 > constants::EPSILON);

            return !(hasNeg && hasPos);
        }

        inline bool colorSpaceContains(ColorSpace space, Float rx, Float ry, Float gx, Float gy, Float bx, Float by, Float wx, Float wy) { return pointInTriangle(rx, ry, chromaticity(space, 0), chromaticity(space, 1), chromaticity(space, 2), chromaticity(space, 3), chromaticity(space, 4), chromaticity(space, 5)) && pointInTriangle(gx, gy, chromaticity(space, 0), chromaticity(space, 1), chromaticity(space, 2), chromaticity(space, 3), chromaticity(space, 4), chromaticity(space, 5)) && pointInTriangle(bx, by, chromaticity(space, 0), chromaticity(space, 1), chromaticity(space, 2), chromaticity(space, 3), chromaticity(space, 4), chromaticity(space, 5)) && pointInTriangle(wx, wy, chromaticity(space, 0), chromaticity(space, 1), chromaticity(space, 2), chromaticity(space, 3), chromaticity(space, 4), chromaticity(space, 5)); }

        inline Matrix<double, 3> bradfordAdapt(double wx1, double wy1, double wx2, double wy2) {
            Matrix<double, 3> bradford(constants::BRADFORD);
            Matrix<double, 3> inverseBradford(constants::BRADFORD_INVERSE);

            double wz1 = 1 - wx1 - wy1;
            double wz2 = 1 - wx2 - wy2;

            Vector<double, 3> source(wx1 / wy1, 1, wz1 / wy1);
            Vector<double, 3> target(wx2 / wy2, 1, wz2 / wy2);

            Vector<double, 3> sourceLMS = bradford * source;
            Vector<double, 3> targetLMS = bradford * target;

            Matrix<double, 3> scaleMatrix(targetLMS[0] / sourceLMS[0], 0, 0, 0, targetLMS[1] / sourceLMS[1], 0, 0, 0, targetLMS[2] / sourceLMS[2]);

            return inverseBradford * scaleMatrix * bradford;
        }
    }
}