#pragma once

#include "core/buffer.h"
#include "core/constants.h"
#include "core/parser.h"
#include "core/platform.h"
#include "core/utils.h"
#include "core/writer.h"

class Tables {
    public:
        Tables() : d_SRGB_TO_XYZ(toXYZMatrix(CHROMATICITIES_SRGB).data(), 3 * 3), d_XYZ_TO_SRGB(inverse(toXYZMatrix(CHROMATICITIES_SRGB)).data(), 3 * 3), d_REC2020_TO_XYZ(toXYZMatrix(CHROMATICITIES_REC2020).data(), 3 * 3), d_XYZ_TO_REC2020(inverse(toXYZMatrix(CHROMATICITIES_REC2020)).data(), 3 * 3), d_ACES2065_TO_XYZ(toXYZMatrix(CHROMATICITIES_ACES2065).data(), 3 * 3), d_XYZ_TO_ACES2065(inverse(toXYZMatrix(CHROMATICITIES_ACES2065)).data(), 3 * 3), d_BRADFORD_INVERSE(inverse(Matrix3<double>(BRADFORD)).data(), 3 * 3), d_PERMUTATION(&h_PERMUTATION[0], PERMUTATION_SIZE), d_CIE_XYZ_1931(&h_CIE_XYZ_1931[0][0], CIE_LAMBDA_BINS * 3) {
            SRGB_TO_XYZ = d_SRGB_TO_XYZ.data();
            XYZ_TO_SRGB = d_XYZ_TO_SRGB.data();
            REC2020_TO_XYZ = d_REC2020_TO_XYZ.data();
            XYZ_TO_REC2020 = d_XYZ_TO_REC2020.data();
            ACES2065_TO_XYZ = d_ACES2065_TO_XYZ.data();
            XYZ_TO_ACES2065 = d_XYZ_TO_ACES2065.data();
            BRADFORD_INVERSE = d_BRADFORD_INVERSE.data();
            PERMUTATION = d_PERMUTATION.data();
            CIE_XYZ_1931 = d_CIE_XYZ_1931.data();

            loadUpsamplingTables("data/srgb.coeff", ColorSpace::SRGB, d_UPSAMPLING_SCALE_SRGB, d_UPSAMPLING_LUT_SRGB);
            loadUpsamplingTables("data/rec2020.coeff", ColorSpace::REC2020, d_UPSAMPLING_SCALE_REC2020, d_UPSAMPLING_LUT_REC2020);
            loadUpsamplingTables("data/aces2065-1.coeff", ColorSpace::ACES2065, d_UPSAMPLING_SCALE_ACES2065, d_UPSAMPLING_LUT_ACES2065);

            UPSAMPLING_SCALE_SRGB = d_UPSAMPLING_SCALE_SRGB.data();
            UPSAMPLING_LUT_SRGB = d_UPSAMPLING_LUT_SRGB.data();
            UPSAMPLING_SCALE_REC2020 = d_UPSAMPLING_SCALE_REC2020.data();
            UPSAMPLING_LUT_REC2020 = d_UPSAMPLING_LUT_REC2020.data();
            UPSAMPLING_SCALE_ACES2065 = d_UPSAMPLING_SCALE_ACES2065.data();
            UPSAMPLING_LUT_ACES2065 = d_UPSAMPLING_LUT_ACES2065.data();
        }

    private:
        Buffer<double> d_SRGB_TO_XYZ, d_XYZ_TO_SRGB, d_REC2020_TO_XYZ, d_XYZ_TO_REC2020, d_ACES2065_TO_XYZ, d_XYZ_TO_ACES2065, d_BRADFORD_INVERSE;
        Buffer<int> d_PERMUTATION;
        Buffer<double> d_CIE_XYZ_1931;
        Buffer<float> d_UPSAMPLING_SCALE_SRGB, d_UPSAMPLING_LUT_SRGB, d_UPSAMPLING_SCALE_REC2020, d_UPSAMPLING_LUT_REC2020, d_UPSAMPLING_SCALE_ACES2065, d_UPSAMPLING_LUT_ACES2065;

        void loadUpsamplingTables(const std::string & file, ColorSpace space, Buffer<float> & d_scale, Buffer<float> & d_lut) {
            std::vector<float> h_scale(UPSAMPLING_RESOLUTION);
            std::vector<float> h_lut(3 * UPSAMPLING_RESOLUTION * UPSAMPLING_RESOLUTION * UPSAMPLING_RESOLUTION * 3);

            if (!Parser::parseUpsamplingTables(file, h_scale, h_lut)) {
                solveGrid(space, h_scale.data(), h_lut.data());

                Writer::writeUpsamplingTables(file, h_scale, h_lut);
            }

            d_scale = Buffer<float>(h_scale);
            d_lut = Buffer<float>(h_lut);
        }
};