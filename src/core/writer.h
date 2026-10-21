#pragma once

#include <cctype>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include <stb/stb_image_write.h>
#include <tinyexr/tinyexr.h>

#include "core/error.h"
#include "core/utils.h"
#include "render/camera.h"
#include "render/film.h"
#include "render/integrator.h"
#include "math/matrix.h"
#include "math/random.h"
#include "math/vector.h"

namespace lambda {
    class Writer {
        public:
            static bool hasValidExtension(const std::string & output) { return utils::hasExtension(output, ".exr") || utils::hasExtension(output, ".pfm") || utils::hasExtension(output, ".png"); }

            static void writeImage(const std::string & output, const Integrator * integrator, Float duration) {
                if (utils::hasExtension(output, ".exr")) writeEXR(output, integrator, duration);
                else if (utils::hasExtension(output, ".pfm")) writePFM(output, integrator->getFilm());
                else if (utils::hasExtension(output, ".png")) writePNG(output, integrator->getFilm());
                else error::fileError(output, "invalid file extension");
            }

            static void writeUpsamplingTables(const std::string & output, float * scale, const float * lut) {
                std::ofstream outputFile(output, std::ios::binary);

                if (!outputFile.is_open()) return;

                outputFile.write("SPEC", 4);

                uint32_t resolution = constants::UPSAMPLING_RESOLUTION;

                outputFile.write(reinterpret_cast<const char *>(&resolution), sizeof(uint32_t));

                outputFile.write(reinterpret_cast<const char *>(scale), sizeof(float) * resolution);
                outputFile.write(reinterpret_cast<const char *>(lut), sizeof(float) * 3 * resolution * resolution * resolution * 3);
            }

        private:
            static void writePNG(const std::string & output, const Film * film) {
                std::vector<unsigned char> pixels(film->getWidth() * film->getHeight() * 3);

                Matrix<double, 3> toRGB = utils::toRGBMatrix(ColorSpace::SRGB);

                Random state(0, 0);

                for (int j = 0; j < film->getHeight(); j++)
                    for (int i = 0; i < film->getWidth(); i++) {
                        Vector<Float, 3> color = toRGB * film->getPixel(i, j);

                        pixels[(j * film->getWidth() + i) * 3 + 0] = utils::quantize(utils::toneMap(color[0]), state);
                        pixels[(j * film->getWidth() + i) * 3 + 1] = utils::quantize(utils::toneMap(color[1]), state);
                        pixels[(j * film->getWidth() + i) * 3 + 2] = utils::quantize(utils::toneMap(color[2]), state);
                    }

                if (!stbi_write_png(output.c_str(), film->getWidth(), film->getHeight(), 3, pixels.data(), film->getWidth() * 3)) {
                    const char * failureReason = stbi_failure_reason();

                    std::string error = failureReason ? " (" + std::string(failureReason) + ")" : "";

                    error::fileError(output, "failed to write file" + error);
                }
            }

            static void writePFM(const std::string & output, const Film * film) {
                std::ofstream outputFile(output, std::ios::binary);

                if (!outputFile.is_open()) error::failedToOpenFileError(output);

                Matrix<double, 3> toRGB = utils::toRGBMatrix(film->getSpace());

                outputFile << "PF\n" << film->getWidth() << " " << film->getHeight() << "\n-1.0\n";

                for (int j = film->getHeight() - 1; j >= 0; j--)
                    for (int i = 0; i < film->getWidth(); i++) {
                        Vector<float, 3> color = toRGB * film->getPixel(i, j);

                        outputFile.write(reinterpret_cast<const char *>(&color[0]), sizeof(float));
                        outputFile.write(reinterpret_cast<const char *>(&color[1]), sizeof(float));
                        outputFile.write(reinterpret_cast<const char *>(&color[2]), sizeof(float));
                    }

                outputFile.close();
            }

            static void writeEXR(const std::string & output, const Integrator * integrator, Float duration) {
                const Film * film = integrator->getFilm();

                EXRHeader header;
                InitEXRHeader(&header);

                EXRImage image;
                InitEXRImage(&image);

                image.num_channels = 3;
                image.width = film->getWidth();
                image.height = film->getHeight();

                std::vector<float> channelB(image.width * image.height);
                std::vector<float> channelG(image.width * image.height);
                std::vector<float> channelR(image.width * image.height);

                Matrix<double, 3> toRGB = utils::toRGBMatrix(film->getSpace());

                for (int j = 0; j < image.height; j++)
                    for (int i = 0; i < image.width; i++) {
                        Vector<float, 3> color = toRGB * film->getPixel(i, j);

                        channelR[j * image.width + i] = std::fmin(color[0], float(65504));
                        channelG[j * image.width + i] = std::fmin(color[1], float(65504));
                        channelB[j * image.width + i] = std::fmin(color[2], float(65504));
                    }

                float * pointers[3] = {channelB.data(), channelG.data(), channelR.data()};

                image.images = reinterpret_cast<unsigned char **>(pointers);

                std::vector<EXRChannelInfo> channels(3);

                header.num_channels = 3;
                header.channels = channels.data();
                strncpy(header.channels[0].name, "B", 255);
                strncpy(header.channels[1].name, "G", 255);
                strncpy(header.channels[2].name, "R", 255);

                std::vector<int> pixelTypes(3, TINYEXR_PIXELTYPE_FLOAT);
                std::vector<int> requestedPixelTypes(3, TINYEXR_PIXELTYPE_HALF);

                header.pixel_types = pixelTypes.data();
                header.requested_pixel_types = requestedPixelTypes.data();

                header.compression_type = TINYEXR_COMPRESSIONTYPE_PIZ;

                std::vector<EXRAttribute> customAttributes(8);

                header.num_custom_attributes = 8;
                header.custom_attributes = customAttributes.data();

                std::string software = "Lambda";

                float chromaticities[8];

                for (int i = 0; i < 8; i++) chromaticities[i] = float(utils::chromaticity(film->getSpace(), i));

                float worldToCamera[16];

                Matrix<Float, 4> worldToCameraMatrix = integrator->getCamera()->getWorldToCamera();

                for (int i = 0; i < 16; i++) worldToCamera[i] = float(worldToCameraMatrix.get(i % 4, i / 4));

                float worldToNDC[16];

                Matrix<Float, 4> worldToNDCMatrix = integrator->getCamera()->getWorldToNDC();

                for (int i = 0; i < 16; i++) worldToNDC[i] = float(worldToNDCMatrix.get(i % 4, i / 4));

                int samplesPerPixel = integrator->getSamples();
                float renderTimeSeconds = float(duration);
                float lambdaMin = float(film->getLambdaMin());
                float lambdaMax = float(film->getLambdaMax());

                std::vector<unsigned char> bytesVector(std::ssize(software) + sizeof(chromaticities) + sizeof(worldToCamera) + sizeof(worldToNDC) + sizeof(samplesPerPixel) + sizeof(renderTimeSeconds) + sizeof(lambdaMin) + sizeof(lambdaMax));

                unsigned char * bytes = bytesVector.data();

                writeEXRAttribute(header, 0, "software", "string", software.c_str(), int(std::ssize(software)), bytes);
                writeEXRAttribute(header, 1, "chromaticities", "chromaticities", chromaticities, 8, bytes);
                writeEXRAttribute(header, 2, "worldToCamera", "m44f", worldToCamera, 16, bytes);
                writeEXRAttribute(header, 3, "worldToNDC", "m44f", worldToNDC, 16, bytes);
                writeEXRAttribute(header, 4, "samplesPerPixel", "int", &samplesPerPixel, 1, bytes);
                writeEXRAttribute(header, 5, "renderTimeSeconds", "float", &renderTimeSeconds, 1, bytes);
                writeEXRAttribute(header, 6, "lambdaMin", "float", &lambdaMin, 1, bytes);
                writeEXRAttribute(header, 7, "lambdaMax", "float", &lambdaMax, 1, bytes);

                const char * error = nullptr;

                if (SaveEXRImageToFile(&image, &header, output.c_str(), &error) != TINYEXR_SUCCESS) {
                    std::string errorMessage = error ? " (" + std::string(error) + ")" : "";

                    FreeEXRErrorMessage(error);

                    error::fileError(output, "failed to open file" + errorMessage);
                }
            }

            template <typename T>
            static void writeEXRAttribute(EXRHeader & header, int index, const std::string & name, const std::string & type, const T * value, int size, unsigned char * & bytes) {
                int totalSize = sizeof(T) * size;

                strncpy(header.custom_attributes[index].name, name.c_str(), 255);
                strncpy(header.custom_attributes[index].type, type.c_str(), 255);
                header.custom_attributes[index].size = totalSize;
                header.custom_attributes[index].value = bytes;
                memcpy(header.custom_attributes[index].value, value, totalSize);

                bytes += totalSize;
            }
    };
}