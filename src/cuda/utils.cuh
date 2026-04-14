#pragma once

#include <cmath>
#include <curand_kernel.h>

#include "matrix.cuh"
#include "spectrum.cuh"
#include "vector.cuh"

constexpr double EPSILON = 1e-4;
constexpr double PI = 3.14159265358979323846;

extern __constant__ int d_permutationTable[256];
extern __constant__ double d_xyz31[471][3];
extern __constant__ double d_D65[531];

__device__ inline double randomDouble(curandState * state) {
    return curand_uniform_double(state);
}

__device__ inline Vector randomUnitVector(curandState * state) {
    Vector unitVector = Vector(randomDouble(state) * 2 - 1, randomDouble(state) * 2 - 1, randomDouble(state) * 2 - 1);
    unitVector.normalize();

    return unitVector;
}

__device__ inline Vector randomInHemisphere(const Vector & n, curandState * state) {
    Vector unitVector;

    do {
        unitVector = randomUnitVector(state);
    } while (n.dot(unitVector) <= 0);

    return unitVector;
}

__host__ __device__ inline Vector reflected(const Vector & v, const Vector & n) {
    return v - 2 * v.dot(n) * n;
}

__host__ __device__ inline Vector refracted(const Vector & v, const Vector & n, double ratio) {
    Vector direction;
    double vn = v.dot(n);
    double root = 1 - ratio * ratio * (1 - vn * vn);

    if (root < 0) direction = v - 2 * vn * n;
    else direction = (ratio * vn - sqrt(root)) * n - ratio * v;

    return direction;
}

__device__ inline double fade(double t) {
    return pow(t, 3) * (t * (6 * t - 15) + 10);
}

__device__ inline int permutation(int i) {
    return d_permutationTable[i % 256];
}

__device__ inline double gradient(int hash, double x, double y, double z)
{
    switch(hash & 0xF) {
        case 0x0: return  x + y;
        case 0x1: return -x + y;
        case 0x2: return  x - y;
        case 0x3: return -x - y;
        case 0x4: return  x + z;
        case 0x5: return -x + z;
        case 0x6: return  x - z;
        case 0x7: return -x - z;
        case 0x8: return  y + z;
        case 0x9: return -y + z;
        case 0xA: return  y - z;
        case 0xB: return -y - z;
        case 0xC: return  y + x;
        case 0xD: return -y + z;
        case 0xE: return  y - x;
        case 0xF: return -y - z;
        default: return 0;
    }
}

__device__ inline double interpolate(double a, double b, double x) {
    return a + x * (b - a);
}

__device__ inline double perlin(const Vector & point) {
    Vector pointAbs(fabs(point[0] + 256), fabs(point[1] + 256), fabs(point[2] + 256));
    int i = (int)pointAbs[0] % 256;
    int j = (int)pointAbs[1] % 256;
    int k = (int)pointAbs[2] % 256;
    double tx = pointAbs[0] - floor(pointAbs[0]);
    double ty = pointAbs[1] - floor(pointAbs[1]);
    double tz = pointAbs[2] - floor(pointAbs[2]);
    double u = fade(tx);
    double v = fade(ty);
    double w = fade(tz);
    int aaa = permutation(permutation(permutation(i) + j) + k);
    int aba = permutation(permutation(permutation(i) + j + 1) + k);
    int aab = permutation(permutation(permutation(i) + j) + k + 1);
    int abb = permutation(permutation(permutation(i) + j + 1) + k + 1);
    int baa = permutation(permutation(permutation(i + 1) + j) + k);
    int bba = permutation(permutation(permutation(i + 1) + j + 1) + k);
    int bab = permutation(permutation(permutation(i + 1) + j) + k + 1);
    int bbb = permutation(permutation(permutation(i + 1) + j + 1) + k + 1);
    double x1 = interpolate(gradient(aaa, tx, ty, tz), gradient(baa, tx - 1, ty, tz), u);
    double x2 = interpolate(gradient(aba, tx, ty - 1, tz), gradient(bba, tx - 1, ty - 1, tz), u);
    double y1 = interpolate(x1, x2, v);
    x1 = interpolate(gradient(aab, tx, ty, tz - 1), gradient(bab, tx - 1, ty, tz - 1), u);
    x2 = interpolate(gradient(abb, tx, ty - 1, tz - 1), gradient(bbb, tx - 1, ty - 1, tz - 1), u);
    double y2 = interpolate(x1, x2, v);
    return (interpolate(y1, y2, w) + 1) / 2;
}

__device__ inline double fract(double x) {
    return x - floor(x);
}

__device__ inline double worley(const Vector & point) {
    int xi = int(floor(point[0]));
    int yi = int(floor(point[1]));
    int zi = int(floor(point[2]));

    double distance = 9999;

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

__device__ inline Matrix interfaceMatrixS(const thrust::complex<double> & n1, const thrust::complex<double> & n2, const thrust::complex<double> & cos1, const thrust::complex<double> & cos2) {
  Matrix interfaceMatrix;
  thrust::complex<double> r = (n1 * cos1 - n2 * cos2) / (n1 * cos1 + n2 * cos2);

  interfaceMatrix.get(0, 0) = 1;
  interfaceMatrix.get(0, 1) = r;
  interfaceMatrix.get(1, 0) = r;
  interfaceMatrix.get(1, 1) = 1;

  interfaceMatrix /= thrust::complex<double>(2) * n1 * cos1 / (n1 * cos1 + n2 * cos2);
  return interfaceMatrix;
}

__device__ inline Matrix interfaceMatrixP(const thrust::complex<double> & n1, const thrust::complex<double> & n2, const thrust::complex<double> & cos1, const thrust::complex<double> & cos2) {
  Matrix interfaceMatrix;
  thrust::complex<double> r = (n2 * cos1 - n1 * cos2) / (n2 * cos1 + n1 * cos2);

  interfaceMatrix.get(0, 0) = 1;
  interfaceMatrix.get(0, 1) = r;
  interfaceMatrix.get(1, 0) = r;
  interfaceMatrix.get(1, 1) = 1;

  interfaceMatrix /= thrust::complex<double>(2) * n1 * cos1 / (n2 * cos1 + n1 * cos2);
  return interfaceMatrix;
}

__device__ inline Matrix propagationMatrix(const thrust::complex<double> & phi) {
  Matrix propMatrix;
  propMatrix.get(0, 0) = thrust::exp(thrust::complex<double>(0, -1) * phi);
  propMatrix.get(1, 1) = thrust::exp(thrust::complex<double>(0, 1) * phi);
  return propMatrix;
}

__device__ inline double x31(double lambda) {
    int min = lambda;
    int max = min + 1;
    return (max - lambda) * d_xyz31[min - 360][0] + (lambda - min) * d_xyz31[max - 360][0];
}

__device__ inline double y31(double lambda) {
    int min = lambda;
    int max = min + 1;
    return (max - lambda) * d_xyz31[min - 360][1] + (lambda - min) * d_xyz31[max - 360][1];
}

__device__ inline double z31(double lambda) {
    int min = lambda;
    int max = min + 1;
    return (max - lambda) * d_xyz31[min - 360][2] + (lambda - min) * d_xyz31[max - 360][2];
}

__device__ inline double d65(double lambda) {
    int min = lambda;
    int max = min + 1;
    return (max - lambda) * d_D65[min - 300] + (lambda - min) * d_D65[max - 300];
}

__device__ inline Vector spectrumToRGB(const Spectrum & s, double min, double max) {
    double x = 0, y = 0, z = 0, w = 0;

    for (int i = 0; i < 9; i++) {
        double lambda = min + i * (max - min) / 8;
        double reflectance = s[i];
        double illuminant = d65(lambda);
        double yIlluminant = y31(lambda) * illuminant;
        x += x31(lambda) * reflectance * illuminant;
        y += yIlluminant * reflectance;
        z += z31(lambda) * reflectance * illuminant;
        w += yIlluminant;
    }

    x /= w;
    y /= w;
    z /= w;

    return Vector(x * 3.2406255 - y * 1.5372080 - z * 0.4986286, -x * 0.9689307 + y * 1.8757561 + z * 0.0415175, x * 0.0557101 - y * 0.2040211 + z * 1.0569959);
}

__device__ inline double sRGB(double r) {
    if (r <= 0.0031308) return 12.92 * r;
    else return 1.055 * pow(r, 1 / 2.4) - 0.055;
}

__device__ inline Vector sRGBToHSV(const Vector & v) {
    double min = fmin(fmin(v[0], v[1]), v[2]);
    double max = fmax(fmax(v[0], v[1]), v[2]);
    double delta = max - min;

    if (delta == 0) return Vector(0, 0, 0);

    double hue = (max == v[0]) ? (v[1] - v[2]) / delta : (max == v[1]) ? (v[2] - v[0]) / delta + 2 : (v[0] - v[1]) / delta + 4;
    hue /= 6;
    if (hue < 0) hue += 1;

    double saturation = (max == 0) ? 0 : delta / max;
    double value = max;

    return Vector(hue, saturation, value);
}

__device__ inline Vector HSVTosRGB(const Vector & v) {
    if (v[1] == 0) return Vector(v[2], v[2], v[2]);

    double h6 = v[0] * 6;
    int i = h6;
    double f = h6 - i;

    double p = v[2] * (1 - v[1]);
    double q = v[2] * (1 - v[1] * f);
    double t = v[2] * (1 - v[1] * (1 - f));

    double r, g, b;

    switch (i) {
        case 0: r = v[2]; g = t; b = p; break;
        case 1: r = q; g = v[2]; b = p; break;
        case 2: r = p; g = v[2]; b = t; break;
        case 3: r = p; g = q; b = v[2]; break;
        case 4: r = t; g = p; b = v[2]; break;
        case 5: r = v[2]; g = p; b = q; break;
        default: r = g = b = 0; break;
    }

    return Vector(r, g, b);
}