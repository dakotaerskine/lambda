#pragma once

#include <cmath>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#define TINYEXR_IMPLEMENTATION
#include <tinyexr/tinyexr.h>

#include "core/constants.h"
#include "core/error.h"
#include "core/payload.h"
#include "core/platform.h"
#include "core/utils.h"
#include "core/writer.h"
#include "math/complex.h"
#include "math/spectrum.h"
#include "math/vector.h"
#include "render/renderer.h"
#include "scene/background.h"
#include "scene/bvh.h"
#include "scene/material.h"
#include "scene/object.h"
#include "scene/texture.h"

class Parser {
    public:
        static bool hasValidExtension(const std::string & output) { return hasExtension(output, ".lrd"); }

        static void parseArguments(int argc, char * argv[], std::string & program, std::string & input, std::string & output) {
            program = argv[0];

            if (argc != 2 && argc != 3) error("invalid number of arguments");

            input = argv[1];

            if (!hasValidExtension(input)) fileError(input, "invalid file extension");

            if (argc == 3) {
                output = argv[2];

                if (!Writer::hasValidExtension(output)) fileError(output, "invalid file extension");
            } else {
                output = input;

                output.erase(output.size() - 4);

                output += ".exr";
            }
        }

        static void parseLRD(const std::string & input, Renderer & renderer, Payload & payload) {
            std::ifstream inputFile(input);

            if (!inputFile.is_open()) failedToOpenFileError(input);

            std::string line;
            int lineNumber = 0;
            bool renderCommandFound = false;
            bool backgroundCommandFound = false;
            bool cameraCommandFound = false;
            bool instanceCommandFound = false;

            std::vector<int> objectOffsets;

            std::map<std::string, int> spectrumIndices;
            std::map<std::string, int> complexSpectrumIndices;
            std::map<std::string, int> objectIndices;
            std::map<std::string, int> materialIndices;
            std::map<std::string, int> scalarTextureIndices;
            std::map<std::string, int> spectrumTextureIndices;

            std::map<std::string, int> imageDataIndices;

            std::vector<ColorSpace> imageSpaces;
            std::vector<int> imageIndices, imageWidths, imageHeights, imageChannels;

            payload.spectra.push_back(DenseSpectrum<Float>(0.5));

            payload.spectrumTextures.push_back(SpectrumTexture::makeConstant(0));
            spectrumTextureIndices["default"] = 0;

            payload.scalarTextures.push_back(ScalarTexture::makeConstant(0.5));
            scalarTextureIndices["default"] = 0;

            payload.materials.push_back(Material::makeLambertian(0));
            materialIndices["default"] = 0;

            while (std::getline(inputFile, line)) {
                lineNumber++;

                line = preprocess(line);
                if (line.empty()) continue;

                std::stringstream ss(line);

                std::string command;
                if (!(ss >> command)) continue;

                if (!renderCommandFound && command != "Render") fileError(input, lineNumber, "expected \"Render\", got \"" + command + "\"");

                if (command == "Render") {
                    renderCommandFound = true;

                    std::string spaceString = parseValue<std::string>(ss, input, lineNumber, "space");

                    ColorSpace space;

                    if (spaceString == "srgb") space = ColorSpace::SRGB;
                    else if (spaceString == "rec2020") space = ColorSpace::REC2020;
                    else if (spaceString == "aces2065-1") space = ColorSpace::ACES2065;
                    else fileError(input, lineNumber, "'space' must be \"aces2065-1\", \"rec2020\", or \"srgb\"");

                    int width = parseValue<int>(ss, input, lineNumber, "width");
                    if (width <= 0) fileError(input, lineNumber, "'width' must be positive, got " + std::to_string(width));

                    int height = parseValue<int>(ss, input, lineNumber, "height");
                    if (height <= 0) fileError(input, lineNumber, "'height' must be positive, got " + std::to_string(height));

                    int samples = parseValue<int>(ss, input, lineNumber, "samples");
                    if (samples < 1) fileError(input, lineNumber, "'samples' must be at least 1, got " + std::to_string(samples));

                    int sqrtSamples = int(std::sqrt(samples));
                    if (sqrtSamples * sqrtSamples != samples) fileError(input, lineNumber, "'samples' must be a perfect square, got " + std::to_string(samples));

                    int depth = parseValue<int>(ss, input, lineNumber, "depth");
                    if (depth < 0) fileError(input, lineNumber, "'depth' must be non-negative, got " + std::to_string(depth));

                    Float lambdaMin = parseValue<Float>(ss, input, lineNumber, "lambdaMin");
                    if (lambdaMin < 0) fileError(input, lineNumber, "'lambdaMin' must be non-negative, got " + std::to_string(lambdaMin));
                    if (lambdaMin < CIE_LAMBDA_MIN) fileError(input, lineNumber, "'lambdaMin' must be at least " + std::to_string(CIE_LAMBDA_MIN) + ", got " + std::to_string(lambdaMin));

                    Float lambdaMax = parseValue<Float>(ss, input, lineNumber, "lambdaMax");
                    if (lambdaMax < 0) fileError(input, lineNumber, "'lambdaMax' must be non-negative, got " + std::to_string(lambdaMax));
                    if (lambdaMax > CIE_LAMBDA_MAX) fileError(input, lineNumber, "'lambdaMax' must be at most " + std::to_string(CIE_LAMBDA_MAX) + ", got " + std::to_string(lambdaMax));

                    if (lambdaMin >= lambdaMax) fileError(input, lineNumber, "'lambdaMin' must be at most 'lambdaMax', got " + std::to_string(lambdaMin) + " and " + std::to_string(lambdaMax));

                    uint64_t seed = parseValue<uint64_t>(ss, input, lineNumber, "seed");

                    renderer = Renderer(space, width, height, samples, sqrtSamples, depth, lambdaMin, lambdaMax, seed);
                }
                else if (command == "Texture") {
                    std::string name = parseValue<std::string>(ss, input, lineNumber, "name");
                    std::string type = parseValue<std::string>(ss, input, lineNumber, "type");
                    std::string subtype = parseValue<std::string>(ss, input, lineNumber, "subtype");

                    if (scalarTextureIndices.contains(name) || spectrumTextureIndices.contains(name)) fileError(input, lineNumber, "'name' is already defined");

                    if (type == "scalar") {
                        if (subtype == "constant") payload.scalarTextures.push_back(ScalarTexture::makeConstant(parseValue<Float>(ss, input, lineNumber, "value")));
                        else if (subtype == "perlin" || subtype == "worley") {
                            Float min = parseValue<Float>(ss, input, lineNumber, "min");
                            Float max = parseValue<Float>(ss, input, lineNumber, "max");
                            Float frequency = parseValue<Float>(ss, input, lineNumber, "frequency");

                            if (min > max) fileError(input, lineNumber, "'min' must be at most 'max', got " + std::to_string(min) + " and " + std::to_string(max));

                            if (subtype == "perlin") payload.scalarTextures.push_back(ScalarTexture::makePerlin(min, max, frequency));
                            else if (subtype == "worley") payload.scalarTextures.push_back(ScalarTexture::makeWorley(min, max, frequency));
                        }
                        else if (subtype == "image") {
                            std::string imageFile = parseValue<std::string>(ss, input, lineNumber, "file");

                            ColorSpace inputSpace;
                            int imageIndex = -1, width, height;

                            if (imageDataIndices.contains(imageFile)) {
                                int imageDataIndex = imageDataIndices.at(imageFile);

                                if (imageChannels[imageDataIndex] == 1) {
                                    inputSpace = imageSpaces[imageDataIndex];
                                    imageIndex = imageIndices[imageDataIndex];
                                    width = imageWidths[imageDataIndex];
                                    height = imageHeights[imageDataIndex];
                                }
                            }

                            if (imageIndex == -1) {
                                imageIndex = int(payload.images.size());

                                parseImage(imageFile, payload, inputSpace, width, height, 1);

                                imageDataIndices[imageFile] = int(imageIndices.size());
                                imageSpaces.push_back(inputSpace);
                                imageIndices.push_back(imageIndex);
                                imageWidths.push_back(width);
                                imageHeights.push_back(height);
                                imageChannels.push_back(1);
                            }

                            payload.scalarTextures.push_back(ScalarTexture::makeImage(imageIndex, width, height));
                        }
                        else fileError(input, lineNumber, "'subtype' must be \"constant\", \"image\", \"perlin\", or \"worley\"");

                        scalarTextureIndices[name] = int(payload.scalarTextures.size() - 1);
                    }
                    else if (type == "spectrum") {
                        if (subtype == "constant") payload.spectrumTextures.push_back(SpectrumTexture::makeConstant(parseSpectrum<Float>(ss, input, lineNumber, "spectrum", payload, spectrumIndices, complexSpectrumIndices)));
                        else if (subtype == "checker") {
                            int value1 = parseSpectrum<Float>(ss, input, lineNumber, "value1", payload, spectrumIndices, complexSpectrumIndices);
                            int value2 = parseSpectrum<Float>(ss, input, lineNumber, "value2", payload, spectrumIndices, complexSpectrumIndices);

                            Float scale = parseValue<Float>(ss, input, lineNumber, "scale");

                            if (std::fabs(scale) < EPSILON_SQUARED) fileError(input, lineNumber, "'scale' must be non-zero, got " + std::to_string(scale));

                            payload.spectrumTextures.push_back(SpectrumTexture::makeChecker(value1, value2, scale));
                        }
                        else if (subtype == "scalar") {
                            std::string scalarTextureName = parseValue<std::string>(ss, input, lineNumber, "texture");

                            int scalarTextureIndex;

                            if (scalarTextureIndices.contains(scalarTextureName)) scalarTextureIndex = scalarTextureIndices.at(scalarTextureName);
                            else fileError(input, lineNumber, "'texture' is not defined");

                            payload.spectrumTextures.push_back(SpectrumTexture::makeScalar(scalarTextureIndex));
                        }
                        else if (subtype == "image") {
                            std::string imageFile = parseValue<std::string>(ss, input, lineNumber, "file");

                            ColorSpace inputSpace;
                            int imageIndex = -1, width, height;

                            if (imageDataIndices.contains(imageFile)) {
                                int imageDataIndex = imageDataIndices.at(imageFile);

                                if (imageChannels[imageDataIndex] == 3) {
                                    inputSpace = imageSpaces[imageDataIndex];
                                    imageIndex = imageIndices[imageDataIndex];
                                    width = imageWidths[imageDataIndex];
                                    height = imageHeights[imageDataIndex];
                                }
                            }

                            if (imageIndex == -1) {
                                imageIndex = int(payload.images.size());

                                parseImage(imageFile, payload, inputSpace, width, height, 3);

                                imageDataIndices[imageFile] = int(imageIndices.size());
                                imageSpaces.push_back(inputSpace);
                                imageIndices.push_back(imageIndex);
                                imageWidths.push_back(width);
                                imageHeights.push_back(height);
                                imageChannels.push_back(3);
                            }

                            payload.spectrumTextures.push_back(SpectrumTexture::makeImage(inputSpace, imageIndex, width, height));
                        }
                        else fileError(input, lineNumber, "'subtype' must be \"constant\", \"checker\", \"image\", or \"scalar\"");

                        spectrumTextureIndices[name] = int(payload.spectrumTextures.size() - 1);
                    }
                    else fileError(input, lineNumber, "'type' must be \"scalar\" or \"spectrum\"");
                }
                else if (command == "Material") {
                    std::string name = parseValue<std::string>(ss, input, lineNumber, "name");
                    std::string type = parseValue<std::string>(ss, input, lineNumber, "type");

                    if (materialIndices.contains(name)) fileError(input, lineNumber, "'name' is already defined");

                    if (type == "lambertian" || type == "mirror") {
                        int albedo = parseSpectrumTexture(ss, input, lineNumber, "albedo", payload, spectrumIndices, complexSpectrumIndices, scalarTextureIndices, spectrumTextureIndices);

                        Float minAlbedo = payload.spectrumTextures[albedo].min(payload.spectra.data(), payload.scalarTextures.data(), payload.images.data());
                        Float maxAlbedo = payload.spectrumTextures[albedo].max(payload.spectra.data(), payload.scalarTextures.data(), payload.images.data());

                        if (minAlbedo < 0) fileError(input, lineNumber, "'albedo' must be positive, got " + std::to_string(minAlbedo));
                        else if (maxAlbedo > 1) fileError(input, lineNumber, "'albedo' must be at most 1, got " + std::to_string(maxAlbedo));

                        if (type == "lambertian") payload.materials.push_back(Material::makeLambertian(albedo));
                        else if (type == "mirror") payload.materials.push_back(Material::makeMirror(albedo));
                    }
                    else if (type == "dielectric") {
                        int n0 = parseSpectrum<Float>(ss, input, lineNumber, "n0", payload, spectrumIndices, complexSpectrumIndices);
                        int n1 = parseSpectrum<Float>(ss, input, lineNumber, "n1", payload, spectrumIndices, complexSpectrumIndices);

                        for (int i = 0; i < CIE_LAMBDA_BINS; i++) {
                            if (payload.spectra[n0][i] <= 0) fileError(input, lineNumber, "'n0' must be positive, got " + std::to_string(payload.spectra[n0][i]));
                            if (payload.spectra[n1][i] <= 0) fileError(input, lineNumber, "'n1' must be positive, got " + std::to_string(payload.spectra[n1][i]));
                        }

                        payload.materials.push_back(Material::makeDielectric(n0, n1));
                    }
                    else if (type == "emissive") payload.materials.push_back(Material::makeEmissive(parseSpectrumTexture(ss, input, lineNumber, "emission", payload, spectrumIndices, complexSpectrumIndices, scalarTextureIndices, spectrumTextureIndices)));
                    else if (type == "thinfilm") {
                        std::vector<int> n = parseSpectrumArray(ss, input, lineNumber, "n", payload, spectrumIndices, complexSpectrumIndices);

                        if (n.size() < 2) fileError(input, lineNumber, "'n' must have at least 2 entries, got " + std::to_string(n.size()));

                        for (int i = 0; i < int(n.size()); i++)
                            for (int j = 0; j < CIE_LAMBDA_BINS; j++)
                                if (payload.complexSpectra[n[i]][j].real() <= 0) fileError(input, lineNumber, "'n' must be positive, got " + std::to_string(payload.complexSpectra[n[i]][j].real()) + " + " + std::to_string(payload.complexSpectra[n[i]][j].imag()) + "i");

                        int nOffset = int(payload.materialProperties.size());

                        payload.materialProperties.insert(payload.materialProperties.end(), n.begin(), n.end());

                        int numLayers = int(n.size() - 2);

                        std::vector<std::string> dNames = parseArray<std::string>(ss, input, lineNumber, "d");
                        if ((int)dNames.size() != numLayers) fileError(input, lineNumber, "'d' must have " + std::to_string(numLayers) + " entries, got " + std::to_string(dNames.size()));

                        int dOffset = int(payload.materialProperties.size());

                        for (int i = 0; i < numLayers; i++) {
                            try {
                                payload.scalarTextures.push_back(ScalarTexture::makeConstant(stoF(dNames[i])));

                                payload.materialProperties.push_back(int(payload.scalarTextures.size() - 1));
                            }
                            catch (const std::exception &) {
                                if (scalarTextureIndices.contains(dNames[i])) payload.materialProperties.push_back(scalarTextureIndices.at(dNames[i]));
                                else fileError(input, lineNumber, "'d' is not defined");
                            }

                            if (payload.scalarTextures[payload.materialProperties[dOffset + i]].min(payload.images.data()) <= 0) fileError(input, lineNumber, "'d' must be positive, got " + std::to_string(payload.scalarTextures[payload.materialProperties[dOffset + i]].min(payload.images.data())));
                        }

                        payload.materials.push_back(Material::makeThinFilm(numLayers, nOffset, dOffset));
                    }
                    else fileError(input, lineNumber, "'type' must be \"dielectric\", \"emissive\", \"lambertian\", \"mirror\", or \"thinfilm\"");

                    materialIndices[name] = int(payload.materials.size()) - 1;
                }
                else if (command == "Object") {
                    std::string name = parseValue<std::string>(ss, input, lineNumber, "name");
                    std::string type = parseValue<std::string>(ss, input, lineNumber, "type");
                    std::string materialName = parseValue<std::string>(ss, input, lineNumber, "material");

                    if (name == "default") fileError(input, lineNumber, "'name' is already defined");
                    else if (objectIndices.contains(name)) {
                        if (objectIndices.at(name) != int(objectOffsets.size()) - 1) fileError(input, lineNumber, "'name' is already defined");
                    }
                    else {
                        objectIndices[name] = int(objectOffsets.size());
                        objectOffsets.push_back(int(payload.objects.size()));
                    }

                    int material;

                    if (materialIndices.contains(materialName)) material = materialIndices.at(materialName);
                    else fileError(input, lineNumber, "'material' is not defined");

                    if (type == "sphere") {
                        Vector<Float> center = parseVector(ss, input, lineNumber, "center");
                        Float radius = parseValue<Float>(ss, input, lineNumber, "radius");

                        if (radius <= 0) fileError(input, lineNumber, "'radius' must be positive, got " + std::to_string(radius));

                        payload.objects.push_back(Object::makeSphere(material, center, radius));
                    }
                    else if (type == "quad" || type == "tri") {
                        Vector<Float> corner = parseVector(ss, input, lineNumber, "corner");
                        Vector<Float> horizontal = parseVector(ss, input, lineNumber, "horizontal");
                        Vector<Float> vertical = parseVector(ss, input, lineNumber, "vertical");

                        if (horizontal.length() < EPSILON_SQUARED) fileError(input, lineNumber, "'horizontal' must be non-zero");
                        if (vertical.length() < EPSILON_SQUARED) fileError(input, lineNumber, "'vertical' must be non-zero");
                        if (cross(horizontal, vertical).length() < EPSILON_SQUARED) fileError(input, lineNumber, "'horizontal' and 'vertical' must be non-parallel");

                        if (type == "quad") payload.objects.push_back(Object::makeQuad(material, corner, horizontal, vertical));
                        else if (type == "tri") payload.objects.push_back(Object::makeTri(material, corner, horizontal, vertical));
                    }
                    else if (type == "mesh") {
                        std::string objFile = parseValue<std::string>(ss, input, lineNumber, "file");

                        if (hasExtension(objFile, ".obj")) parseOBJ(objFile, payload, material);
                        else fileError(objFile, "invalid file extension");
                    }
                    else fileError(input, lineNumber, "'type' must be \"quad\", \"sphere\", or \"tri\"");
                }
                else if (command == "Instance") {
                    instanceCommandFound = true;

                    std::string objectName = parseValue<std::string>(ss, input, lineNumber, "object");
                    std::string materialName = parseValue<std::string>(ss, input, lineNumber, "material");

                    int objectIndex, object, count;

                    if (objectIndices.contains(objectName)) {
                        objectIndex = objectIndices.at(objectName);

                        object = objectOffsets[objectIndex];
                        count = objectIndex < int(objectOffsets.size()) - 1 ? objectOffsets[objectIndex + 1] - object : int(payload.objects.size()) - object;
                    }
                    else fileError(input, lineNumber, "'object' is not defined");

                    int material = -1;

                    if (materialName != "default") {
                        if (materialIndices.contains(materialName)) material = materialIndices.at(materialName);
                        else fileError(input, lineNumber, "'material' is not defined");
                    }

                    Vector<Float> translation = parseVector(ss, input, lineNumber, "translation");
                    Vector<Float> rotation = parseVector(ss, input, lineNumber, "rotation");
                    Vector<Float> scale = parseVector(ss, input, lineNumber, "scale");

                    if (std::fabs(scale[0]) < EPSILON_SQUARED || std::fabs(scale[1]) < EPSILON_SQUARED || std::fabs(scale[2]) < EPSILON_SQUARED) fileError(input, lineNumber, "'scale' must be non-zero");

                    Instance instance(object, count, material, translation, rotation, scale);

                    if (std::fabs(scale[0] - scale[1]) > EPSILON_SQUARED || std::fabs(scale[1] - scale[2]) > EPSILON_SQUARED)
                        for (int i = 0; i < count; i++)
                            if (payload.materials[instance.getMaterial(payload.objects.data(), i)].isEmissive()) fileError(input, lineNumber, "'scale' must be uniform for emissive objects");

                    payload.instances.push_back(instance);
                }
                else if (command == "Background") {
                    backgroundCommandFound = true;

                    std::string type = parseValue<std::string>(ss, input, lineNumber, "type");

                    if (type == "equirectangular") payload.background = Background::makeEquirectangular(parseSpectrumTexture(ss, input, lineNumber, "background", payload, spectrumIndices, complexSpectrumIndices, scalarTextureIndices, spectrumTextureIndices));
                    else fileError(input, lineNumber, "'type' must be \"equirectangular\"");
                }
                else if (command == "Camera") {
                    cameraCommandFound = true;

                    Vector<Float> position = parseVector(ss, input, lineNumber, "position");
                    Vector<Float> corner = parseVector(ss, input, lineNumber, "corner");
                    Vector<Float> horizontal = parseVector(ss, input, lineNumber, "horizontal");
                    Vector<Float> vertical = parseVector(ss, input, lineNumber, "vertical");

                    if (horizontal.length() < EPSILON_SQUARED) fileError(input, lineNumber, "'horizontal' must be non-zero");
                    if (vertical.length() < EPSILON_SQUARED) fileError(input, lineNumber, "'vertical' must be non-zero");
                    if (cross(horizontal, vertical).length() < EPSILON_SQUARED) fileError(input, lineNumber, "'horizontal' and 'vertical' must be non-parallel");

                    renderer.setCamera(position, corner, horizontal, vertical);
                }
                else fileError(input, lineNumber, "expected \"Background\", \"Camera\", \"Instance\", \"Material\", \"Render\", \"Object\", or \"Texture\", got \"" + command + "\"");

                if (ss >> command) fileError(input, lineNumber, "unexpected token \"" + command + "\"");
            }

            inputFile.close();

            if (!renderCommandFound) fileError(input, "expected \"Render\"");
            if (!backgroundCommandFound) fileError(input, "expected \"Background\"");
            if (!cameraCommandFound) fileError(input, "expected \"Camera\"");
            if (!instanceCommandFound) fileError(input, "expected \"Instance\"");

            objectOffsets.push_back(int(payload.objects.size()));

            payload.nodes = BVH::makeBVH(payload.objects, payload.instances, objectOffsets);

            Vector<Float> sceneCenter;
            Float sceneRadius = 0;

            for (int i = 0; i < int(payload.instances.size()); i++)
                for (int j = 0; j < payload.instances[i].getCount(); j++) {
                    int objectIndex = payload.instances[i].getObject() + j;

                    Vector<Float> objectCenter = payload.instances[i].center(payload.objects.data(), j);
                    Float objectRadius = payload.instances[i].radius(payload.objects.data(), j);

                    Vector<Float> sceneToObject = objectCenter - sceneCenter;
                    Float distance = sceneToObject.length();

                    if (distance + sceneRadius <= objectRadius) {
                        sceneCenter = objectCenter;
                        sceneRadius = objectRadius;
                    }
                    else if (distance + objectRadius > sceneRadius) {
                        Float newRadius = Float(0.5) * (distance + sceneRadius + objectRadius);

                        sceneCenter = sceneCenter + (newRadius - sceneRadius) * sceneToObject / distance;
                        sceneRadius = newRadius;
                    }

                    int material = payload.instances[i].getMaterial(payload.objects.data(), j);

                    if (payload.materials[material].isEmissive()) {
                        payload.lightInstances.push_back(i);
                        payload.lightObjects.push_back(objectIndex);

                        Float lightPower = payload.instances[i].area(payload.objects.data(), j) * payload.materials[material].averageEmission(payload.spectra.data(), payload.scalarTextures.data(), payload.spectrumTextures.data(), payload.images.data());

                        payload.lightPowers.push_back(lightPower);
                        payload.totalLightPower += lightPower;
                    }
                }

            payload.lightInstances.push_back(-1);
            payload.lightObjects.push_back(-1);

            Float backgroundLightPower = payload.background.area(sceneRadius) * payload.background.average(payload.spectra.data(), payload.scalarTextures.data(), payload.spectrumTextures.data(), payload.images.data());

            payload.lightPowers.push_back(backgroundLightPower);
            payload.totalLightPower += backgroundLightPower;
        }

        static bool parseUpsamplingTables(const std::string & input, std::vector<float> & scale, std::vector<float> & lut) {
            std::ifstream inputFile(input, std::ios::binary);

            if (!inputFile) return false;

            char header[4];

            inputFile.read(header, 4);

            if (!inputFile || std::memcmp(header, "SPEC", 4) != 0) return false;

            uint32_t resolution;

            inputFile.read(reinterpret_cast<char *>(&resolution), sizeof(uint32_t));

            if (!inputFile || resolution != UPSAMPLING_RESOLUTION) return false;

            inputFile.read(reinterpret_cast<char *>(scale.data()), sizeof(float) * resolution);
            inputFile.read(reinterpret_cast<char *>(lut.data()), sizeof(float) * 3 * resolution * resolution * resolution * 3);

            if (!inputFile) return false;

            return true;
        }

    private:
        static std::string preprocess(const std::string & s) {
            size_t commentPos = s.find('#');
            std::string stripped = commentPos == std::string::npos ? s : s.substr(0, commentPos);

            std::string result;

            for (char c : stripped) {
                if (c == '(' || c == ')' || c == '[' || c == ']') {
                    result += ' ';
                    result += c;
                    result += ' ';
                }
                else result += c;
            }

            return result;
        }

        static Float stoF(const std::string & s, size_t * index = nullptr) {
            size_t localIndex = 0;

            Float result = Float((sizeof(Float) == sizeof(float)) ? std::stof(s, &localIndex) : std::stod(s, &localIndex));

            if (localIndex < s.length()) invalidArgumentError("stoF");

            if (index) *index = localIndex;

            return result;
        }

        template <typename T>
        static T parseValue(std::stringstream & ss, const std::string & input, int lineNumber, const std::string & parameter) {
            T value;

            if (!(ss >> value)) fileError(input, lineNumber, "'" + parameter + "' is missing or invalid");

            return value;
        }

        static Complex parseComplex(std::string & token, const std::string & input, int lineNumber, const std::string & parameter) {
            std::string message = "'" + parameter + "' is missing or invalid";

            size_t index = 0;

            double real = 0, imaginary = 0;

            if (token == "i") {
                imaginary = 1;
                return Complex(real, imaginary);
            }
            else if (token == "-i") {
                imaginary = -1;
                return Complex(real, imaginary);
            }

            try {
                real = std::stod(token, &index);

                if (index < token.length()) {
                    token = token.substr(index);

                    if (token.back() == 'i') token.pop_back();
                    else fileError(input, lineNumber, message);

                    if (token == "+") imaginary = 1;
                    else if (token == "-") imaginary = -1;
                    else if (token.empty()) {
                        imaginary = real;
                        real = 0;
                    }
                    else imaginary = std::stod(token);
                }
            }
            catch (const std::exception &) {
                fileError(input, lineNumber, message);
            }

            return Complex(real, imaginary);
        }

        static Vector<Float> parseVector(std::stringstream & ss, const std::string & input, int lineNumber, const std::string & parameter) {
            std::string message = "'" + parameter + "' is missing or invalid";

            std::string token;

            if (!(ss >> token) || token != "(") fileError(input, lineNumber, message);

            Vector<Float> vector;

            if (!(ss >> vector[0])) fileError(input, lineNumber, message);
            if (!(ss >> vector[1])) fileError(input, lineNumber, message);
            if (!(ss >> vector[2])) fileError(input, lineNumber, message);

            if (!(ss >> token) || token != ")") fileError(input, lineNumber, message);

            return vector;
        }

        template <typename T>
        static int parseSpectrum(std::stringstream & ss, const std::string & input, int lineNumber, const std::string & parameter, Payload & payload, std::map<std::string, int> & spectrumIndices, std::map<std::string, int> & complexSpectrumIndices) {
            std::string message = "'" + parameter + "' is missing or invalid";

            std::string token;

            if (!(ss >> token)) fileError(input, lineNumber, message);

            if (token != "(" && !hasExtension(token, ".spd")) {
                if constexpr (std::is_same_v<T, Float>) {
                    try {
                        payload.spectra.push_back(DenseSpectrum<Float>(stoF(token)));

                        return int(payload.spectra.size() - 1);
                    } catch (const std::exception &) {
                        fileError(input, lineNumber, message);
                    }
                }
                else if constexpr (std::is_same_v<T, Complex>) {
                    payload.complexSpectra.push_back(DenseSpectrum<Complex>(parseComplex(token, input, lineNumber, parameter)));

                    return int(payload.complexSpectra.size() - 1);
                }
                else fileError(input, lineNumber, message);
            }

            std::map<double, T> samples;

            if (token != "(") {
                if (hasExtension(token, ".spd")) {
                    if constexpr (std::is_same_v<T, Float>) {
                        if (spectrumIndices.contains(token)) return spectrumIndices.at(token);
                        else spectrumIndices[token] = int(payload.spectra.size());
                    }
                    else if constexpr (std::is_same_v<T, Complex>) {
                        if (complexSpectrumIndices.contains(token)) return complexSpectrumIndices.at(token);
                        else complexSpectrumIndices[token] = int(payload.complexSpectra.size());
                    }

                    parseSPD(token, samples);
                }
                else fileError(token, "invalid file extension");
            }
            else {
                while (ss >> token) {
                    if (token == ")") break;

                    double lambda;

                    try {
                        lambda = std::stod(token);
                    } catch (const std::exception &) {
                        fileError(input, lineNumber, message);
                    }

                    if (!(ss >> token)) fileError(input, lineNumber, message);

                    if constexpr (std::is_same_v<T, Float>) {
                        try {
                            samples[lambda] = stoF(token);
                        } catch (const std::exception &) {
                            fileError(input, lineNumber, message);
                        }
                    }
                    else if constexpr (std::is_same_v<T, Complex>) samples[lambda] = parseComplex(token, input, lineNumber, parameter);
                    else fileError(input, lineNumber, message);
                }

                if (token != ")") fileError(input, lineNumber, message);
            }

            if (samples.empty()) fileError(input, lineNumber, message);

            DenseSpectrum<T> spectrum;

            auto iterator = samples.begin();

            double sampleMin = iterator->first;
            double sampleMax = samples.rbegin()->first;

            for (int i = 0; i < CIE_LAMBDA_BINS; i++) {
                if (i <= sampleMin - CIE_LAMBDA_MIN || samples.size() == 1) spectrum[i] = samples[sampleMin];
                else if (i >= sampleMax - CIE_LAMBDA_MIN) spectrum[i] = samples[sampleMax];
                else {
                    double lambda = i + CIE_LAMBDA_MIN;

                    while (std::next(iterator) != samples.end() && std::next(iterator)->first < lambda) iterator++;

                    double min = iterator->first;
                    double max = std::next(iterator)->first;

                    spectrum[i] = T((max - lambda) / (max - min) * samples[min] + (lambda - min) / (max - min) * samples[max]);
                }
            }

            if constexpr (std::is_same_v<T, Float>) {
                payload.spectra.push_back(spectrum);

                return int(payload.spectra.size() - 1);
            }
            else if constexpr (std::is_same_v<T, Complex>) {
                payload.complexSpectra.push_back(spectrum);

                return int(payload.complexSpectra.size() - 1);
            }
            else fileError(input, lineNumber, message);
        }

        template <typename T>
        static std::vector<T> parseArray(std::stringstream & ss, const std::string & input, int lineNumber, const std::string & parameter) {
            std::string message = "'" + parameter + "' is missing or invalid";

            std::string token;

            if (!(ss >> token) || token != "[") fileError(input, lineNumber, message);

            std::vector<T> values;

            while (ss >> token) {
                if (token == "]") return values;

                if constexpr (std::is_same_v<T, Float>) {
                    try {
                        values.push_back(stoF(token));
                    } catch (const std::exception &) {
                        fileError(input, lineNumber, message);
                    }
                }
                else if constexpr (std::is_same_v<T, std::string>) values.push_back(token);
                else fileError(input, lineNumber, message);
            }

            fileError(input, lineNumber, message);
        }

        static std::vector<int> parseSpectrumArray(std::stringstream & ss, const std::string & input, int lineNumber, const std::string & parameter, Payload & payload, std::map<std::string, int> & spectrumIndices, std::map<std::string, int> & complexSpectrumIndices) {
            std::string message = "'" + parameter + "' is missing or invalid";

            std::string token;

            if (!(ss >> token) || token != "[") fileError(input, lineNumber, message);

            std::streampos pos = ss.tellg();

            std::vector<int> values;

            while (ss >> token) {
                if (token == "]") return values;

                ss.seekg(pos);

                values.push_back(parseSpectrum<Complex>(ss, input, lineNumber, parameter, payload, spectrumIndices, complexSpectrumIndices));

                pos = ss.tellg();
            }

            fileError(input, lineNumber, message);
        }

        static int parseSpectrumTexture(std::stringstream & ss, const std::string & input, int lineNumber, const std::string & parameter, Payload & payload, std::map<std::string, int> & spectrumIndices, std::map<std::string, int> & complexSpectrumIndices, const std::map<std::string, int> & scalarTextureIndices, std::map<std::string, int> & spectrumTextureIndices) {
            std::streampos pos = ss.tellg();

            try {
                payload.spectrumTextures.push_back(SpectrumTexture::makeConstant(parseSpectrum<Float>(ss, input, lineNumber, parameter, payload, spectrumIndices, complexSpectrumIndices)));

                return int(payload.spectrumTextures.size() - 1);
            }
            catch (const std::exception &) {
                ss.seekg(pos);

                std::string name = parseValue<std::string>(ss, input, lineNumber, parameter);

                if (spectrumTextureIndices.contains(name)) return spectrumTextureIndices.at(name);
                else if (scalarTextureIndices.contains(name)) {
                    int scalarTextureIndex = scalarTextureIndices.at(name);

                    payload.spectrumTextures.push_back(SpectrumTexture::makeScalar(scalarTextureIndex));

                    return int(payload.spectrumTextures.size()) - 1;
                }
                else fileError(input, lineNumber, "'" + parameter + "' is not defined");
            }
        }

        static void parseImage(const std::string & input, Payload & payload, ColorSpace & space, int & width, int & height, int channels) {
            bool is16Bit = stbi_is_16_bit(input.c_str());
            bool is32Bit = stbi_is_hdr(input.c_str());
            bool isEXR = IsEXR(input.c_str()) == TINYEXR_SUCCESS;

            bool sRGB = !isEXR && !is32Bit && channels == 3;

            int nativeChannels = 0;

            unsigned char * data8 = nullptr;
            unsigned short * data16 = nullptr;
            float * data32 = nullptr;
            float * dataEXR = nullptr;

            const char * errorEXR = nullptr;

            Float rx = chromaticity(ColorSpace::SRGB, 0);
            Float ry = chromaticity(ColorSpace::SRGB, 1);
            Float gx = chromaticity(ColorSpace::SRGB, 2);
            Float gy = chromaticity(ColorSpace::SRGB, 3);
            Float bx = chromaticity(ColorSpace::SRGB, 4);
            Float by = chromaticity(ColorSpace::SRGB, 5);
            Float wx = chromaticity(ColorSpace::SRGB, 6);
            Float wy = chromaticity(ColorSpace::SRGB, 7);

            if (isEXR) {
                EXRVersion version;

                if (ParseEXRVersionFromFile(&version, input.c_str()) == TINYEXR_SUCCESS) {
                    EXRHeader header;

                    InitEXRHeader(&header);

                    if (ParseEXRHeaderFromFile(&header, &version, input.c_str(), &errorEXR) == TINYEXR_SUCCESS) {
                        for (int i = 0; i < header.num_custom_attributes; i++) {
                            if (std::strcmp(header.custom_attributes[i].name, "chromaticities") == 0) {
                                const float * chromas = reinterpret_cast<const float *>(header.custom_attributes[i].value);

                                rx = Float(chromas[0]);
                                ry = Float(chromas[1]);
                                gx = Float(chromas[2]);
                                gy = Float(chromas[3]);
                                bx = Float(chromas[4]);
                                by = Float(chromas[5]);
                                wx = Float(chromas[6]);
                                wy = Float(chromas[7]);
                            }
                        }

                        FreeEXRHeader(&header);
                    }
                }

                if (LoadEXR(&dataEXR, &width, &height, input.c_str(), &errorEXR) != TINYEXR_SUCCESS) {
                    std::string error = errorEXR;
                    FreeEXRErrorMessage(errorEXR);
                    fileError(input, "failed to open file (" + error + ")");
                }
            }
            else if (is32Bit) data32 = stbi_loadf(input.c_str(), &width, &height, &nativeChannels, channels);
            else if (is16Bit) data16 = stbi_load_16(input.c_str(), &width, &height, &nativeChannels, channels);
            else data8 = stbi_load(input.c_str(), &width, &height, &nativeChannels, channels);

            if (!data8 && !data16 && !data32 && !dataEXR) {
                std::string error = stbi_failure_reason();
                if (error == "unknown image type") fileError(input, "invalid file extension");
                else fileError(input, "failed to open file (" + error + ")");
            }

            space = ColorSpace::SRGB;

            Matrix3<double> colorTransform(1, 0, 0, 0, 1, 0, 0, 0, 1);

            if (channels == 3) {
                if (colorSpaceContains(ColorSpace::SRGB, rx, ry, gx, gy, bx, by, wx, wy)) space = ColorSpace::SRGB;
                else if (colorSpaceContains(ColorSpace::REC2020, rx, ry, gx, gy, bx, by, wx, wy)) space = ColorSpace::REC2020;
                else space = ColorSpace::ACES2065;

                if (std::fabs(rx - chromaticity(space, 0)) > EPSILON_SQUARED || std::fabs(ry - chromaticity(space, 1)) > EPSILON_SQUARED || std::fabs(gx - chromaticity(space, 2)) > EPSILON_SQUARED || std::fabs(gy - chromaticity(space, 3)) > EPSILON_SQUARED || std::fabs(bx - chromaticity(space, 4)) > EPSILON_SQUARED || std::fabs(by - chromaticity(space, 5)) > EPSILON_SQUARED || std::fabs(wx - chromaticity(space, 6)) > EPSILON_SQUARED || std::fabs(wy - chromaticity(space, 7)) > EPSILON_SQUARED)  colorTransform = toRGBMatrix(space) * bradfordAdapt(wx, wy, chromaticity(space, 6), chromaticity(space, 7)) * toXYZMatrix(rx, ry, gx, gy, bx, by, wx, wy);
            }

            size_t totalElements = width * height * channels;

            payload.images.reserve(payload.images.size() + totalElements);

            for (size_t i = 0, j = 0; i < totalElements; i++, j += i % channels == 0 ? 4 - channels + 1 : 1) {
                Float value = isEXR ? (std::isfinite(dataEXR[j]) ? Float(dataEXR[j]) : Float(0)) : is32Bit ? (std::isfinite(data32[i]) ? Float(data32[i]) : Float(0)) : is16Bit ? Float(data16[i]) / Float(65535) : Float(data8[i]) / Float(255);

                if (sRGB) value = sRGBToLinear(value);

                payload.images.push_back(value);

                if (channels == 3 && i % 3 == 2) {
                    Vector<Float> color(payload.images[payload.images.size() - 3], payload.images[payload.images.size() - 2], payload.images[payload.images.size() - 1]);

                    color = transform(colorTransform, color);

                    payload.images[payload.images.size() - 3] = color[0];
                    payload.images[payload.images.size() - 2] = color[1];
                    payload.images[payload.images.size() - 1] = color[2];
                }
            }

            if (data8) stbi_image_free(data8);
            if (data16) stbi_image_free(data16);
            if (data32) stbi_image_free(data32);
            if (dataEXR) free(dataEXR);
        }

        static void parseOBJ(const std::string & input, Payload & payload, int material) {
            std::ifstream inputFile(input);

            if (!inputFile.is_open()) failedToOpenFileError(input);

            std::string line;
            int lineNumber = 0;
            bool fTagFound = false;

            std::vector<Vector<Float>> vertices;
            std::vector<Float> u;
            std::vector<Float> v;
            std::vector<Vector<Float>> normals;

            while (std::getline(inputFile, line)) {
                lineNumber++;

                if (line.empty()) continue;

                std::stringstream ss(line);

                std::string tag;
                if (!(ss >> tag)) continue;

                if (tag == "v") {
                    Vector<Float> vertex;

                    if (!(ss >> vertex[0])) fileError(input, lineNumber, "'x' is missing or invalid");
                    if (!(ss >> vertex[1])) fileError(input, lineNumber, "'y' is missing or invalid");
                    if (!(ss >> vertex[2])) fileError(input, lineNumber, "'z' is missing or invalid");

                    vertices.push_back(vertex);
                }
                else if (tag == "vt") {
                    Float uValue, vValue;

                    if (!(ss >> uValue)) fileError(input, lineNumber, "'u' is missing or invalid");
                    if (!(ss >> vValue)) fileError(input, lineNumber, "'v' is missing or invalid");

                    u.push_back(uValue);
                    v.push_back(vValue);
                }
                else if (tag == "vn") {
                    Vector<Float> normal;

                    if (!(ss >> normal[0])) fileError(input, lineNumber, "'x' is missing or invalid");
                    if (!(ss >> normal[1])) fileError(input, lineNumber, "'y' is missing or invalid");
                    if (!(ss >> normal[2])) fileError(input, lineNumber, "'z' is missing or invalid");

                    normals.push_back(normal);
                }
                else if (tag == "f") {
                    fTagFound = true;

                    std::string token;

                    std::vector<int> vertexIndices, textureIndices, normalIndices;

                    while (ss >> token) {
                        std::stringstream tokenStream(token);

                        std::string vertexIndexString, textureIndexString, normalIndexString;

                        if (!std::getline(tokenStream, vertexIndexString, '/')) fileError(input, lineNumber, "'v' is missing or invalid");
                        std::getline(tokenStream, textureIndexString, '/');
                        std::getline(tokenStream, normalIndexString, '/');

                        int vertexIndex, textureIndex, normalIndex;

                        try {
                            vertexIndex = std::stoi(vertexIndexString);
                        } catch (const std::exception &) {
                            fileError(input, lineNumber, "'v' is missing or invalid");
                        }

                        if (vertexIndex < 0 && int(vertices.size()) + vertexIndex >= 0) vertexIndex = int(vertices.size()) + vertexIndex;
                        else if (vertexIndex < 0) fileError(input, lineNumber, "'v' must be positive, got " + std::to_string(vertexIndex));
                        else if (vertexIndex > int(vertices.size())) fileError(input, lineNumber, "'v' must be at most " + std::to_string(int(vertices.size()) - 1) + ", got " + std::to_string(vertexIndex));
                        else vertexIndex--;

                        vertexIndices.push_back(vertexIndex);

                        if (!textureIndexString.empty()) {
                            try {
                                textureIndex = std::stoi(textureIndexString);
                            } catch (const std::exception &) {
                                fileError(input, lineNumber, "'vt' is missing or invalid");
                            }

                            if (textureIndex < 0 && int(u.size()) + textureIndex >= 0) textureIndex = int(u.size()) + textureIndex;
                            else if (textureIndex < 0) fileError(input, lineNumber, "'vt' must be positive, got " + std::to_string(textureIndex));
                            else if (textureIndex > int(u.size())) fileError(input, lineNumber, "'vt' must be at most " + std::to_string(int(u.size()) - 1) + ", got " + std::to_string(textureIndex));
                            else textureIndex--;
                        }
                        else textureIndex = -1;

                        textureIndices.push_back(textureIndex);

                        if (!normalIndexString.empty()) {
                            try {
                                normalIndex = std::stoi(normalIndexString);
                            } catch (const std::exception &) {
                                fileError(input, lineNumber, "'vn' is missing or invalid");
                            }

                            if (normalIndex < 0 && int(normals.size()) + normalIndex >= 0) normalIndex = int(normals.size()) + normalIndex;
                            else if (normalIndex < 0) fileError(input, lineNumber, "'vn' must be positive, got " + std::to_string(normalIndex));
                            else if (normalIndex > int(normals.size())) fileError(input, lineNumber, "'vn' must be at most " + std::to_string(int(normals.size()) - 1) + ", got " + std::to_string(normalIndex));
                            else normalIndex--;
                        }
                        else normalIndex = -1;

                        normalIndices.push_back(normalIndex);
                    }

                    if (vertexIndices.size() < 3) fileError(input, lineNumber, "'f' must have at least 3 vertices");

                    Vector<Float> vertex0 = vertices[vertexIndices[0]];
                    Vector<Float> normal0 = normalIndices[0] >= 0 ? normals[normalIndices[0]] : Vector<Float>(0, 0, 0);
                    Float u0 = textureIndices[0] >= 0 ? u[textureIndices[0]] : 0;
                    Float v0 = textureIndices[0] >= 0 ? v[textureIndices[0]] : 0;

                    for (int i = 1; i < int(vertexIndices.size()) - 1; i++) {
                        Vector<Float> vertex1 = vertices[vertexIndices[i]];
                        Vector<Float> normal1 = normalIndices[i] >= 0 ? normals[normalIndices[i]] : Vector<Float>(0, 0, 0);
                        Float u1 = textureIndices[i] >= 0 ? u[textureIndices[i]] : 1;
                        Float v1 = textureIndices[i] >= 0 ? v[textureIndices[i]] : 0;

                        Vector<Float> vertex2 = vertices[vertexIndices[i + 1]];
                        Vector<Float> normal2 = normalIndices[i + 1] >= 0 ? normals[normalIndices[i + 1]] : Vector<Float>(0, 0, 0);
                        Float u2 = textureIndices[i + 1] >= 0 ? u[textureIndices[i + 1]] : 0;
                        Float v2 = textureIndices[i + 1] >= 0 ? v[textureIndices[i + 1]] : 1;

                        if ((vertex1 - vertex0).length() < EPSILON_SQUARED) fileError(input, lineNumber, "'f' must be non-degenerate");
                        if ((vertex2 - vertex0).length() < EPSILON_SQUARED) fileError(input, lineNumber, "'f' must be non-degenerate");
                        if (cross(vertex1 - vertex0, vertex2 - vertex0).length() < EPSILON_SQUARED) fileError(input, lineNumber, "'f' must be non-degenerate");

                        payload.objects.push_back(Object::makeTri(material, vertex0, vertex1 - vertex0, vertex2 - vertex0, normal0, normal1, normal2, u0, u1, u2, v0, v1, v2));
                    }
                }
                else continue;

                if (ss >> tag) fileError(input, lineNumber, "unexpected token \"" + tag + "\"");
            }

            if (!fTagFound) fileError(input, lineNumber, "expected \"f\"");

            inputFile.close();
        }

        template <typename T>
        static void parseSPD(const std::string & input, std::map<double, T> & samples) {
            std::ifstream inputFile(input);

            if (!inputFile.is_open()) failedToOpenFileError(input);

            std::string line, token;
            int lineNumber = 0;

            while (std::getline(inputFile, line)) {
                lineNumber++;

                std::stringstream ss(preprocess(line));

                if (!(ss >> token)) continue;

                double lambda;

                try {
                    lambda = std::stod(token);
                } catch (const std::exception &) {
                    fileError(input, lineNumber, "'lambda' is missing or invalid");
                }

                if (!(ss >> token)) fileError(input, lineNumber, "'value' is missing or invalid");

                if constexpr (std::is_same_v<T, Float>) {
                    try {
                        samples[lambda] = stoF(token);
                    } catch (const std::exception &) {
                        fileError(input, lineNumber, "'value' is missing or invalid");
                    }
                }
                else if constexpr (std::is_same_v<T, Complex>) samples[lambda] = parseComplex(token, input, lineNumber, "value");
                else fileError(input, lineNumber, "'value' is missing or invalid");

                if (ss >> token) fileError(input, lineNumber, "unexpected token \"" + token + "\"");
            }

            inputFile.close();
        }
};