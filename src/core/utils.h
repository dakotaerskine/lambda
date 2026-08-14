#pragma once

#include <cmath>
#include <cstdint>
#include <string>

#include "core/constants.h"
#include "core/platform.h"
#include "math/matrix.h"
#include "math/random.h"
#include "math/spectrum.h"
#include "math/vector.h"

HOST_DEVICE inline Float clamp(Float x, Float min, Float max) { return x < min ? min : (x > max ? max : x); }

HOST_DEVICE inline Float randomFloat(Random & state) {
    return state.next();
}

HOST_DEVICE inline Vector randomUnitVector(Random & state) {
    Vector unitVector = normalize(Vector(randomFloat(state) * 2 - 1, randomFloat(state) * 2 - 1, randomFloat(state) * 2 - 1));

    return unitVector;
}

HOST_DEVICE inline Vector randomInHemisphere(const Vector & n, Random & state) {
    Float r1 = randomFloat(state);
    Float r2 = randomFloat(state);

    Float phi = 2 * PI * r1;

    Float r = sqrtF(r2);

    Float x = r * cosF(phi);
    Float y = r * sinF(phi);
    Float z = sqrtF(1 - r2);

    Vector w = normalize(n);

    Vector u = fabsF(w[0]) > fabsF(w[1]) ? Vector(0, 0, 1) : Vector(1, 0, 0);
    u = normalize(cross(u, w));
    Vector v = cross(w, u);

    return x * u + y * v + z * w;
}

HOST_DEVICE inline Vector randomInCone(const Vector & n, Float cosThetaMax, Random & state) {
    Float r1 = randomFloat(state);
    Float r2 = randomFloat(state);

    Float cosTheta = 1 + r1 * (cosThetaMax - 1);
    Float sinTheta = sqrtF(1 - cosTheta * cosTheta);
    Float phi = 2 * PI * r2;

    Float x = sinTheta * cosF(phi);
    Float y = sinTheta * sinF(phi);
    Float z = cosTheta;

    Vector w = normalize(n);
    Vector u = fabsF(w[0]) > fabsF(w[1]) ? Vector(0, 0, 1) : Vector(1, 0, 0);
    u = normalize(cross(u, w));
    Vector v = cross(w, u);

    return x * u + y * v + z * w;
}

HOST_DEVICE inline Float powerHeuristic(Float pdfA, Float pdfB) {
    pdfA *= pdfA;
    pdfB *= pdfB;

    return pdfA / (pdfA + pdfB);
}

HOST_DEVICE inline Vector reflected(const Vector & v, const Vector & n) {
    return v - 2 * dot(v, n) * n;
}

HOST_DEVICE inline Vector refracted(const Vector & v, const Vector & n, Float ratio) {
    Vector direction;
    Float vn = dot(v, n);
    Float root = 1 - ratio * ratio * (1 - vn * vn);

    if (root < 0) direction = v - 2 * vn * n;
    else direction = ratio * v - (ratio * vn + sqrtF(root)) * n;

    return direction;
}

HOST_DEVICE inline Float fade(Float t) { return powF(t, 3) * (t * (6 * t - 15) + 10); }
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

HOST_DEVICE inline Float interpolate(Float a, Float b, Float x) {
    return a + x * (b - a);
}

HOST_DEVICE inline Float perlinNoise(const Vector & point) {
    int i = (int)floorF(point[0]) & 255;
    int j = (int)floorF(point[1]) & 255;
    int k = (int)floorF(point[2]) & 255;
    Float tx = point[0] - floorF(point[0]);
    Float ty = point[1] - floorF(point[1]);
    Float tz = point[2] - floorF(point[2]);
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
    return x - floorF(x);
}

HOST_DEVICE inline Float worleyNoise(const Vector & point) {
    int xi = int(floorF(point[0]));
    int yi = int(floorF(point[1]));
    int zi = int(floorF(point[2]));

    Float distance = 9999;

    for (int xo = -1; xo <= 1; xo++)
        for (int yo = -1; yo <= 1; yo++)
            for (int zo = -1; zo <= 1; zo++) {
                Vector cell(Float(xi + xo), Float(yi + yo), Float(zi + zo));
                Vector feature(fract(sinF(dot(cell, Vector(Float(127.1), Float(311.7), Float(74.7)))) * Float(43758.5453)), fract(sinF(dot(cell, Vector(Float(269.5), Float(183.3), Float(246.1)))) * Float(43758.5453)), fract(sinF(dot(cell, Vector(Float(113.5), Float(271.9), Float(124.6)))) * Float(43758.5453)));
                feature += cell;
                distance = fminF(distance, (point - feature).length());
            }

    return distance;
}

HOST_DEVICE inline Matrix interfaceMatrixS(const Complex & n1, const Complex & n2, const Complex & cos1, const Complex & cos2) {
  Matrix interfaceMatrix;
  Complex r = (n1 * cos1 - n2 * cos2) / (n1 * cos1 + n2 * cos2);

  interfaceMatrix.get(0, 0) = 1;
  interfaceMatrix.get(0, 1) = r;
  interfaceMatrix.get(1, 0) = r;
  interfaceMatrix.get(1, 1) = 1;

  interfaceMatrix /= Complex(2) * n1 * cos1 / (n1 * cos1 + n2 * cos2);
  return interfaceMatrix;
}

HOST_DEVICE inline Matrix interfaceMatrixP(const Complex & n1, const Complex & n2, const Complex & cos1, const Complex & cos2) {
  Matrix interfaceMatrix;
  Complex r = (n2 * cos1 - n1 * cos2) / (n2 * cos1 + n1 * cos2);

  interfaceMatrix.get(0, 0) = 1;
  interfaceMatrix.get(0, 1) = r;
  interfaceMatrix.get(1, 0) = r;
  interfaceMatrix.get(1, 1) = 1;

  interfaceMatrix /= Complex(2) * n1 * cos1 / (n2 * cos1 + n1 * cos2);
  return interfaceMatrix;
}

HOST_DEVICE inline Matrix propagationMatrix(const Complex & phi) {
  Matrix propMatrix;
  propMatrix.get(0, 0) = expC(Complex(0, -1) * phi);
  propMatrix.get(1, 1) = expC(Complex(0, 1) * phi);
  return propMatrix;
}

HOST_DEVICE inline Float xyz31(Float lambda, int i) {
    int min = int(lambda);

    if (min == CIE_LAMBDA_MAX) return Float(CIE_XYZ_1931[(min - CIE_LAMBDA_MIN) * 3 + i]);

    int max = min + 1;

    return (Float(max) - lambda) * Float(CIE_XYZ_1931[(min - CIE_LAMBDA_MIN) * 3 + i]) + (lambda - Float(min)) * Float(CIE_XYZ_1931[(max - CIE_LAMBDA_MIN) * 3 + i]);
}

HOST_DEVICE inline Float d65(Float lambda) {
    int min = int(lambda);
    int max = min + 1;

    return (Float(max) - lambda) * Float(CIE_D65[min - CIE_D65_LAMBDA_MIN]) + (lambda - Float(min)) * Float(CIE_D65[max - CIE_D65_LAMBDA_MIN]);
}

HOST_DEVICE inline Vector spectrumToRGB(const SampledSpectrum & s, const SampledSpectrum & lambdas, Float lambdaRange) {
    Float x = 0, y = 0, z = 0;

    Float weight = lambdaRange / HERO_COUNT;

    for (int i = 0; i < HERO_COUNT; i++) {
        Float lambda = lambdas[i];
        Float radiance = s[i];

        x += xyz31(lambda, 0) * radiance * weight;
        y += xyz31(lambda, 1) * radiance * weight;
        z += xyz31(lambda, 2) * radiance * weight;
    }

    x /= Float(106.856895);
    y /= Float(106.856895);
    z /= Float(106.856895);

    return Vector(x * Float(3.2406255) - y * Float(1.5372080) - z * Float(0.4986286), -x * Float(0.9689307) + y * Float(1.8757561) + z * Float(0.0415175), x * Float(0.0557101) - y * Float(0.2040211) + z * Float(1.0569959));
}

HOST_DEVICE inline Float sRGB(Float r) {
    if (r <= 0.0031308) return Float(12.92) * r;
    else return Float(1.055) * powF(r, 1 / Float(2.4)) - Float(0.055);
}

HOST_DEVICE inline Float toneMap(Float value) { return sRGB(clamp(value, 0, 1)); }

HOST_DEVICE inline uint8_t quantize(Float value, Random & state) {
    Float dither = randomFloat(state) - Float(0.5);

    return uint8_t(clamp(int(std::round(value * 255 + dither)), 0, 255));
}

inline bool hasExtension(const std::string & output, const std::string & extension) { return output.size() >= extension.size() && output.substr(output.size() - extension.size()) == extension; }