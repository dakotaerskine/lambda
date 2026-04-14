#pragma once

#include "intersection.cuh"
#include "ray.cuh"
#include "spectrum.cuh"
#include "utils.cuh"
#include "vector.cuh"

class Material {
    public:
        __host__ __device__ Material() {}

        __host__ __device__ Material(const double * _n, const double * _d, const double * r, const double * s) {
            for (int i = 0; i < 5; i++)
                n[i] = _n[i];

            for (int i = 0; i < 2; i++) {
                d[i] = _d[i];
                dRanges[i] = r[i];
                noiseScales[i] = s[i];
            }
        }

        __device__ bool scatter(const Ray & r, const Intersection & i, Ray & scattered, double & attenuation, curandState * state) const {
            double reflectance = thinFilmReflectance(i.frontFacing, i.point, i.normal.dot(-r.getDirection()), r.getLambda());
            double ratio = i.frontFacing ? n[0] / n[5 - 1] : n[5 - 1] / n[0];
            Vector direction = reflected(r.getDirection(), i.normal);
            attenuation = reflectance;

            if (randomDouble(state) > reflectance) {
                direction = refracted(r.getDirection(), i.normal, ratio);
                attenuation = 1 - reflectance;
            }

            scattered = Ray(i.point, direction, r.getLambda(), r.getLambdaIndex());

            return true;
        }

    private:
        double n[5];
        double d[2];
        double dRanges[2];
        double noiseScales[2];

        __device__ double thinFilmReflectance(bool frontFacing, const Vector & point, double cos0, double lambda) const {
            int jOffset = frontFacing ? 0 : 5 - 1;
            int jSign = frontFacing ? 1 : -1;

            thrust::complex<double> cosCurrent = cos0;
            thrust::complex<double> sinCurrent;
            thrust::complex<double> sinNext = n[jOffset] / n[jOffset + jSign];
            sinNext *= sinNext * (thrust::complex<double>(1) - cosCurrent * cosCurrent);
            thrust::complex<double> cosNext = thrust::sqrt(thrust::complex<double>(1) - sinNext);
            Matrix matrices[2];
            matrices[0] = interfaceMatrixS(n[jOffset], n[jOffset + jSign], cosCurrent, cosNext);
            matrices[1] = interfaceMatrixP(n[jOffset], n[jOffset + jSign], cosCurrent, cosNext);

            for (unsigned int j = 0; j < 2; j++) {
                cosCurrent = cosNext;
                sinCurrent = sinNext;
                sinNext = n[jOffset + jSign * (j + 1)] / n[jOffset + jSign * (j + 2)];
                sinNext *= sinNext * sinCurrent;
                cosNext = thrust::sqrt(thrust::complex<double>(1) - sinNext);
                Matrix interfaceS = interfaceMatrixS(n[jOffset + jSign * (j + 1)], n[jOffset + jSign * (j + 2)], cosCurrent, cosNext);
                Matrix interfaceP = interfaceMatrixP(n[jOffset + jSign * (j + 1)], n[jOffset + jSign * (j + 2)], cosCurrent, cosNext);
                thrust::complex<double> phi = thrust::complex<double>(2 * PI / lambda) * n[jOffset + jSign * (j + 1)] * (d[jOffset + jSign * j] + dRanges[jOffset + jSign * j] * (perlin(point * noiseScales[jOffset + jSign * j]) - 0.5)) * cosCurrent;
                Matrix propagation = propagationMatrix(phi);
                matrices[0] *= propagation * interfaceS;
                matrices[1] *= propagation * interfaceP;
            }

            double R_s = thrust::abs(matrices[0].get(0, 1) / matrices[0].get(0, 0));
            double R_p = thrust::abs(matrices[1].get(0, 1) / matrices[1].get(0, 0));

            R_s *= R_s;
            R_p *= R_p;

            return (R_s + R_p) * 0.5;
        }
};