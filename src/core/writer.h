#pragma once

#include <cctype>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

#define TINYEXR_IMPLEMENTATION
#include <tinyexr/tinyexr.h>

#include "core/error.h"
#include "core/utils.h"
#include "render/camera.h"
#include "render/renderer.h"
#include "math/matrix.h"
#include "math/random.h"
#include "math/vector.h"

class Writer {
    public:
        static bool hasValidExtension(const std::string & output) { return hasExtension(output, ".exr") || hasExtension(output, ".pfm") || hasExtension(output, ".png"); }

        static void writeImage(const std::string & output, const Renderer & renderer, Float duration) {
            if (hasExtension(output, ".exr")) writeEXR(output, renderer, duration);
            else if (hasExtension(output, ".pfm")) writePFM(output, renderer);
            else if (hasExtension(output, ".png")) writePNG(output, renderer);
            else fileError(output, "invalid file extension");
        }

        static void writeUpsamplingTables(const std::string & output, const std::vector<float> & scale, const std::vector<float> & lut) {
            std::ofstream outputFile(output, std::ios::binary);

            if (!outputFile.is_open()) failedToOpenFileError(output);

            outputFile.write("SPEC", 4);

            uint32_t resolution = UPSAMPLING_RESOLUTION;

            outputFile.write(reinterpret_cast<const char *>(&resolution), sizeof(uint32_t));

            outputFile.write(reinterpret_cast<const char *>(scale.data()), sizeof(float) * resolution);
            outputFile.write(reinterpret_cast<const char *>(lut.data()), sizeof(float) * 3 * resolution * resolution * resolution * 3);
        }

    private:
        static void writePNG(const std::string & output, const Renderer & renderer) {
            std::vector<unsigned char> pixels(renderer.getTotalPixels() * 3);

            Random state(0, 0);

            for (int i = 0; i < renderer.getTotalPixels(); i++) {
                Vector<Float> color = transform(toRGBMatrix(ColorSpace::SRGB), Vector<Float>(renderer.getChannel(i, 0), renderer.getChannel(i, 1), renderer.getChannel(i, 2)));

                pixels[i * 3 + 0] = quantize(toneMap(color[0]), state);
                pixels[i * 3 + 1] = quantize(toneMap(color[1]), state);
                pixels[i * 3 + 2] = quantize(toneMap(color[2]), state);
            }

            if (!stbi_write_png(output.c_str(), renderer.getWidth(), renderer.getHeight(), 3, pixels.data(), renderer.getWidth() * 3)) {
                std::string error = stbi_failure_reason();
                fileError(output, "failed to write file (" + error + ")");
            }
        }

        static void writePFM(const std::string & output, const Renderer & renderer) {
            std::ofstream outputFile(output, std::ios::binary);

            if (!outputFile.is_open()) failedToOpenFileError(output);

            Random state(0, 0);

            outputFile << "PF\n" << renderer.getWidth() << " " << renderer.getHeight() << "\n-1.0\n";

            for (int j = renderer.getHeight() - 1; j >= 0; j--)
                for (int i = 0; i < renderer.getWidth(); i++) {
                    int index = j * renderer.getWidth() + i;

                    Vector<Float> color = transform(toRGBMatrix(renderer.getSpace()), Vector<Float>(renderer.getChannel(index, 0), renderer.getChannel(index, 1), renderer.getChannel(index, 2)));

                    outputFile.write(reinterpret_cast<const char *>(&color[0]), sizeof(float));
                    outputFile.write(reinterpret_cast<const char *>(&color[1]), sizeof(float));
                    outputFile.write(reinterpret_cast<const char *>(&color[2]), sizeof(float));
                }

            outputFile.close();
        }

        static void writeEXR(const std::string & output, const Renderer & renderer, Float duration) {
            EXRHeader header;
            InitEXRHeader(&header);

            EXRImage image;
            InitEXRImage(&image);

            image.num_channels = 3;
            image.width = renderer.getWidth();
            image.height = renderer.getHeight();

            std::vector<float> channelB(renderer.getTotalPixels());
            std::vector<float> channelG(renderer.getTotalPixels());
            std::vector<float> channelR(renderer.getTotalPixels());

            for (int i = 0; i < renderer.getTotalPixels(); i++) {
                Vector<Float> color = transform(toRGBMatrix(renderer.getSpace()), Vector<Float>(renderer.getChannel(i, 0), renderer.getChannel(i, 1), renderer.getChannel(i, 2)));

                channelR[i] = float(color[0]);
                channelG[i] = float(color[1]);
                channelB[i] = float(color[2]);
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
            std::vector<unsigned char> softwareValue(int(software.size()) + 1);

            strncpy(header.custom_attributes[0].name, "software", 255);
            strncpy(header.custom_attributes[0].type, "string", 255);
            header.custom_attributes[0].size = int(software.size()) + 1;
            header.custom_attributes[0].value = softwareValue.data();
            memcpy(header.custom_attributes[0].value, software.c_str(), header.custom_attributes[0].size);

            float chromaticities[8];

            for (int i = 0; i < 8; i++)
                chromaticities[i] = float(chromaticity(renderer.getSpace(), i));

            std::vector<unsigned char> chromaticitiesValue(sizeof(float) * 8);

            strncpy(header.custom_attributes[1].name, "chromaticities", 255);
            strncpy(header.custom_attributes[1].type, "chromaticities", 255);
            header.custom_attributes[1].size = sizeof(float) * 8;
            header.custom_attributes[1].value = chromaticitiesValue.data();
            memcpy(header.custom_attributes[1].value, chromaticities, header.custom_attributes[1].size);

            float worldToCamera[16];

            Matrix4<Float> worldToCameraMatrix = renderer.getCamera().getWorldToCamera();

            for (int i = 0; i < 16; i++)
                worldToCamera[i] = float(worldToCameraMatrix.get(i / 4, i % 4));

            std::vector<unsigned char> worldToCameraValue(sizeof(float) * 16);

            strncpy(header.custom_attributes[2].name, "worldToCamera", 255);
            strncpy(header.custom_attributes[2].type, "m44f", 255);
            header.custom_attributes[2].size = sizeof(float) * 16;
            header.custom_attributes[2].value = worldToCameraValue.data();
            memcpy(header.custom_attributes[2].value, worldToCamera, header.custom_attributes[2].size);

            float worldToNDC[16];

            Matrix4<Float> worldToNDCMatrix = renderer.getCamera().getWorldToNDC();

            for (int i = 0; i < 16; i++)
                worldToNDC[i] = float(worldToNDCMatrix.get(i / 4, i % 4));

            std::vector<unsigned char> worldToNDCValue(sizeof(float) * 16);

            strncpy(header.custom_attributes[3].name, "worldToNDC", 255);
            strncpy(header.custom_attributes[3].type, "m44f", 255);
            header.custom_attributes[3].size = sizeof(float) * 16;
            header.custom_attributes[3].value = worldToNDCValue.data();
            memcpy(header.custom_attributes[3].value, worldToNDC, header.custom_attributes[3].size);

            int samplesPerPixel = renderer.getSamples();

            std::vector<unsigned char> samplesPerPixelValue(sizeof(int));

            strncpy(header.custom_attributes[4].name, "samplesPerPixel", 255);
            strncpy(header.custom_attributes[4].type, "int", 255);
            header.custom_attributes[4].size = sizeof(int);
            header.custom_attributes[4].value = samplesPerPixelValue.data();
            memcpy(header.custom_attributes[4].value, &samplesPerPixel, header.custom_attributes[4].size);

            float renderTimeSeconds = float(duration);

            std::vector<unsigned char> renderTimeSecondsValue(sizeof(float));

            strncpy(header.custom_attributes[5].name, "renderTimeSeconds", 255);
            strncpy(header.custom_attributes[5].type, "float", 255);
            header.custom_attributes[5].size = sizeof(float);
            header.custom_attributes[5].value = renderTimeSecondsValue.data();
            memcpy(header.custom_attributes[5].value, &renderTimeSeconds, header.custom_attributes[5].size);

            float lambdaMin = float(renderer.getLambdaMin());

            std::vector<unsigned char> lambdaMinValue(sizeof(float));

            strncpy(header.custom_attributes[6].name, "lambdaMin", 255);
            strncpy(header.custom_attributes[6].type, "float", 255);
            header.custom_attributes[6].size = sizeof(float);
            header.custom_attributes[6].value = lambdaMinValue.data();
            memcpy(header.custom_attributes[6].value, &lambdaMin, header.custom_attributes[6].size);

            float lambdaMax = float(renderer.getLambdaMax());

            std::vector<unsigned char> lambdaMaxValue(sizeof(float));

            strncpy(header.custom_attributes[7].name, "lambdaMax", 255);
            strncpy(header.custom_attributes[7].type, "float", 255);
            header.custom_attributes[7].size = sizeof(float);
            header.custom_attributes[7].value = lambdaMaxValue.data();
            memcpy(header.custom_attributes[7].value, &lambdaMax, header.custom_attributes[7].size);

            const char * error = nullptr;

            if (SaveEXRImageToFile(&image, &header, output.c_str(), &error) != TINYEXR_SUCCESS) {
                std::string errorMessage = error;
                FreeEXRErrorMessage(error);
                fileError(output, "failed to open file (" + errorMessage + ")");
            }
        }
};