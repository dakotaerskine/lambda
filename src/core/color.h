#pragma once

#include <string>

#include "core/allocator.h"
#include "core/constants.h"
#include "core/loader.h"
#include "core/platform.h"
#include "core/utils.h"
#include "math/matrix.h"
#include "math/vector.h"

namespace lambda {
    class ColorSpace {
        public:
            static void initialize(Allocator & allocator) {
                if (!srgb) srgb = initialize(allocator, constants::CHROMATICITIES_SRGB, constants::CIE_D65, LAMBDA_DATA_DIRECTORY "/srgb.coeff");
                if (!rec2020) rec2020 = initialize(allocator, constants::CHROMATICITIES_REC2020, constants::CIE_D65, LAMBDA_DATA_DIRECTORY "/rec2020.coeff");
                if (!aces2065) aces2065 = initialize(allocator, constants::CHROMATICITIES_ACES2065, constants::CIE_D60, LAMBDA_DATA_DIRECTORY "/aces2065-1.coeff");
            }

            static const ColorSpace * get(const std::string & name) {
                if (name == "srgb") return sRGB();
                else if (name == "rec2020") return Rec2020();
                else if (name == "aces2065-1") return ACES2065();

                return nullptr;
            }

            static const ColorSpace * sRGB() { return srgb; }

            static const ColorSpace * Rec2020() { return rec2020; }

            static const ColorSpace * ACES2065() { return aces2065; }

            ColorSpace(const double _chromaticities[8], const double * _illum = nullptr, float * _upsamplingScale = nullptr, float (* _upsamplingLUT)[constants::UPSAMPLING_RESOLUTION][constants::UPSAMPLING_RESOLUTION][constants::UPSAMPLING_RESOLUTION][3] = nullptr) : illum(_illum), upsamplingScale(_upsamplingScale), upsamplingLUT(_upsamplingLUT) {
                for (int i = 0; i < 8; i++) chromaticities[i] = _chromaticities[i];

                double xr = chromaticities[0] / chromaticities[1];
                double yr = 1;
                double zr = (1 - chromaticities[0] - chromaticities[1]) / chromaticities[1];
                double xg = chromaticities[2] / chromaticities[3];
                double yg = 1;
                double zg = (1 - chromaticities[2] - chromaticities[3]) / chromaticities[3];
                double xb = chromaticities[4] / chromaticities[5];
                double yb = 1;
                double zb = (1 - chromaticities[4] - chromaticities[5]) / chromaticities[5];
                double xw = chromaticities[6] / chromaticities[7];
                double yw = 1;
                double zw = (1 - chromaticities[6] - chromaticities[7]) / chromaticities[7];

                double determinant = xr * (yg * zb - yb * zg) - xg * (yr * zb - yb * zr) + xb * (yr * zg - yg * zr);

                double sr = (xw * (yg * zb - yb * zg) - xg * (yw * zb - yb * zw) + xb * (yw * zg - yg * zw)) / determinant;
                double sg = (xr * (yw * zb - yb * zw) - xw * (yr * zb - yb * zr) + xb * (yr * zw - yw * zr)) / determinant;
                double sb = (xr * (yg * zw - yw * zg) - xg * (yr * zw - yw * zr) + xw * (yr * zg - yg * zr)) / determinant;

                toXYZMatrix = Matrix<double, 3>(sr * xr, sg * xg, sb * xb, sr * yr, sg * yg, sb * yb, sr * zr, sg * zg, sb * zb);
                toRGBMatrix = inverse(toXYZMatrix);
            }

            double chromaticity(int i) const { return chromaticities[i]; }

            bool contains(const ColorSpace & space) const {
                for (int i = 0; i < 4; i++) {
                    double px = space.chromaticities[i * 2 + 0];
                    double py = space.chromaticities[i * 2 + 1];

                    double d1 = (px - chromaticities[2]) * (chromaticities[1] - chromaticities[3]) - (chromaticities[0] - chromaticities[2]) * (py - chromaticities[3]);
                    double d2 = (px - chromaticities[4]) * (chromaticities[3] - chromaticities[5]) - (chromaticities[2] - chromaticities[4]) * (py - chromaticities[5]);
                    double d3 = (px - chromaticities[0]) * (chromaticities[5] - chromaticities[1]) - (chromaticities[4] - chromaticities[0]) * (py - chromaticities[1]);

                    if ((d1 < -constants::EPSILON_RELATIVE || d2 < -constants::EPSILON_RELATIVE || d3 < -constants::EPSILON_RELATIVE) && (d1 > constants::EPSILON_RELATIVE || d2 > constants::EPSILON_RELATIVE || d3 > constants::EPSILON_RELATIVE)) return false;
                }

                return true;
            }

            Vector<double, 3> toXYZ(const Vector<double, 3> & color) const { return toXYZMatrix * color; }

            Vector<double, 3> toRGB(const Vector<double, 3> & color) const { return toRGBMatrix * color; }

            Vector<double, 3> toLab(const Vector<double, 3> & color, const Vector<double, 3> & whitepoint) const {
                Vector<double, 3> xyz = toXYZ(color);

                return Vector<double, 3>(116 * transformLab(xyz[1] / whitepoint[1]) - 16, 500 * (transformLab(xyz[0] / whitepoint[0]) - transformLab(xyz[1] / whitepoint[1])), 200 * (transformLab(xyz[1] / whitepoint[1]) - transformLab(xyz[2] / whitepoint[2])));
            }

            double illuminant(int lambda) const { return illum && lambda >= constants::CIE_LAMBDA_MIN && lambda <= constants::CIE_LAMBDA_MAX ? illum[lambda - constants::CIE_LAMBDA_MIN] : 0; }

            bool hasUpsampling() const { return upsamplingScale && upsamplingLUT; }

            static Matrix<double, 3> bradfordAdaptation(const ColorSpace & source, const ColorSpace & target) {
                Matrix<double, 3> bradford(constants::BRADFORD);
                Matrix<double, 3> inverseBradford = inverse(bradford);

                double wz1 = 1 - source.chromaticities[6] - source.chromaticities[7];
                double wz2 = 1 - target.chromaticities[6] - target.chromaticities[7];

                Vector<double, 3> sourceXYZ(source.chromaticities[6] / source.chromaticities[7], 1, wz1 / source.chromaticities[7]);
                Vector<double, 3> targetXYZ(target.chromaticities[6] / target.chromaticities[7], 1, wz2 / target.chromaticities[7]);

                Vector<double, 3> sourceLMS = bradford * sourceXYZ;
                Vector<double, 3> targetLMS = bradford * targetXYZ;

                Matrix<double, 3> scaleMatrix(targetLMS[0] / sourceLMS[0], 0, 0, 0, targetLMS[1] / sourceLMS[1], 0, 0, 0, targetLMS[2] / sourceLMS[2]);

                return inverseBradford * scaleMatrix * bradford;
            }

            LAMBDA_DEVICE Vector<Float, 3> upsample(const Vector<Float, 3> & color) const {
                if (!upsamplingScale || !upsamplingLUT) return Vector<Float, 3>();

                Float r = utils::clamp(color[0], Float(0), Float(1));
                Float g = utils::clamp(color[1], Float(0), Float(1));
                Float b = utils::clamp(color[2], Float(0), Float(1));

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

                if (x <= constants::BLACK_LEVEL) return Vector<Float, 3>(upsamplingLUT[caseIndex][0][0][0]);

                Float gridScale = (constants::UPSAMPLING_RESOLUTION - 1) / x;

                y *= gridScale;
                z *= gridScale;

                int left = 0;
                int lastInterval = constants::UPSAMPLING_RESOLUTION - 2;
                int size = lastInterval;

                while (size > 0) {
                    int half = size >> 1;
                    int middle = left + half + 1;

                    if (upsamplingScale[middle] <= x) {
                        left = middle;
                        size -= half + 1;
                    }
                    else size = half;
                }

                int x0 = left < lastInterval ? left : lastInterval;
                int x1 = x0 + 1;
                int y0 = (int)utils::clamp(std::floor(y), Float(0), Float(constants::UPSAMPLING_RESOLUTION - 2));
                int y1 = y0 + 1;
                int z0 = (int)utils::clamp(std::floor(z), Float(0), Float(constants::UPSAMPLING_RESOLUTION - 2));
                int z1 = z0 + 1;

                Float tx = (x - upsamplingScale[x0]) / (upsamplingScale[x1] - upsamplingScale[x0]);
                Float ty = y - Float(y0);
                Float tz = z - Float(z0);

                Vector<Float, 3> coefficients;

                for (int j = 0; j < 3; j++) {
                    Float c000 = upsamplingLUT[caseIndex][x0][y0][z0][j];
                    Float c100 = upsamplingLUT[caseIndex][x1][y0][z0][j];
                    Float c010 = upsamplingLUT[caseIndex][x0][y1][z0][j];
                    Float c110 = upsamplingLUT[caseIndex][x1][y1][z0][j];
                    Float c001 = upsamplingLUT[caseIndex][x0][y0][z1][j];
                    Float c101 = upsamplingLUT[caseIndex][x1][y0][z1][j];
                    Float c011 = upsamplingLUT[caseIndex][x0][y1][z1][j];
                    Float c111 = upsamplingLUT[caseIndex][x1][y1][z1][j];

                    coefficients[j] = utils::interpolate(utils::interpolate(utils::interpolate(c000, c100, tx), utils::interpolate(c010, c110, tx), ty), utils::interpolate(utils::interpolate(c001, c101, tx), utils::interpolate(c011, c111, tx), ty), tz);
                }

                return coefficients;
            }

        private:
            static inline ColorSpace * srgb;
            static inline ColorSpace * rec2020;
            static inline ColorSpace * aces2065;

            double chromaticities[8];
            Matrix<double, 3> toXYZMatrix, toRGBMatrix;
            const double * illum;
            float * upsamplingScale;
            float (* upsamplingLUT)[constants::UPSAMPLING_RESOLUTION][constants::UPSAMPLING_RESOLUTION][constants::UPSAMPLING_RESOLUTION][3];

            static ColorSpace * initialize([[maybe_unused]] Allocator & allocator, const double chromaticities[8], const double * illuminant, [[maybe_unused]] const std::string & file) {
                float * upsamplingScale = nullptr;
                float (* upsamplingLUT)[constants::UPSAMPLING_RESOLUTION][constants::UPSAMPLING_RESOLUTION][constants::UPSAMPLING_RESOLUTION][3] = nullptr;

                #if LAMBDA_UPSAMPLING
                    upsamplingScale = allocator.allocate<float>(constants::UPSAMPLING_RESOLUTION);
                    upsamplingLUT = allocator.allocate<float[constants::UPSAMPLING_RESOLUTION][constants::UPSAMPLING_RESOLUTION][constants::UPSAMPLING_RESOLUTION][3]>(3);

                    if (!Loader::loadUpsampling(file, upsamplingScale, upsamplingLUT)) {
                        upsamplingScale = nullptr;
                        upsamplingLUT = nullptr;
                    }
                #endif

                return allocator.make(ColorSpace(chromaticities, illuminant, upsamplingScale, upsamplingLUT));
            }

            static double transformLab(double t) {
                double delta = 6.0 / 29;

                if (t > delta * delta * delta) return std::cbrt(t);
                else return t / (delta * delta * 3) + 4.0 / 29;
            }
    };
}