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
#include "render/renderer.h"
#include "math/matrix.h"
#include "math/random.h"
#include "math/vector.h"

namespace lambda {
    class Writer {
        public:
            static bool hasValidExtension(const std::string & output) { return utils::hasExtension(output, ".exr") || utils::hasExtension(output, ".pfm") || utils::hasExtension(output, ".png"); }

            static void writeImage(const std::string & output, const Renderer * renderer, Float duration) {
                if (utils::hasExtension(output, ".exr")) writeEXR(output, renderer, duration);
                else if (utils::hasExtension(output, ".pfm")) writePFM(output, renderer);
                else if (utils::hasExtension(output, ".png")) writePNG(output, renderer);
                else error::fileError(output, "invalid file extension");
            }

            static void writeUpsamplingTables(const std::string & output, float * scale, const float * lut) {
                std::ofstream outputFile(output, std::ios::binary);

                if (!outputFile.is_open()) error::failedToOpenFileError(output);

                outputFile.write("SPEC", 4);

                uint32_t resolution = constants::UPSAMPLING_RESOLUTION;

                outputFile.write(reinterpret_cast<const char *>(&resolution), sizeof(uint32_t));

                outputFile.write(reinterpret_cast<const char *>(scale), sizeof(float) * resolution);
                outputFile.write(reinterpret_cast<const char *>(lut), sizeof(float) * 3 * resolution * resolution * resolution * 3);
            }

        private:
            static void writePNG(const std::string & output, const Renderer * renderer) {
                std::vector<unsigned char> pixels(renderer->getTotalPixels() * 3);

                Random state(0, 0);

                for (int i = 0; i < renderer->getTotalPixels(); i++) {
                    Vector<Float, 3> color = utils::toRGBMatrix(ColorSpace::SRGB) * Vector<Float, 3>(renderer->getChannel(i, 0), renderer->getChannel(i, 1), renderer->getChannel(i, 2));

                    pixels[i * 3 + 0] = utils::quantize(utils::toneMap(color[0]), state);
                    pixels[i * 3 + 1] = utils::quantize(utils::toneMap(color[1]), state);
                    pixels[i * 3 + 2] = utils::quantize(utils::toneMap(color[2]), state);
                }

                if (!stbi_write_png(output.c_str(), renderer->getWidth(), renderer->getHeight(), 3, pixels.data(), renderer->getWidth() * 3)) {
                    const char * failureReason = stbi_failure_reason();

                    std::string error = failureReason ? " (" + std::string(failureReason) + ")" : "";

                    error::fileError(output, "failed to write file" + error);
                }
            }

            static void writePFM(const std::string & output, const Renderer * renderer) {
                std::ofstream outputFile(output, std::ios::binary);

                if (!outputFile.is_open()) error::failedToOpenFileError(output);

                Random state(0, 0);

                outputFile << "PF\n" << renderer->getWidth() << " " << renderer->getHeight() << "\n-1.0\n";

                for (int j = renderer->getHeight() - 1; j >= 0; j--)
                    for (int i = 0; i < renderer->getWidth(); i++) {
                        int index = j * renderer->getWidth() + i;

                        Vector<float, 3> color = utils::toRGBMatrix(renderer->getSpace()) * Vector<float, 3>(renderer->getChannel(index, 0), renderer->getChannel(index, 1), renderer->getChannel(index, 2));

                        outputFile.write(reinterpret_cast<const char *>(&color[0]), sizeof(float));
                        outputFile.write(reinterpret_cast<const char *>(&color[1]), sizeof(float));
                        outputFile.write(reinterpret_cast<const char *>(&color[2]), sizeof(float));
                    }

                outputFile.close();
            }

            static void writeEXR(const std::string & output, const Renderer * renderer, Float duration) {
                EXRHeader header;
                InitEXRHeader(&header);

                EXRImage image;
                InitEXRImage(&image);

                image.num_channels = 3;
                image.width = renderer->getWidth();
                image.height = renderer->getHeight();

                std::vector<float> channelB(renderer->getTotalPixels());
                std::vector<float> channelG(renderer->getTotalPixels());
                std::vector<float> channelR(renderer->getTotalPixels());

                for (int i = 0; i < renderer->getTotalPixels(); i++) {
                    Vector<float, 3> color = utils::toRGBMatrix(renderer->getSpace()) * Vector<float, 3>(renderer->getChannel(i, 0), renderer->getChannel(i, 1), renderer->getChannel(i, 2));

                    channelR[i] = color[0];
                    channelG[i] = color[1];
                    channelB[i] = color[2];
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

                for (int i = 0; i < 8; i++) chromaticities[i] = float(utils::chromaticity(renderer->getSpace(), i));

                float worldToCamera[16];

                Matrix<Float, 4> worldToCameraMatrix = renderer->getCamera()->getWorldToCamera();

                for (int i = 0; i < 16; i++) worldToCamera[i] = float(worldToCameraMatrix.get(i / 4, i % 4));

                float worldToNDC[16];

                Matrix<Float, 4> worldToNDCMatrix = renderer->getCamera()->getWorldToNDC();

                for (int i = 0; i < 16; i++) worldToNDC[i] = float(worldToNDCMatrix.get(i / 4, i % 4));

                int samplesPerPixel = renderer->getSamples();
                float renderTimeSeconds = float(duration);
                float lambdaMin = float(renderer->getLambdaMin());
                float lambdaMax = float(renderer->getLambdaMax());

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