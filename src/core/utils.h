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

HOST_DEVICE inline Float clamp(Float x, Float min, Float max) { return std::fmin(std::fmax(x, min), max); }

HOST_DEVICE inline Float interpolate(Float a, Float b, Float t) { return (1 - t) * a + t * b; }

HOST_DEVICE inline Float randomFloat(Random & state) {
    return state.next();
}

HOST_DEVICE inline Vector<Float> randomUnitVector(Random & state) {
    Vector<Float> unitVector = normalize(Vector<Float>(randomFloat(state) * 2 - 1, randomFloat(state) * 2 - 1, randomFloat(state) * 2 - 1));

    return unitVector;
}

HOST_DEVICE inline Vector<Float> randomInHemisphere(const Vector<Float> & n, Random & state) {
    Float r1 = randomFloat(state);
    Float r2 = randomFloat(state);

    Float phi = 2 * PI * r1;

    Float r = std::sqrt(r2);

    Float x = r * std::cos(phi);
    Float y = r * std::sin(phi);
    Float z = std::sqrt(1 - r2);

    Vector<Float> w = normalize(n);

    Vector<Float> u = std::fabs(w[0]) > std::fabs(w[1]) ? Vector<Float>(0, 0, 1) : Vector<Float>(1, 0, 0);
    u = normalize(cross(u, w));
    Vector<Float> v = cross(w, u);

    return x * u + y * v + z * w;
}

HOST_DEVICE inline Vector<Float> randomInCone(const Vector<Float> & n, Float cosThetaMax, Random & state) {
    Float r1 = randomFloat(state);
    Float r2 = randomFloat(state);

    Float cosTheta = 1 + r1 * (cosThetaMax - 1);
    Float sinTheta = std::sqrt(1 - cosTheta * cosTheta);
    Float phi = 2 * PI * r2;

    Float x = sinTheta * std::cos(phi);
    Float y = sinTheta * std::sin(phi);
    Float z = cosTheta;

    Vector<Float> w = normalize(n);
    Vector<Float> u = std::fabs(w[0]) > std::fabs(w[1]) ? Vector<Float>(0, 0, 1) : Vector<Float>(1, 0, 0);
    u = normalize(cross(u, w));
    Vector<Float> v = cross(w, u);

    return x * u + y * v + z * w;
}

HOST_DEVICE inline Float powerHeuristic(Float pdfA, Float pdfB) {
    pdfA *= pdfA;
    pdfB *= pdfB;

    return pdfA / (pdfA + pdfB);
}

HOST_DEVICE inline Vector<Float> reflected(const Vector<Float> & v, const Vector<Float> & n) {
    return v - 2 * dot(v, n) * n;
}

HOST_DEVICE inline Vector<Float> refracted(const Vector<Float> & v, const Vector<Float> & n, Float ratio) {
    Vector<Float> direction;
    Float vn = dot(v, n);
    Float root = 1 - ratio * ratio * (1 - vn * vn);

    if (root < 0) direction = v - 2 * vn * n;
    else direction = ratio * v - (ratio * vn + std::sqrt(root)) * n;

    return direction;
}

template <typename T>
HOST_DEVICE inline Vector<T> transform(const Matrix<T, 3> & m, const Vector<T> & v) { return Vector<T>(m.get(0, 0) * v[0] + m.get(0, 1) * v[1] + m.get(0, 2) * v[2], m.get(1, 0) * v[0] + m.get(1, 1) * v[1] + m.get(1, 2) * v[2], m.get(2, 0) * v[0] + m.get(2, 1) * v[1] + m.get(2, 2) * v[2]); }

template <typename T>
HOST_DEVICE inline Vector<T> transform(const Matrix<T, 4> & m, const Vector<T> & v, T w = 1) {
    T denominator = m.get(3, 0) * v[0] + m.get(3, 1) * v[1] + m.get(3, 2) * v[2] + m.get(3, 3) * w;

    if (std::fabs(denominator) < EPSILON) denominator = 1;

    return Vector<T>(m.get(0, 0) * v[0] + m.get(0, 1) * v[1] + m.get(0, 2) * v[2] + m.get(0, 3) * w, m.get(1, 0) * v[0] + m.get(1, 1) * v[1] + m.get(1, 2) * v[2] + m.get(1, 3) * w, m.get(2, 0) * v[0] + m.get(2, 1) * v[1] + m.get(2, 2) * v[2] + m.get(2, 3) * w) / denominator;
}

HOST_DEVICE inline Vector<Float> transform(const Matrix<double, 3> & m, const Vector<Float> & v) { return Vector<Float>(Float(m.get(0, 0)) * v[0] + Float(m.get(0, 1)) * v[1] + Float(m.get(0, 2)) * v[2], Float(m.get(1, 0)) * v[0] + Float(m.get(1, 1)) * v[1] + Float(m.get(1, 2)) * v[2], Float(m.get(2, 0)) * v[0] + Float(m.get(2, 1)) * v[1] + Float(m.get(2, 2)) * v[2]); }

HOST_DEVICE inline Float fade(Float t) { return t * t * t * (t * (6 * t - 15) + 10); }
HOST_DEVICE inline int permutation(int i) { return PERMUTATION[i & 255]; }

HOST_DEVICE inline Float gradient(int hash, Float x, Float y, Float z)
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

HOST_DEVICE inline Float perlinNoise(const Vector<Float> & point) {
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

HOST_DEVICE inline Float fract(Float x) {
    return x - std::floor(x);
}

HOST_DEVICE inline Float worleyNoise(const Vector<Float> & point) {
    int xi = int(std::floor(point[0]));
    int yi = int(std::floor(point[1]));
    int zi = int(std::floor(point[2]));

    Float distance = 9999;

    for (int xo = -1; xo <= 1; xo++)
        for (int yo = -1; yo <= 1; yo++)
            for (int zo = -1; zo <= 1; zo++) {
                Vector<Float> cell(Float(xi + xo), Float(yi + yo), Float(zi + zo));
                Vector<Float> feature(fract(std::sin(dot(cell, Vector<Float>(Float(127.1), Float(311.7), Float(74.7)))) * Float(43758.5453)), fract(std::sin(dot(cell, Vector<Float>(Float(269.5), Float(183.3), Float(246.1)))) * Float(43758.5453)), fract(std::sin(dot(cell, Vector<Float>(Float(113.5), Float(271.9), Float(124.6)))) * Float(43758.5453)));
                feature += cell;
                distance = std::fmin(distance, (point - feature).length());
            }

    return distance;
}

HOST_DEVICE inline Matrix<Complex, 2> interfaceMatrixS(const Complex & n1, const Complex & n2, const Complex & cos1, const Complex & cos2) {
  Complex r = (n1 * cos1 - n2 * cos2) / (n1 * cos1 + n2 * cos2);

  Matrix<Complex, 2> interfaceMatrix(1, r, r, 1);

  interfaceMatrix /= Complex(2) * n1 * cos1 / (n1 * cos1 + n2 * cos2);

  return interfaceMatrix;
}

HOST_DEVICE inline Matrix<Complex, 2> interfaceMatrixP(const Complex & n1, const Complex & n2, const Complex & cos1, const Complex & cos2) {
  Complex r = (n2 * cos1 - n1 * cos2) / (n2 * cos1 + n1 * cos2);

  Matrix<Complex, 2> interfaceMatrix(1, r, r, 1);

  interfaceMatrix /= Complex(2) * n1 * cos1 / (n2 * cos1 + n1 * cos2);

  return interfaceMatrix;
}

HOST_DEVICE inline Matrix<Complex, 2> propagationMatrix(const Complex & phi) { return Matrix<Complex, 2>(exp(Complex(0, -1) * phi), 0, 0, exp(Complex(0, 1) * phi)); }

HOST_DEVICE inline double xyz31(int lambda, int i) { return CIE_XYZ_1931[(lambda - CIE_LAMBDA_MIN) * 3 + i]; }

HOST_DEVICE inline Float xyz31(Float lambda, int i) {
    int min = int(lambda);

    if (min == CIE_LAMBDA_MAX) return Float(xyz31(min, i));

    int max = min + 1;

    return interpolate(Float(xyz31(min, i)), Float(xyz31(max, i)), lambda - Float(min));
}

inline Matrix<double, 3> toXYZMatrix(ColorSpace space) {
    if (space == ColorSpace::SRGB) return Matrix<double, 3>(SRGB_TO_XYZ);
    else if (space == ColorSpace::REC2020) return Matrix<double, 3>(REC2020_TO_XYZ);
    else if (space == ColorSpace::ACES2065) return Matrix<double, 3>(ACES2065_TO_XYZ);

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
    if (space == ColorSpace::SRGB) return Matrix<double, 3>(XYZ_TO_SRGB);
    else if (space == ColorSpace::REC2020) return Matrix<double, 3>(XYZ_TO_REC2020);
    else if (space == ColorSpace::ACES2065) return Matrix<double, 3>(XYZ_TO_ACES2065);

    return Matrix<double, 3>();
}

HOST_DEVICE inline Vector<Float> spectrumToXYZ(const SampledSpectrum & s, const SampledSpectrum & lambdas, Float lambdaRange) {
    Vector<Float> xyz;

    Float weight = lambdaRange / HERO_COUNT;

    for (int i = 0; i < HERO_COUNT; i++) {
        Float lambda = lambdas[i];
        Float radiance = s[i];

        xyz += Vector<Float>(xyz31(lambda, 0), xyz31(lambda, 1), xyz31(lambda, 2)) * radiance * weight;
    }

    return xyz / CIE_Y_INTEGRAL;
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

inline bool hasExtension(const std::string & output, const std::string & extension) { return output.size() >= extension.size() && std::equal(extension.begin(), extension.end(), output.end() - extension.size(), [](unsigned char a, unsigned char b) { return std::tolower(a) == std::tolower(b); }); }

HOST_DEVICE inline Float sigmoid(Float x) { return Float(0.5) + Float(0.5) * x / std::sqrt(1 + x * x); }

inline double smoothStep(double x) { return x * x * (3 - 2 * x); }

inline double illuminant(ColorSpace space, int i) {
    if (space == ColorSpace::SRGB) return h_CIE_D65[i];
    else if (space == ColorSpace::REC2020) return h_CIE_D65[i];
    else if (space == ColorSpace::ACES2065) return h_CIE_D60[i];

    return 0;
}

inline double cieLabTransform(double t) {
    double delta = 6.0 / 29;

    if (t > delta * delta * delta) return std::cbrt(t);
    else return t / (delta * delta * 3) + 4 / 29;
}

inline Vector<double> cieLab(ColorSpace space, const Vector<double> & color, const Vector<double> & whitepoint) {
    Vector<double> xyz = transform(toXYZMatrix(space), color);

    return Vector<double>(116 * cieLabTransform(xyz[1] / whitepoint[1]) - 16, 500 * (cieLabTransform(xyz[0] / whitepoint[0]) - cieLabTransform(xyz[1] / whitepoint[1])), 200 * (cieLabTransform(xyz[1] / whitepoint[1]) - cieLabTransform(xyz[2] / whitepoint[2])));
}

HOST_DEVICE inline Float upsamplingScale(ColorSpace space, int i) {
    if (space == ColorSpace::SRGB) return Float(UPSAMPLING_SCALE_SRGB[i]);
    else if (space == ColorSpace::REC2020) return Float(UPSAMPLING_SCALE_REC2020[i]);
    else if (space == ColorSpace::ACES2065) return Float(UPSAMPLING_SCALE_ACES2065[i]);

    return Float(0);
}

HOST_DEVICE inline Float upsamplingLUT(ColorSpace space, int caseIndex, int x, int y, int z, int k) {
    size_t i = (((size_t(caseIndex) * UPSAMPLING_RESOLUTION + x) * UPSAMPLING_RESOLUTION + y) * UPSAMPLING_RESOLUTION + z) * 3 + k;

    if (space == ColorSpace::SRGB) return Float(UPSAMPLING_LUT_SRGB[i]);
    else if (space == ColorSpace::REC2020) return Float(UPSAMPLING_LUT_REC2020[i]);
    else if (space == ColorSpace::ACES2065) return Float(UPSAMPLING_LUT_ACES2065[i]);

    return Float(0);
}

HOST_DEVICE inline int upsamplingInterval(ColorSpace space, Float x) {
    int left = 0, lastInterval = UPSAMPLING_RESOLUTION - 2, size = lastInterval;

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

HOST_DEVICE inline Vector<Float> upsampleRGB(ColorSpace space, const Vector<Float> & color) {
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

    if (x < EPSILON) return Vector<Float>(upsamplingLUT(space, 0, 0, 0, 0, 0), upsamplingLUT(space, 0, 0, 0, 0, 1), upsamplingLUT(space, 0, 0, 0, 0, 2));

    Float gridScale = (UPSAMPLING_RESOLUTION - 1) / x;

    y *= gridScale;
    z *= gridScale;

    int x0 = upsamplingInterval(space, x);
    int x1 = x0 + 1;
    int y0 = (int)clamp(std::floor(y), Float(0), UPSAMPLING_RESOLUTION - 2);
    int y1 = y0 + 1;
    int z0 = (int)clamp(std::floor(z), Float(0), UPSAMPLING_RESOLUTION - 2);
    int z1 = z0 + 1;

    Float tx = (x - upsamplingScale(space, x0)) / (upsamplingScale(space, x1) - upsamplingScale(space, x0));
    Float ty = y - Float(y0);
    Float tz = z - Float(z0);

    Vector<Float> coefficients;

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
    if (space == ColorSpace::SRGB) return Float(CHROMATICITIES_SRGB[i]);
    else if (space == ColorSpace::REC2020) return Float(CHROMATICITIES_REC2020[i]);
    else if (space == ColorSpace::ACES2065) return Float(CHROMATICITIES_ACES2065[i]);

    return Float(0);
}

HOST_DEVICE inline Float triangleSign(Float px, Float py, Float ax, Float ay, Float bx, Float by) { return (px - bx) * (ay - by) - (ax - bx) * (py - by); }

HOST_DEVICE inline bool pointInTriangle(Float px, Float py, Float ax, Float ay, Float bx, Float by, Float cx, Float cy) {
    Float d1 = triangleSign(px, py, ax, ay, bx, by);
    Float d2 = triangleSign(px, py, bx, by, cx, cy);
    Float d3 = triangleSign(px, py, cx, cy, ax, ay);

    bool hasNeg = (d1 < -EPSILON) || (d2 < -EPSILON) || (d3 < -EPSILON);
    bool hasPos = (d1 > EPSILON) || (d2 > EPSILON) || (d3 > EPSILON);

    return !(hasNeg && hasPos);
}

inline bool colorSpaceContains(ColorSpace space, Float rx, Float ry, Float gx, Float gy, Float bx, Float by, Float wx, Float wy) { return pointInTriangle(rx, ry, chromaticity(space, 0), chromaticity(space, 1), chromaticity(space, 2), chromaticity(space, 3), chromaticity(space, 4), chromaticity(space, 5)) && pointInTriangle(gx, gy, chromaticity(space, 0), chromaticity(space, 1), chromaticity(space, 2), chromaticity(space, 3), chromaticity(space, 4), chromaticity(space, 5)) && pointInTriangle(bx, by, chromaticity(space, 0), chromaticity(space, 1), chromaticity(space, 2), chromaticity(space, 3), chromaticity(space, 4), chromaticity(space, 5)) && pointInTriangle(wx, wy, chromaticity(space, 0), chromaticity(space, 1), chromaticity(space, 2), chromaticity(space, 3), chromaticity(space, 4), chromaticity(space, 5)); }

inline Matrix<double, 3> bradfordAdapt(double wx1, double wy1, double wx2, double wy2) {
    Matrix<double, 3> bradford(BRADFORD);
    Matrix<double, 3> inverseBradford(BRADFORD_INVERSE);

    double wz1 = 1 - wx1 - wy1;
    double wz2 = 1 - wx2 - wy2;

    Vector<double> source(wx1 / wy1, 1, wz1 / wy1);
    Vector<double> target(wx2 / wy2, 1, wz2 / wy2);

    Vector<double> sourceLMS = transform(bradford, source);
    Vector<double> targetLMS = transform(bradford, target);

    Matrix<double, 3> scaleMatrix(targetLMS[0] / sourceLMS[0], 0, 0, 0, targetLMS[1] / sourceLMS[1], 0, 0, 0, targetLMS[2] / sourceLMS[2]);

    return inverseBradford * scaleMatrix * bradford;
}