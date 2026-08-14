#pragma once

#include <cctype>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>

#include "core/utils.h"
#include "math/random.h"

#ifndef __CUDA_ARCH__
    class Writer {
        public:
            static bool hasValidExtension(const std::string & output) { return hasExtension(output, ".ppm"); }

            static void write(const std::string & output, int width, int height, Float * const buffer) {
                if (hasExtension(output, ".ppm")) writePPM(output, width, height, buffer);
                else throw std::runtime_error(": invalid file extension");
            }

        private:
            static void writePPM(const std::string & output, int width, int height, Float * const buffer) {
                std::ofstream outputFile(output);

                if (!outputFile.is_open()) {
                    std::string error = std::strerror(errno);
                    error[0] = char(std::tolower(error[0]));
                    throw std::runtime_error(": failed to open file (" + error + ")");
                }

                Random state(0, 0);

                outputFile << "P3\n" << width << " " << height << "\n255\n";

                for (int i = 0; i < width * height; i++) {
                    outputFile << (int)quantize(toneMap(buffer[i * 3 + 0]), state) << " " << (int)quantize(toneMap(buffer[i * 3 + 1]), state) << " " << (int)quantize(toneMap(buffer[i * 3 + 2]), state) << "\n";
                }

                outputFile.close();
            }
    };
#endif