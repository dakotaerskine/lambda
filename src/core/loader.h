#pragma once

#include <cstring>
#include <cstdint>
#include <fstream>
#include <string>

#include "core/constants.h"

namespace lambda {
    class Loader {
        public:
            Loader() = delete;

            static bool loadUpsampling(const std::string & input, float * upsamplingScale, float (* upsamplingLUT)[constants::UPSAMPLING_RESOLUTION][constants::UPSAMPLING_RESOLUTION][constants::UPSAMPLING_RESOLUTION][3]) {
                std::ifstream inputFile(input, std::ios::binary);

                if (!inputFile) return false;

                char header[4];

                inputFile.read(header, 4);

                if (!inputFile || std::memcmp(header, "SPEC", 4) != 0) return false;

                uint32_t resolution;

                inputFile.read(reinterpret_cast<char *>(&resolution), sizeof(uint32_t));

                if (!inputFile || resolution != constants::UPSAMPLING_RESOLUTION) return false;

                inputFile.read(reinterpret_cast<char *>(upsamplingScale), sizeof(float) * resolution);
                inputFile.read(reinterpret_cast<char *>(upsamplingLUT), sizeof(float) * 3 * resolution * resolution * resolution * 3);

                if (!inputFile) return false;

                return true;
            }

            static void saveUpsampling(const std::string & output, const float * upsamplingScale, const float (* upsamplingLUT)[constants::UPSAMPLING_RESOLUTION][constants::UPSAMPLING_RESOLUTION][constants::UPSAMPLING_RESOLUTION][3]) {
                std::ofstream outputFile(output, std::ios::binary);

                if (!outputFile.is_open()) return;

                outputFile.write("SPEC", 4);

                uint32_t resolution = constants::UPSAMPLING_RESOLUTION;

                outputFile.write(reinterpret_cast<const char *>(&resolution), sizeof(uint32_t));

                outputFile.write(reinterpret_cast<const char *>(upsamplingScale), sizeof(float) * resolution);
                outputFile.write(reinterpret_cast<const char *>(upsamplingLUT), sizeof(float) * 3 * resolution * resolution * resolution * 3);
            }
    };
}