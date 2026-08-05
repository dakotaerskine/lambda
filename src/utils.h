#pragma once

#include <cmath>

#include "constants.h"
#include "matrix.h"
#include "platform.h"
#include "random.h"
#include "spectrum.h"
#include "vector.h"

HOST_DEVICE inline Float randomDouble(Random & state) {
    return state.next();
}

HOST_DEVICE inline Vector randomUnitVector(Random & state) {
    Vector unitVector = Vector(randomDouble(state) * 2 - 1, randomDouble(state) * 2 - 1, randomDouble(state) * 2 - 1).normalize();

    return unitVector;
}

HOST_DEVICE inline Vector randomInHemisphere(const Vector & n, Random & state) {
    Float r1 = randomDouble(state);
    Float r2 = randomDouble(state);

    Float phi = 2 * PI * r1;

    Float r = sqrt(r2);

    Float x = r * cos(phi);
    Float y = r * sin(phi);
    Float z = sqrt(1 - r2);

    Vector w = n.normalize();

    Vector u = fabs(w[0]) > fabs(w[1]) ? Vector(0, 0, 1) : Vector(1, 0, 0);
    u = u.cross(w);
    Vector v = w.cross(u);

    return x * u + y * v + z * w;
}

HOST_DEVICE inline Complex squareRoot(const Complex & c) {
    #ifdef __CUDA_ARCH__
        return thrust::sqrt((thrust::complex<double>)c);
    #else
        return std::sqrt((std::complex<double>)c);
    #endif
}

HOST_DEVICE inline Float absoluteValue(const Complex & c) {
    #ifdef __CUDA_ARCH__
        return thrust::abs((thrust::complex<double>)c);
    #else
        return std::abs((std::complex<double>)c);
    #endif
}

HOST_DEVICE inline Complex exponential(const Complex & c) {
    #ifdef __CUDA_ARCH__
        return thrust::exp((thrust::complex<double>)c);
    #else
        return std::exp((std::complex<double>)c);
    #endif
}

HOST_DEVICE inline Vector reflected(const Vector & v, const Vector & n) {
    return v - 2 * v.dot(n) * n;
}

HOST_DEVICE inline Vector refracted(const Vector & v, const Vector & n, Float ratio) {
    Vector direction;
    Float vn = v.dot(n);
    Float root = 1 - ratio * ratio * (1 - vn * vn);

    if (root < 0) direction = v - 2 * vn * n;
    else direction = ratio * v - (ratio * vn + sqrt(root)) * n;

    return direction;
}

HOST_DEVICE inline Float fade(Float t) {
    return pow(t, 3) * (t * (6 * t - 15) + 10);
}

HOST_DEVICE inline int permutation(int i) {
    #ifdef __CUDA_ARCH__
        return d_PERMUTATION[i & 255];
    #else
        return PERMUTATION[i & 255];
    #endif
}

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
    int i = (int)floor(point[0]) & 255;
    int j = (int)floor(point[1]) & 255;
    int k = (int)floor(point[2]) & 255;
    Float tx = point[0] - floor(point[0]);
    Float ty = point[1] - floor(point[1]);
    Float tz = point[2] - floor(point[2]);
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
    return x - floor(x);
}

HOST_DEVICE inline Float worleyNoise(const Vector & point) {
    int xi = int(floor(point[0]));
    int yi = int(floor(point[1]));
    int zi = int(floor(point[2]));

    Float distance = 9999;

    for (int xo = -1; xo <= 1; xo++) {
        for (int yo = -1; yo <= 1; yo++) {
            for (int zo = -1; zo <= 1; zo++) {
                Vector cell(xi + xo, yi + yo, zi + zo);
                Vector feature(fract(sin(cell.dot(Vector(127.1, 311.7, 74.7))) * 43758.5453), fract(sin(cell.dot(Vector(269.5, 183.3, 246.1))) * 43758.5453), fract(sin(cell.dot(Vector(113.5, 271.9, 124.6))) * 43758.5453));
                feature += cell;
                distance = fmin(distance, (point - feature).length());
            }
        }
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
  propMatrix.get(0, 0) = exponential(Complex(0, -1) * phi);
  propMatrix.get(1, 1) = exponential(Complex(0, 1) * phi);
  return propMatrix;
}

HOST_DEVICE inline Float xyz31(int lambda, int i) {
    #ifdef __CUDA_ARCH__
        return d_CIE_XYZ_1931[lambda - CIE_LAMBDA_MIN][i];
    #else
        return CIE_XYZ_1931[lambda - CIE_LAMBDA_MIN][i];
    #endif
}

HOST_DEVICE inline Float x31(Float lambda) {
    int min = lambda;
    int max = min + 1;

    return (max - lambda) * xyz31(min, 0) + (lambda - min) * xyz31(max, 0);
}

HOST_DEVICE inline Float y31(Float lambda) {
    int min = lambda;
    int max = min + 1;

    return (max - lambda) * xyz31(min, 1) + (lambda - min) * xyz31(max, 1);
}

HOST_DEVICE inline Float z31(Float lambda) {
    int min = lambda;
    int max = min + 1;

    return (max - lambda) * xyz31(min, 2) + (lambda - min) * xyz31(max, 2);
}

HOST_DEVICE inline Float d65(Float lambda) {
    int min = lambda;
    int max = min + 1;

    Float d65Min, d65Max;

    #ifdef __CUDA_ARCH__
        d65Min = d_CIE_D65[min - 300];
        d65Max = d_CIE_D65[max - 300];
    #else
        d65Min = CIE_D65[min - 300];
        d65Max = CIE_D65[max - 300];
    #endif

    return (max - lambda) * d65Min + (lambda - min) * d65Max;
}

HOST_DEVICE inline Vector spectrumToRGB(const Spectrum & s, Float min, Float max) {
    Float x = 0, y = 0, z = 0, w = 0;

    int size = s.getSize();

    for (int i = 0; i < size; i++) {
        Float lambda = min + i * (max - min) / (size - 1);
        Float radiance = s[i];
        x += x31(lambda) * radiance;
        y += y31(lambda) * radiance;
        z += z31(lambda) * radiance;
        w += y31(lambda);
    }

    x /= w;
    y /= w;
    z /= w;

    return Vector(x * 3.2406255 - y * 1.5372080 - z * 0.4986286, -x * 0.9689307 + y * 1.8757561 + z * 0.0415175, x * 0.0557101 - y * 0.2040211 + z * 1.0569959);
}

HOST_DEVICE inline Float sRGB(Float r) {
    if (r <= 0.0031308) return 12.92 * r;
    else return 1.055 * pow(r, 1 / 2.4) - 0.055;
}