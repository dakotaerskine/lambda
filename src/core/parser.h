#pragma once

#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "core/constants.h"
#include "core/platform.h"
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

        static void parseLRD(const std::string & input, Renderer & renderer, std::vector<DenseSpectrum<Float>> & spectra, std::vector<DenseSpectrum<Complex>> & complexSpectra, Background & background, std::vector<Object> & objects, std::vector<Instance> & instances, std::vector<BVHNode> & nodes, std::vector<int> & lightInstances, std::vector<int> & lightObjects, std::vector<Float> & lightPowers, Float & totalLightPower, std::vector<Material> & materials, std::vector<int> & materialProperties, std::vector<ScalarTexture> & scalarTextures, std::vector<SpectrumTexture> & spectrumTextures) {
            std::ifstream inputFile(input);

            if (!inputFile.is_open()) {
                std::string error = std::strerror(errno);
                error[0] = char(std::tolower(error[0]));
                throw std::runtime_error(input + ": failed to open file (" + error + ")");
            }

            std::string line;
            int lineNumber = 0;
            bool renderCommandFound = false;
            bool backgroundCommandFound = false;
            bool cameraCommandFound = false;
            bool instanceCommandFound = false;

            std::vector<int> objectOffsets;
            std::map<std::string, int> objectIndices;
            std::map<std::string, int> materialIndices;
            std::map<std::string, int> scalarTextureIndices;
            std::map<std::string, int> spectrumTextureIndices;

            spectra.push_back(DenseSpectrum<Float>(0.5));

            spectrumTextures.push_back(SpectrumTexture::makeConstant(0));
            spectrumTextureIndices["default"] = 0;

            scalarTextures.push_back(ScalarTexture::makeConstant(0.5));
            scalarTextureIndices["default"] = 0;

            materials.push_back(Material::makeLambertian(0));
            materialIndices["default"] = 0;

            while (std::getline(inputFile, line)) {
                lineNumber++;

                line = preprocess(line);
                if (line.empty()) continue;

                std::stringstream ss(line);

                std::string command;
                if (!(ss >> command)) continue;

                if (!renderCommandFound && command != "Render") throw error(input, lineNumber, "expected \"Render\", got \"" + command + "\"");

                if (command == "Render") {
                    renderCommandFound = true;

                    int width = parseValue<int>(ss, input, lineNumber, "width");
                    if (width <= 0) throw error(input, lineNumber, "'width' must be positive, got " + std::to_string(width));

                    int height = parseValue<int>(ss, input, lineNumber, "height");
                    if (height <= 0) throw error(input, lineNumber, "'height' must be positive, got " + std::to_string(height));

                    int depth = parseValue<int>(ss, input, lineNumber, "depth");
                    if (depth < 0) throw error(input, lineNumber, "'depth' must be non-negative, got " + std::to_string(depth));

                    int samples = parseValue<int>(ss, input, lineNumber, "samples");
                    if (samples < 1) throw error(input, lineNumber, "'samples' must be at least 1, got " + std::to_string(samples));

                    int sqrtSamples = int(std::sqrt(samples));
                    if (sqrtSamples * sqrtSamples != samples) throw error(input, lineNumber, "'samples' must be a perfect square, got " + std::to_string(samples));

                    Float lambdaMin = parseValue<Float>(ss, input, lineNumber, "lambdaMin");
                    if (lambdaMin < 0) throw error(input, lineNumber, "'lambdaMin' must be non-negative, got " + std::to_string(lambdaMin));
                    if (lambdaMin < CIE_LAMBDA_MIN) throw error(input, lineNumber, "'lambdaMin' must be at least " + std::to_string(CIE_LAMBDA_MIN) + ", got " + std::to_string(lambdaMin));

                    Float lambdaMax = parseValue<Float>(ss, input, lineNumber, "lambdaMax");
                    if (lambdaMax < 0) throw error(input, lineNumber, "'lambdaMax' must be non-negative, got " + std::to_string(lambdaMax));
                    if (lambdaMax > CIE_LAMBDA_MAX) throw error(input, lineNumber, "'lambdaMax' must be at most " + std::to_string(CIE_LAMBDA_MAX) + ", got " + std::to_string(lambdaMax));

                    if (lambdaMin >= lambdaMax) throw error(input, lineNumber, "'lambdaMin' must be at most 'lambdaMax', got " + std::to_string(lambdaMin) + " and " + std::to_string(lambdaMax));

                    renderer = Renderer(width, height, depth, samples, sqrtSamples, lambdaMin, lambdaMax);
                }
                else if (command == "Texture") {
                    std::string name = parseValue<std::string>(ss, input, lineNumber, "name");
                    std::string type = parseValue<std::string>(ss, input, lineNumber, "type");
                    std::string subtype = parseValue<std::string>(ss, input, lineNumber, "subtype");

                    if (scalarTextureIndices.contains(name) || spectrumTextureIndices.contains(name)) throw error(input, lineNumber, "'name' is already defined");

                    if (type == "scalar") {
                        if (subtype == "constant") scalarTextures.push_back(ScalarTexture::makeConstant(parseValue<Float>(ss, input, lineNumber, "value")));
                        else if (subtype == "perlin" || subtype == "worley") {
                            Float min = parseValue<Float>(ss, input, lineNumber, "min");
                            Float max = parseValue<Float>(ss, input, lineNumber, "max");
                            Float frequency = parseValue<Float>(ss, input, lineNumber, "frequency");

                            if (min > max) throw error(input, lineNumber, "'min' must be at most 'max', got " + std::to_string(min) + " and " + std::to_string(max));

                            if (subtype == "perlin") scalarTextures.push_back(ScalarTexture::makePerlin(min, max, frequency));
                            else if (subtype == "worley") scalarTextures.push_back(ScalarTexture::makeWorley(min, max, frequency));
                        }
                        else throw error(input, lineNumber, "'subtype' must be \"constant\", \"perlin\", or \"worley\"");

                        scalarTextureIndices[name] = int(scalarTextures.size() - 1);
                    }
                    else if (type == "spectrum") {
                        if (subtype == "constant") spectrumTextures.push_back(SpectrumTexture::makeConstant(parseSpectrum<Float>(ss, input, lineNumber, "spectrum", spectra, complexSpectra)));
                        else if (subtype == "checker") {
                            int value1 = parseSpectrum<Float>(ss, input, lineNumber, "value1", spectra, complexSpectra);
                            int value2 = parseSpectrum<Float>(ss, input, lineNumber, "value2", spectra, complexSpectra);

                            Float scale = parseValue<Float>(ss, input, lineNumber, "scale");

                            spectrumTextures.push_back(SpectrumTexture::makeChecker(value1, value2, scale));
                        }
                        else if (subtype == "scalar") {
                            int scalarTextureIndex;
                            
                            if (scalarTextureIndices.contains(parseValue<std::string>(ss, input, lineNumber, "texture"))) scalarTextureIndex = scalarTextureIndices.at(parseValue<std::string>(ss, input, lineNumber, "texture"));
                            else throw error(input, lineNumber, "'texture' is not defined");

                            spectrumTextures.push_back(SpectrumTexture::makeScalar(scalarTextureIndex));
                        }
                        else throw error(input, lineNumber, "'subtype' must be \"constant\", \"checker\", or \"scalar\"");

                        spectrumTextureIndices[name] = int(spectrumTextures.size() - 1);
                    }
                    else throw error(input, lineNumber, "'type' must be \"scalar\" or \"spectrum\"");
                }
                else if (command == "Material") {
                    std::string name = parseValue<std::string>(ss, input, lineNumber, "name");
                    std::string type = parseValue<std::string>(ss, input, lineNumber, "type");

                    if (materialIndices.contains(name)) throw error(input, lineNumber, "'name' is already defined");

                    if (type == "lambertian" || type == "mirror") {
                        int albedo = parseSpectrumTexture(ss, input, lineNumber, "albedo", spectra, complexSpectra, scalarTextureIndices, spectrumTextures, spectrumTextureIndices);

                        Float minAlbedo = spectrumTextures[albedo].min(spectra.data(), scalarTextures.data());
                        Float maxAlbedo = spectrumTextures[albedo].max(spectra.data(), scalarTextures.data());

                        if (minAlbedo < 0) throw error(input, lineNumber, "'albedo' must be positive, got " + std::to_string(minAlbedo));
                        else if (maxAlbedo > 1) throw error(input, lineNumber, "'albedo' must be at most 1, got " + std::to_string(maxAlbedo));

                        if (type == "lambertian") materials.push_back(Material::makeLambertian(albedo));
                        else if (type == "mirror") materials.push_back(Material::makeMirror(albedo));
                    }
                    else if (type == "dielectric") {
                        int n0 = parseSpectrum<Float>(ss, input, lineNumber, "n0", spectra, complexSpectra);
                        int n1 = parseSpectrum<Float>(ss, input, lineNumber, "n1", spectra, complexSpectra);

                        for (int i = 0; i < CIE_LAMBDA_BINS; i++) {
                            if (spectra[n0][i] <= 0) throw error(input, lineNumber, "'n0' must be positive, got " + std::to_string(spectra[n0][i]));
                            if (spectra[n1][i] <= 0) throw error(input, lineNumber, "'n1' must be positive, got " + std::to_string(spectra[n1][i]));
                        }

                        materials.push_back(Material::makeDielectric(n0, n1));
                    }
                    else if (type == "emissive") materials.push_back(Material::makeEmissive(parseSpectrumTexture(ss, input, lineNumber, "emission", spectra, complexSpectra, scalarTextureIndices, spectrumTextures, spectrumTextureIndices)));
                    else if (type == "thinfilm") {
                        std::vector<int> n = parseSpectrumArray(ss, input, lineNumber, "n", spectra, complexSpectra);

                        if (n.size() < 2) throw error(input, lineNumber, "'n' must have at least 2 entries, got " + std::to_string(n.size()));

                        int nOffset = int(materialProperties.size());

                        materialProperties.insert(materialProperties.end(), n.begin(), n.end());

                        int numLayers = int(n.size() - 2);

                        std::vector<std::string> dNames = parseArray<std::string>(ss, input, lineNumber, "d");
                        if ((int)dNames.size() != numLayers) throw error(input, lineNumber, "'d' must have " + std::to_string(numLayers) + " entries, got " + std::to_string(dNames.size()));

                        int dOffset = int(materialProperties.size());

                        for (int i = 0; i < numLayers; i++) {
                            try {
                                scalarTextures.push_back(ScalarTexture::makeConstant(stoF(dNames[i])));

                                materialProperties.push_back(int(scalarTextures.size() - 1));
                            }
                            catch (const std::exception &) {
                                if (scalarTextureIndices.contains(dNames[i])) materialProperties.push_back(scalarTextureIndices.at(dNames[i]));
                                else throw error(input, lineNumber, "'d' is not defined");
                            }
                        }

                        materials.push_back(Material::makeThinFilm(numLayers, nOffset, dOffset));
                    }
                    else throw error(input, lineNumber, "'type' must be \"dielectric\", \"emissive\", \"lambertian\", \"mirror\", or \"thinfilm\"");

                    materialIndices[name] = int(materials.size()) - 1;
                }
                else if (command == "Object") {
                    std::string name = parseValue<std::string>(ss, input, lineNumber, "name");
                    std::string type = parseValue<std::string>(ss, input, lineNumber, "type");
                    std::string materialName = parseValue<std::string>(ss, input, lineNumber, "material");

                    if (name == "default") throw error(input, lineNumber, "'name' is already defined");
                    else if (objectIndices.contains(name)) {
                        if (objectIndices.at(name) != int(objectOffsets.size()) - 1) throw error(input, lineNumber, "'name' is already defined");
                    }
                    else {
                        objectIndices[name] = int(objectOffsets.size());
                        objectOffsets.push_back(int(objects.size()));
                    }

                    int material;

                    if (materialIndices.contains(materialName)) material = materialIndices.at(materialName);
                    else throw error(input, lineNumber, "'material' is not defined");

                    if (type == "sphere") {
                        Vector center = parseVector(ss, input, lineNumber, "center");
                        Float radius = parseValue<Float>(ss, input, lineNumber, "radius");

                        if (radius <= 0) throw error(input, lineNumber, "'radius' must be positive, got " + std::to_string(radius));

                        objects.push_back(Object::makeSphere(material, center, radius));
                    }
                    else if (type == "quad" || type == "tri") {
                        Vector corner = parseVector(ss, input, lineNumber, "corner");
                        Vector horizontal = parseVector(ss, input, lineNumber, "horizontal");
                        Vector vertical = parseVector(ss, input, lineNumber, "vertical");

                        if (horizontal.length() < EPSILON) throw error(input, lineNumber, "'horizontal' must be non-zero");
                        if (vertical.length() < EPSILON) throw error(input, lineNumber, "'vertical' must be non-zero");
                        if (cross(horizontal, vertical).length() < EPSILON) throw error(input, lineNumber, "'horizontal' and 'vertical' must be non-parallel");

                        if (type == "quad") objects.push_back(Object::makeQuad(material, corner, horizontal, vertical));
                        else if (type == "tri") objects.push_back(Object::makeTri(material, corner, horizontal, vertical));
                    }
                    else if (type == "mesh") {
                        std::string objFile = parseValue<std::string>(ss, input, lineNumber, "file");

                        parseOBJ(objFile, objects, material);
                    }
                    else throw error(input, lineNumber, "'type' must be \"quad\", \"sphere\", or \"tri\"");
                }
                else if (command == "Instance") {
                    instanceCommandFound = true;

                    std::string objectName = parseValue<std::string>(ss, input, lineNumber, "object");
                    std::string materialName = parseValue<std::string>(ss, input, lineNumber, "material");

                    int objectIndex, object, count;

                    if (objectIndices.contains(objectName)) {
                        objectIndex = objectIndices.at(objectName);

                        object = objectOffsets[objectIndex];
                        count = objectIndex < int(objectOffsets.size()) - 1 ? objectOffsets[objectIndex + 1] - object : int(objects.size()) - object;
                    }
                    else throw error(input, lineNumber, "'object' is not defined");

                    int material = -1;

                    if (materialName != "default") {
                        if (materialIndices.contains(materialName)) material = materialIndices.at(materialName);
                        else throw error(input, lineNumber, "'material' is not defined");
                    }

                    Vector translation = parseVector(ss, input, lineNumber, "translation");
                    Vector rotation = parseVector(ss, input, lineNumber, "rotation");
                    Vector scale = parseVector(ss, input, lineNumber, "scale");

                    if (fabsF(scale[0]) < EPSILON || fabsF(scale[1]) < EPSILON || fabsF(scale[2]) < EPSILON) throw error(input, lineNumber, "'scale' must be non-zero");

                    Instance instance(object, count, material, translation, rotation, scale);

                    if (fabsF(scale[0] - scale[1]) > EPSILON || fabsF(scale[1] - scale[2]) > EPSILON)
                        for (int i = 0; i < count; i++)
                            if (materials[instance.getMaterial(objects.data(), i)].isEmissive()) throw error(input, lineNumber, "'scale' must be uniform for emissive objects");

                    instances.push_back(instance);
                }
                else if (command == "Background") {
                    backgroundCommandFound = true;

                    std::string type = parseValue<std::string>(ss, input, lineNumber, "type");

                    if (type == "equirectangular") background = Background::makeEquirectangular(parseSpectrumTexture(ss, input, lineNumber, "background", spectra, complexSpectra, scalarTextureIndices, spectrumTextures, spectrumTextureIndices));
                    else throw error(input, lineNumber, "'type' must be \"equirectangular\"");
                }
                else if (command == "Camera") {
                    cameraCommandFound = true;

                    Vector position = parseVector(ss, input, lineNumber, "position");
                    Vector corner = parseVector(ss, input, lineNumber, "corner");
                    Vector horizontal = parseVector(ss, input, lineNumber, "horizontal");
                    Vector vertical = parseVector(ss, input, lineNumber, "vertical");

                    if (horizontal.length() < EPSILON) throw error(input, lineNumber, "'horizontal' must be non-zero");
                    if (vertical.length() < EPSILON) throw error(input, lineNumber, "'vertical' must be non-zero");
                    if (cross(horizontal, vertical).length() < EPSILON) throw error(input, lineNumber, "'horizontal' and 'vertical' must be non-parallel");

                    renderer.setCamera(position, corner, horizontal, vertical);
                }
                else throw error(input, lineNumber, "expected \"Background\", \"Camera\", \"Instance\", \"Material\", \"Render\", \"Object\", or \"Texture\", got \"" + command + "\"");

                if (ss >> command) throw error(input, lineNumber, "unexpected token \"" + command + "\"");
            }

            if (!renderCommandFound) throw std::runtime_error(input + ": expected \"Render\"");
            if (!backgroundCommandFound) throw std::runtime_error(input + ": expected \"Background\"");
            if (!cameraCommandFound) throw std::runtime_error(input + ": expected \"Camera\"");
            if (!instanceCommandFound) throw std::runtime_error(input + ": expected \"Instance\"");

            objectOffsets.push_back(int(objects.size()));

            nodes = BVH::makeBVH(objects, instances, objectOffsets);

            Vector sceneCenter;
            Float sceneRadius = 0;

            for (int i = 0; i < int(instances.size()); i++)
                for (int j = 0; j < instances[i].getCount(); j++) {
                    int objectIndex = instances[i].getObject() + j;

                    Vector objectCenter = instances[i].center(objects.data(), j);
                    Float objectRadius = instances[i].radius(objects.data(), j);

                    Vector sceneToObject = objectCenter - sceneCenter;
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

                    int material = instances[i].getMaterial(objects.data(), j);

                    if (materials[material].isEmissive()) {
                        lightInstances.push_back(i);
                        lightObjects.push_back(objectIndex);

                        Float lightPower = instances[i].area(objects.data(), j) * materials[material].averageEmission(spectra.data(), scalarTextures.data(), spectrumTextures.data());

                        lightPowers.push_back(lightPower);
                        totalLightPower += lightPower;
                    }
                }

            lightInstances.push_back(-1);
            lightObjects.push_back(-1);

            Float backgroundLightPower = background.area(sceneRadius) * background.average(spectra.data(), scalarTextures.data(), spectrumTextures.data());

            lightPowers.push_back(backgroundLightPower);
            totalLightPower += backgroundLightPower;
        }

    private:
        static std::runtime_error error(const std::string & input, int lineNumber, const std::string & message) { return std::runtime_error(input + ":" + std::to_string(lineNumber) + ": " + message); }

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
                else if (c == ',') result += ' ';
                else result += c;
            }

            return result;
        }

        static Float stoF(const std::string & s, size_t * index = nullptr) { return Float((sizeof(Float) == sizeof(float)) ? std::stof(s, index) : std::stod(s, index)); }

        template <typename T>
        static T parseValue(std::stringstream & ss, const std::string & input, int lineNumber, const std::string & parameter) {
            T value;

            if (!(ss >> value)) throw error(input, lineNumber, "'" + parameter + "' is missing or invalid");

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
                real = stoF(token, &index);

                if (index < token.length()) {
                    token = token.substr(index);

                    if (token.back() == 'i') token.pop_back();
                    else throw error(input, lineNumber, message);

                    if (token == "+") imaginary = 1;
                    else if (token == "-") imaginary = -1;
                    else if (token.empty()) {
                        imaginary = real;
                        real = 0;
                    }
                    else imaginary = stoF(token);
                }
            }
            catch (const std::exception &) {
                throw error(input, lineNumber, message);
            }

            return Complex(real, imaginary);
        }

        static Vector parseVector(std::stringstream & ss, const std::string & input, int lineNumber, const std::string & parameter) {
            std::string message = "'" + parameter + "' is missing or invalid";

            std::string token;

            if (!(ss >> token) || token != "(") throw error(input, lineNumber, message);

            Vector vector;

            if (!(ss >> vector[0])) throw error(input, lineNumber, message);
            if (!(ss >> vector[1])) throw error(input, lineNumber, message);
            if (!(ss >> vector[2])) throw error(input, lineNumber, message);

            if (!(ss >> token) || token != ")") throw error(input, lineNumber, message);

            return vector;
        }

        template <typename T>
        static int parseSpectrum(std::stringstream & ss, const std::string & input, int lineNumber, const std::string & parameter, std::vector<DenseSpectrum<Float>> & spectra, std::vector<DenseSpectrum<Complex>> & complexSpectra) {
            std::string message = "'" + parameter + "' is missing or invalid";

            std::string token;

            if (!(ss >> token)) throw error(input, lineNumber, message);

            if (token != "(") {
                if constexpr (std::is_same_v<T, Float>) {
                    try {
                        spectra.push_back(DenseSpectrum<Float>(stoF(token)));

                        return int(spectra.size() - 1);
                    } catch (const std::exception &) {
                        throw error(input, lineNumber, message);
                    }
                }
                else if constexpr (std::is_same_v<T, Complex>) {
                    complexSpectra.push_back(DenseSpectrum<Complex>(parseComplex(token, input, lineNumber, message)));

                    return int(complexSpectra.size() - 1);
                }
            }

            std::map<double, T> samples;

            while (ss >> token) {
                if (token == ")") break;

                double lambda;

                try {
                    lambda = std::stod(token);
                } catch (const std::exception &) {
                    throw error(input, lineNumber, message);
                }

                if (lambda < CIE_LAMBDA_MIN || lambda > CIE_LAMBDA_MAX) throw error(input, lineNumber, message);

                if (!(ss >> token)) throw error(input, lineNumber, message);

                if constexpr (std::is_same_v<T, Float>) {
                    try {
                        samples[lambda] = stoF(token);
                    } catch (const std::exception &) {
                        throw error(input, lineNumber, message);
                    }
                }
                else if constexpr (std::is_same_v<T, Complex>) samples[lambda] = parseComplex(token, input, lineNumber, message);
                else throw error(input, lineNumber, message);
            }

            if (token != ")" || samples.empty()) throw error(input, lineNumber, message);

            DenseSpectrum<T> spectrum;

            typename std::map<double, T>::iterator iterator = samples.begin();

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
                spectra.push_back(spectrum);

                return int(spectra.size() - 1);
            }
            else if constexpr (std::is_same_v<T, Complex>) {
                complexSpectra.push_back(spectrum);

                return int(complexSpectra.size() - 1);
            }
            else throw error(input, lineNumber, message);
        }

        template <typename T>
        static std::vector<T> parseArray(std::stringstream & ss, const std::string & input, int lineNumber, const std::string & parameter) {
            std::string message = "'" + parameter + "' is missing or invalid";

            std::string token;

            if (!(ss >> token) || token != "[") throw error(input, lineNumber, message);

            std::vector<T> values;

            while (ss >> token) {
                if (token == "]") return values;

                if constexpr (std::is_same_v<T, Float>) {
                    try {
                        values.push_back(stoF(token));
                    } catch (const std::exception &) {
                        throw error(input, lineNumber, message);
                    }
                }
                else if constexpr (std::is_same_v<T, std::string>) values.push_back(token);
                else throw error(input, lineNumber, message);
            }

            throw error(input, lineNumber, message);
        }

        static std::vector<int> parseSpectrumArray(std::stringstream & ss, const std::string & input, int lineNumber, const std::string & parameter, std::vector<DenseSpectrum<Float>> & spectra, std::vector<DenseSpectrum<Complex>> & complexSpectra) {
            std::string message = "'" + parameter + "' is missing or invalid";

            std::string token;

            if (!(ss >> token) || token != "[") throw error(input, lineNumber, message);

            std::streampos pos = ss.tellg();

            std::vector<int> values;

            while (ss >> token) {
                if (token == "]") return values;

                ss.seekg(pos);

                values.push_back(parseSpectrum<Complex>(ss, input, lineNumber, parameter, spectra, complexSpectra));

                pos = ss.tellg();
            }

            throw error(input, lineNumber, message);
        }

        static int parseSpectrumTexture(std::stringstream & ss, const std::string & input, int lineNumber, const std::string & parameter, std::vector<DenseSpectrum<Float>> & spectra, std::vector<DenseSpectrum<Complex>> & complexSpectra, const std::map<std::string, int> & scalarTextureIndices, std::vector<SpectrumTexture> & spectrumTextures, std::map<std::string, int> & spectrumTextureIndices) {
            std::streampos pos = ss.tellg();

            try {
                spectrumTextures.push_back(SpectrumTexture::makeConstant(parseSpectrum<Float>(ss, input, lineNumber, parameter, spectra, complexSpectra)));

                return int(spectrumTextures.size() - 1);
            }
            catch (const std::exception &) {
                ss.seekg(pos);

                std::string name = parseValue<std::string>(ss, input, lineNumber, parameter);
                
                if (spectrumTextureIndices.contains(name)) return spectrumTextureIndices.at(name);
                else if (scalarTextureIndices.contains(name)) {
                    int scalarTextureIndex = scalarTextureIndices.at(name);

                    spectrumTextures.push_back(SpectrumTexture::makeScalar(scalarTextureIndex));

                    return int(spectrumTextures.size()) - 1;
                }
                else throw error(input, lineNumber, "'" + parameter + "' is not defined");
            }
        }

        static void parseOBJ(const std::string & input, std::vector<Object> & objects, int material) {
            std::ifstream inputFile(input);

            if (!inputFile.is_open()) {
                std::string error = std::strerror(errno);
                error[0] = char(std::tolower(error[0]));
                throw std::runtime_error(input + ": failed to open file (" + error + ")");
            }

            std::string line;
            int lineNumber = 0;
            bool fTagFound = false;

            std::vector<Vector> vertices;
            std::vector<Float> u;
            std::vector<Float> v;
            std::vector<Vector> normals;

            while (std::getline(inputFile, line)) {
                lineNumber++;

                if (line.empty()) continue;

                std::stringstream ss(line);

                std::string tag;
                if (!(ss >> tag)) continue;

                if (tag == "v") {
                    Vector vertex;

                    if (!(ss >> vertex[0])) throw error(input, lineNumber, "'x' is missing or invalid");
                    if (!(ss >> vertex[1])) throw error(input, lineNumber, "'y' is missing or invalid");
                    if (!(ss >> vertex[2])) throw error(input, lineNumber, "'z' is missing or invalid");

                    vertices.push_back(vertex);
                }
                else if (tag == "vt") {
                    Float uValue, vValue;

                    if (!(ss >> uValue)) throw error(input, lineNumber, "'u' is missing or invalid");
                    if (!(ss >> vValue)) throw error(input, lineNumber, "'v' is missing or invalid");

                    u.push_back(uValue);
                    v.push_back(vValue);
                }
                else if (tag == "vn") {
                    Vector normal;

                    if (!(ss >> normal[0])) throw error(input, lineNumber, "'x' is missing or invalid");
                    if (!(ss >> normal[1])) throw error(input, lineNumber, "'y' is missing or invalid");
                    if (!(ss >> normal[2])) throw error(input, lineNumber, "'z' is missing or invalid");

                    normals.push_back(normal);
                }
                else if (tag == "f") {
                    fTagFound = true;

                    std::string token;

                    std::vector<int> vertexIndices, textureIndices, normalIndices;

                    while (ss >> token) {
                        std::stringstream tokenStream(token);

                        std::string vertexIndexString, textureIndexString, normalIndexString;

                        if (!std::getline(tokenStream, vertexIndexString, '/')) throw error(input, lineNumber, "'v' is missing or invalid");
                        std::getline(tokenStream, textureIndexString, '/');
                        std::getline(tokenStream, normalIndexString, '/');

                        int vertexIndex, textureIndex, normalIndex;

                        try {
                            vertexIndex = std::stoi(vertexIndexString);
                        } catch (const std::exception &) {
                            throw error(input, lineNumber, "'v' is missing or invalid");
                        }

                        if (vertexIndex < 0 && int(vertices.size()) + vertexIndex >= 0) vertexIndex = int(vertices.size()) + vertexIndex;
                        else if (vertexIndex < 0) throw error(input, lineNumber, "'v' must be positive, got " + std::to_string(vertexIndex));
                        else if (vertexIndex > int(vertices.size())) throw error(input, lineNumber, "'v' must be at most " + std::to_string(int(vertices.size()) - 1) + ", got " + std::to_string(vertexIndex));
                        else vertexIndex--;

                        vertexIndices.push_back(vertexIndex);

                        if (!textureIndexString.empty()) {
                            try {
                                textureIndex = std::stoi(textureIndexString);
                            } catch (const std::exception &) {
                                throw error(input, lineNumber, "'vt' is missing or invalid");
                            }

                            if (textureIndex < 0 && int(u.size()) + textureIndex >= 0) textureIndex = int(u.size()) + textureIndex;
                            else if (textureIndex < 0) throw error(input, lineNumber, "'vt' must be positive, got " + std::to_string(textureIndex));
                            else if (textureIndex > int(u.size())) throw error(input, lineNumber, "'vt' must be at most " + std::to_string(int(u.size()) - 1) + ", got " + std::to_string(textureIndex));
                            else textureIndex--;
                        }
                        else textureIndex = -1;

                        textureIndices.push_back(textureIndex);

                        if (!normalIndexString.empty()) {
                            try {
                                normalIndex = std::stoi(normalIndexString);
                            } catch (const std::exception &) {
                                throw error(input, lineNumber, "'vn' is missing or invalid");
                            }

                            if (normalIndex < 0 && int(normals.size()) + normalIndex >= 0) normalIndex = int(normals.size()) + normalIndex;
                            else if (normalIndex < 0) throw error(input, lineNumber, "'vn' must be positive, got " + std::to_string(normalIndex));
                            else if (normalIndex > int(normals.size())) throw error(input, lineNumber, "'vn' must be at most " + std::to_string(int(normals.size()) - 1) + ", got " + std::to_string(normalIndex));
                            else normalIndex--;
                        }
                        else normalIndex = -1;

                        normalIndices.push_back(normalIndex);
                    }

                    if (vertexIndices.size() < 3) throw error(input, lineNumber, "'f' must have at least 3 vertices");

                    Vector vertex0 = vertices[vertexIndices[0]];
                    Vector normal0 = normalIndices[0] >= 0 ? normals[normalIndices[0]] : Vector(0, 0, 0);
                    Float u0 = textureIndices[0] >= 0 ? u[textureIndices[0]] : 0;
                    Float v0 = textureIndices[0] >= 0 ? v[textureIndices[0]] : 0;

                    for (int i = 1; i < int(vertexIndices.size()) - 1; i++) {
                        Vector vertex1 = vertices[vertexIndices[i]];
                        Vector normal1 = normalIndices[i] >= 0 ? normals[normalIndices[i]] : Vector(0, 0, 0);
                        Float u1 = textureIndices[i] >= 0 ? u[textureIndices[i]] : 1;
                        Float v1 = textureIndices[i] >= 0 ? v[textureIndices[i]] : 0;

                        Vector vertex2 = vertices[vertexIndices[i + 1]];
                        Vector normal2 = normalIndices[i + 1] >= 0 ? normals[normalIndices[i + 1]] : Vector(0, 0, 0);
                        Float u2 = textureIndices[i + 1] >= 0 ? u[textureIndices[i + 1]] : 0;
                        Float v2 = textureIndices[i + 1] >= 0 ? v[textureIndices[i + 1]] : 1;

                        objects.push_back(Object::makeTri(material, vertex0, vertex1 - vertex0, vertex2 - vertex0, normal0, normal1, normal2, u0, u1, u2, v0, v1, v2));
                    }
                }
                else continue;

                if (ss >> tag) throw error(input, lineNumber, "unexpected token \"" + tag + "\"");
            }
                

            if (!fTagFound) throw error(input, lineNumber, "expected \"f\"");
        }
};