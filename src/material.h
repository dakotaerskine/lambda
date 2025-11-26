#ifndef MATERIAL_H
#define MATERIAL_H

#include <vector>

#include "intersection.h"
#include "ray.h"
#include "spectrum.h"
#include "utils.h"
#include "vector.h"

class Material {
    public:
        virtual bool scatter(const Ray & r, const Intersection & i, Ray & scattered, double & attenuation) const = 0;
};

class Lambertian : public Material {
    public:
        Lambertian(const Spectrum & a) : albedo(a) {}

        bool scatter(const Ray & r, const Intersection & i, Ray & scattered, double & attenuation) const override {
            scattered = Ray(i.point, randomInHemisphere(i.normal), r.getLambda(), r.getLambdaIndex());
            attenuation = albedo[r.getLambdaIndex()];

            return true;
        }
    
    private:
        Spectrum albedo;
};

class Metal : public Material {
    public:
        Metal(const Spectrum & a) : albedo(a) {}

        bool scatter(const Ray & r, const Intersection & i, Ray & scattered, double & attenuation) const override {
            scattered = Ray(i.point, reflected(r.getDirection(), i.normal), r.getLambda(), r.getLambdaIndex());
            attenuation = albedo[r.getLambdaIndex()];

            return true;
        }
    
    private:
        Spectrum albedo;
};

class Dielectric : public Material {
    public:
        Dielectric(double _n0, double _n1, const Spectrum & a) : n0(_n0), n1(_n1), albedo(a) {}

        bool scatter(const Ray & r, const Intersection & i, Ray & scattered, double & attenuation) const override {
            double ratio = i.frontFacing ? n0 / n1 : n1 / n0;
            double reflectance = schlick(r.getDirection(), i.normal);
            Vector direction = (randomDouble() < reflectance) ? reflected(r.getDirection(), i.normal) : refracted(r.getDirection(), i.normal, ratio);

            scattered = Ray(i.point, direction, r.getLambda(), r.getLambdaIndex());
            attenuation = albedo[r.getLambdaIndex()];

            return true;
        }
    
    private:
        double n0, n1;
        Spectrum albedo;

        double schlick(const Vector & v, const Vector & n) const {
            double r0 = (n0 - n1) / (n0 + n1);
            r0 *= r0;
            return r0 + (1 - r0) * pow(1 + v.dot(n), 5);
        }
};

class Emissive : public Material {
    public:
        Emissive(const Spectrum & e) : emission(e) {}

        bool scatter(const Ray & r, const Intersection & i, Ray & scattered, double & attenuation) const override {
            attenuation = emission[r.getLambdaIndex()];
            return false;
        }
    
    private:
        Spectrum emission;
};

class ThinFilm : public Material {
    public:
        ThinFilm(const std::vector<double> & _n, const std::vector<double> & _d, const std::vector<double> & r, const std::vector<double> & s) : n(_n), d(_d), dRanges(r), noiseScales(s) {}

        bool scatter(const Ray & r, const Intersection & i, Ray & scattered, double & attenuation) const override {
            double reflectance = thinFilmReflectance(i.frontFacing, i.point, i.normal.dot(-r.getDirection()), r.getLambda());
            double ratio = i.frontFacing ? n[0] / n[n.size() - 1] : n[n.size() - 1] / n[0];
            Vector direction = reflected(r.getDirection(), i.normal);
            attenuation = reflectance;

            if (randomDouble() > reflectance) {
                direction = refracted(r.getDirection(), i.normal, ratio);
                attenuation = 1 - reflectance;
            }

            scattered = Ray(i.point, direction, r.getLambda(), r.getLambdaIndex());

            return true;
        }

    private:
        std::vector<double> n, d, dRanges, noiseScales;

        double thinFilmReflectance(bool frontFacing, const Vector & point, double cos0, double lambda) const {
            int jOffset = frontFacing ? 0 : n.size() - 1;
            int jSign = frontFacing ? 1 : -1;

            std::complex<double> cosCurrent = cos0;
            std::complex<double> sinCurrent;
            std::complex<double> sinNext = n[jOffset] / n[jOffset + jSign];
            sinNext *= sinNext * (std::complex<double>(1) - cosCurrent * cosCurrent);
            std::complex<double> cosNext = sqrt(std::complex<double>(1) - sinNext);
            std::vector<Matrix> matrices(2);
            matrices[0] = interfaceMatrixS(n[jOffset], n[jOffset + jSign], cosCurrent, cosNext);
            matrices[1] = interfaceMatrixP(n[jOffset], n[jOffset + jSign], cosCurrent, cosNext);

            for (unsigned int j = 0; j < d.size(); j++) {
                cosCurrent = cosNext;
                sinCurrent = sinNext;
                sinNext = n[jOffset + jSign * (j + 1)] / n[jOffset + jSign * (j + 2)];
                sinNext *= sinNext * sinCurrent;
                cosNext = sqrt(std::complex<double>(1) - sinNext);
                Matrix interfaceS = interfaceMatrixS(n[jOffset + jSign * (j + 1)], n[jOffset + jSign * (j + 2)], cosCurrent, cosNext);
                Matrix interfaceP = interfaceMatrixP(n[jOffset + jSign * (j + 1)], n[jOffset + jSign * (j + 2)], cosCurrent, cosNext);
                std::complex<double> phi = std::complex<double>(2 * M_PI / lambda) * n[jOffset + jSign * (j + 1)] * (d[jOffset + jSign * j] + dRanges[jOffset + jSign * j] * (perlin(point * noiseScales[jOffset + jSign * j]) * perlin(point * noiseScales[jOffset + jSign * j]) - 0.5)) * cosCurrent;
                Matrix propagation = propagationMatrix(phi);
                matrices[0] *= propagation * interfaceS;
                matrices[1] *= propagation * interfaceP;
            }

            double R_s = abs(matrices[0].get(0, 1) / matrices[0].get(0, 0));
            double R_p = abs(matrices[1].get(0, 1) / matrices[1].get(0, 0));

            R_s *= R_s;
            R_p *= R_p;

            return (R_s + R_p) * 0.5;
        }
};

class Feldspar : public Material {
    public:
        Feldspar(const std::vector<double> & _n, const std::vector<double> & _d, const std::vector<double> & r, const std::vector<double> & s, Vector _normal) : n(_n), d(_d), dRanges(r), noiseScales(s), normal(_normal) {}

        bool scatter(const Ray & r, const Intersection & i, Ray & scattered, double & attenuation) const override {
            double reflectance = lamellarReflectance(i.frontFacing, i.point, normal.dot(-r.getDirection()), r.getLambda());
            double ratio = i.frontFacing ? n[0] / n[n.size() - 1] : n[n.size() - 1] / n[0];
            Vector direction = reflected(r.getDirection(), i.normal);
            attenuation = reflectance;

            if (randomDouble() > reflectance) {
                direction = refracted(r.getDirection(), i.normal, ratio);
                attenuation = 1 - reflectance;
            }

            scattered = Ray(i.point, direction, r.getLambda(), r.getLambdaIndex());

            return true;
        }

    private:
        std::vector<double> n, d, dRanges, noiseScales;
        Vector normal;

        double lamellarReflectance(bool frontFacing, const Vector & point, double cos0, double lambda) const {
            int jOffset = frontFacing ? 0 : n.size() - 1;
            int jSign = frontFacing ? 1 : -1;

            std::complex<double> sin1 = n[0] / n[1];
            sin1 *= sin1 * (std::complex<double>(1) - cos0 * cos0);
            std::complex<double> cos1 = sqrt(std::complex<double>(1) - sin1);
            std::complex<double> sin2 = n[1] / n[2];
            sin2 *= sin2 * sin1;
            std::complex<double> cos2 = sqrt(std::complex<double>(1) - sin2);

            std::vector<Matrix> matrices(2);
            matrices[0] = interfaceMatrixS(n[0], n[1], cos0, cos1);
            matrices[1] = interfaceMatrixP(n[0], n[1], cos0, cos1);

            Matrix multiplierS = propagationMatrix(std::complex<double>(2 * M_PI / lambda) * n[1] * d[0] * cos1);
            Matrix multiplierP = multiplierS;

            multiplierS *= interfaceMatrixS(n[1], n[2], cos1, cos2);
            multiplierP *= interfaceMatrixP(n[1], n[2], cos1, cos2);

            Matrix propagation = propagationMatrix(std::complex<double>(2 * M_PI / lambda) * n[2] * d[1] * cos2);

            matrices[0] *= multiplierS;
            matrices[1] *= multiplierP;

            double R_s = abs(matrices[0].get(0, 1) / matrices[0].get(0, 0));
            double R_p = abs(matrices[1].get(0, 1) / matrices[1].get(0, 0));

            R_s *= R_s;
            R_p *= R_p;

            return (R_s + R_p) * 0.5;
        }
};

#endif