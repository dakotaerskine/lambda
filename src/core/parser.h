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

#ifndef __CUDA_ARCH__
    class Parser {
        public:
            static bool hasValidExtension(const std::string & output) { return hasExtension(output, ".lrd"); }

            static void parseFile(const std::string & input, Renderer & renderer, std::vector<DenseSpectrum<Float>> & spectra, std::vector<DenseSpectrum<Complex>> & complexSpectra, Background & background, std::vector<Object> & objects, std::vector<BVHNode> & nodes, std::vector<int> & lights, std::vector<Float> & lightPowers, Float & totalLightPower, std::vector<Material> & materials, std::vector<int> & materialProperties, std::vector<ScalarTexture> & scalarTextures, std::vector<SpectrumTexture> & spectrumTextures) {
                std::ifstream inputFile(input);

                if (!inputFile.is_open()) {
                    std::string error = std::strerror(errno);
                    error[0] = char(std::tolower(error[0]));
                    throw std::runtime_error(": failed to open file (" + error + ")");
                }

                std::string line;
                int lineNumber = 0;
                bool renderCommandFound = false;
                bool backgroundCommandFound = false;
                bool cameraCommandFound = false;

                std::map<std::string, int> materialIndices;
                std::map<std::string, int> scalarTextureIndices;
                std::map<std::string, int> spectrumTextureIndices;

                Vector sceneCenter;
                Float sceneRadius = 0;

                while (std::getline(inputFile, line)) {
                    lineNumber++;

                    line = preprocess(line);
                    if (line.empty()) continue;

                    std::stringstream ss(line);

                    std::string command;
                    if (!(ss >> command)) continue;

                    if (!renderCommandFound && command != "Render") throw error(lineNumber, "expected \"Render\", got \"" + command + "\"");

                    if (command == "Render") {
                        renderCommandFound = true;

                        int width = parseValue<int>(ss, lineNumber, "width");
                        if (width <= 0) throw error(lineNumber, "'width' must be positive, got " + std::to_string(width));

                        int height = parseValue<int>(ss, lineNumber, "height");
                        if (height <= 0) throw error(lineNumber, "'height' must be positive, got " + std::to_string(height));

                        int depth = parseValue<int>(ss, lineNumber, "depth");
                        if (depth < 0) throw error(lineNumber, "'depth' must be non-negative, got " + std::to_string(depth));

                        int samples = parseValue<int>(ss, lineNumber, "samples");
                        if (samples < 1) throw error(lineNumber, "'samples' must be at least 1, got " + std::to_string(samples));

                        int sqrtSamples = int(std::sqrt(samples));
                        if (sqrtSamples * sqrtSamples != samples) throw error(lineNumber, "'samples' must be a perfect square, got " + std::to_string(samples));

                        Float lambdaMin = parseValue<Float>(ss, lineNumber, "lambdaMin");
                        if (lambdaMin < 0) throw error(lineNumber, "'lambdaMin' must be non-negative, got " + std::to_string(lambdaMin));
                        if (lambdaMin < CIE_LAMBDA_MIN) throw error(lineNumber, "'lambdaMin' must be at least " + std::to_string(CIE_LAMBDA_MIN) + ", got " + std::to_string(lambdaMin));

                        Float lambdaMax = parseValue<Float>(ss, lineNumber, "lambdaMax");
                        if (lambdaMax < 0) throw error(lineNumber, "'lambdaMax' must be non-negative, got " + std::to_string(lambdaMax));
                        if (lambdaMax > CIE_LAMBDA_MAX) throw error(lineNumber, "'lambdaMax' must be at most " + std::to_string(CIE_LAMBDA_MAX) + ", got " + std::to_string(lambdaMax));

                        if (lambdaMin >= lambdaMax) throw error(lineNumber, "'lambdaMin' must be at most 'lambdaMax', got " + std::to_string(lambdaMin) + " and " + std::to_string(lambdaMax));

                        renderer = Renderer(width, height, depth, samples, sqrtSamples, lambdaMin, lambdaMax);
                    }
                    else if (command == "Texture") {
                        std::string name = parseValue<std::string>(ss, lineNumber, "name");
                        std::string type = parseValue<std::string>(ss, lineNumber, "type");
                        std::string subtype = parseValue<std::string>(ss, lineNumber, "subtype");

                        if (scalarTextureIndices.find(name) != scalarTextureIndices.end() || spectrumTextureIndices.find(name) != spectrumTextureIndices.end()) throw error(lineNumber, "'name' is already defined");

                        if (type == "scalar") {
                            if (subtype == "constant") scalarTextures.push_back(ScalarTexture::makeConstant(parseValue<Float>(ss, lineNumber, "value")));
                            else if (subtype == "perlin" || subtype == "worley") {
                                Float min = parseValue<Float>(ss, lineNumber, "min");
                                Float max = parseValue<Float>(ss, lineNumber, "max");
                                Float frequency = parseValue<Float>(ss, lineNumber, "frequency");

                                if (min > max) throw error(lineNumber, "'min' must be at most 'max', got " + std::to_string(min) + " and " + std::to_string(max));

                                if (subtype == "perlin") scalarTextures.push_back(ScalarTexture::makePerlin(min, max, frequency));
                                else if (subtype == "worley") scalarTextures.push_back(ScalarTexture::makeWorley(min, max, frequency));
                            }
                            else throw error(lineNumber, "'subtype' must be \"constant\", \"perlin\", or \"worley\"");

                            scalarTextureIndices[name] = int(scalarTextures.size() - 1);
                        }
                        else if (type == "spectrum") {
                            if (subtype == "constant") spectrumTextures.push_back(SpectrumTexture::makeConstant(parseSpectrum<Float>(ss, lineNumber, "spectrum", spectra, complexSpectra)));
                            else if (subtype == "checker") {
                                int value1 = parseSpectrum<Float>(ss, lineNumber, "value1", spectra, complexSpectra);
                                int value2 = parseSpectrum<Float>(ss, lineNumber, "value2", spectra, complexSpectra);

                                Float scale = parseValue<Float>(ss, lineNumber, "scale");

                                spectrumTextures.push_back(SpectrumTexture::makeChecker(value1, value2, scale));
                            }
                            else throw error(lineNumber, "'subtype' must be \"constant\" or \"checker\"");

                            spectrumTextureIndices[name] = int(spectrumTextures.size() - 1);
                        }
                        else throw error(lineNumber, "'type' must be \"scalar\" or \"spectrum\"");
                    }
                    else if (command == "Material") {
                        std::string name = parseValue<std::string>(ss, lineNumber, "name");
                        std::string type = parseValue<std::string>(ss, lineNumber, "type");

                        if (materialIndices.find(name) != materialIndices.end()) throw error(lineNumber, "'name' is already defined");

                        if (type == "lambertian") materials.push_back(Material::makeLambertian(parseSpectrumTexture(ss, lineNumber, "albedo", spectra, complexSpectra, spectrumTextures, spectrumTextureIndices)));
                        else if (type == "mirror") materials.push_back(Material::makeMirror(parseSpectrumTexture(ss, lineNumber, "albedo", spectra, complexSpectra, spectrumTextures, spectrumTextureIndices)));
                        else if (type == "dielectric") {
                            int n0 = parseSpectrum<Float>(ss, lineNumber, "n0", spectra, complexSpectra);
                            int n1 = parseSpectrum<Float>(ss, lineNumber, "n1", spectra, complexSpectra);

                            for (int i = 0; i < CIE_LAMBDA_BINS; i++) {
                                if (spectra[n0][i] <= 0) throw error(lineNumber, "'n0' must be positive, got " + std::to_string(spectra[n0][i]));
                                if (spectra[n1][i] <= 0) throw error(lineNumber, "'n1' must be positive, got " + std::to_string(spectra[n1][i]));
                            }

                            materials.push_back(Material::makeDielectric(n0, n1));
                        }
                        else if (type == "emissive") materials.push_back(Material::makeEmissive(parseSpectrumTexture(ss, lineNumber, "emission", spectra, complexSpectra, spectrumTextures, spectrumTextureIndices)));
                        else if (type == "thinfilm") {
                            std::vector<int> n = parseSpectrumArray(ss, lineNumber, "n", spectra, complexSpectra);

                            if (n.size() < 2) throw error(lineNumber, "'n' must have at least 2 entries, got " + std::to_string(n.size()));

                            int nOffset = int(materialProperties.size());

                            materialProperties.insert(materialProperties.end(), n.begin(), n.end());

                            int numLayers = int(n.size() - 2);

                            std::vector<std::string> dNames = parseArray<std::string>(ss, lineNumber, "d");
                            if ((int)dNames.size() != numLayers) throw error(lineNumber, "'d' must have " + std::to_string(numLayers) + " entries, got " + std::to_string(dNames.size()));

                            int dOffset = int(materialProperties.size());

                            for (int i = 0; i < numLayers; i++) {
                                try {
                                    scalarTextures.push_back(ScalarTexture::makeConstant(stoF(dNames[i])));

                                    materialProperties.push_back(int(scalarTextures.size() - 1));
                                }
                                catch (const std::exception &) {
                                    try {
                                        materialProperties.push_back(scalarTextureIndices.at(dNames[i]));
                                    } catch (const std::out_of_range &) {
                                        throw error(lineNumber, "'d' is not defined");
                                    }
                                }
                            }

                            materials.push_back(Material::makeThinFilm(numLayers, nOffset, dOffset));
                        }
                        else throw error(lineNumber, "'type' must be \"dielectric\", \"emissive\", \"lambertian\", \"mirror\", or \"thinfilm\"");

                        materialIndices[name] = int(materials.size() - 1);
                    }
                    else if (command == "Object") {
                        std::string type = parseValue<std::string>(ss, lineNumber, "type");
                        std::string materialName = parseValue<std::string>(ss, lineNumber, "material");

                        int material;

                        try {
                            material = materialIndices.at(materialName);
                        } catch (const std::out_of_range &) {
                            throw error(lineNumber, "'material' is not defined");
                        }

                        if (type == "sphere") {
                            Vector center = parseVector(ss, lineNumber, "center");
                            Float radius = parseValue<Float>(ss, lineNumber, "radius");

                            if (radius <= 0) throw error(lineNumber, "'radius' must be positive, got " + std::to_string(radius));

                            objects.push_back(Object::makeSphere(material, center, radius));
                        }
                        else if (type == "quad") {
                            Vector corner = parseVector(ss, lineNumber, "corner");
                            Vector horizontal = parseVector(ss, lineNumber, "horizontal");
                            Vector vertical = parseVector(ss, lineNumber, "vertical");

                            if (horizontal.length() < EPSILON) throw error(lineNumber, "'horizontal' must be non-zero, got " + std::to_string(horizontal.length()));
                            if (vertical.length() < EPSILON) throw error(lineNumber, "'vertical' must be non-zero, got " + std::to_string(vertical.length()));
                            if (cross(horizontal, vertical).length() < EPSILON) throw error(lineNumber, "'horizontal' and 'vertical' must be non-parallel");

                            objects.push_back(Object::makeQuad(material, corner, horizontal, vertical));
                        }
                        else throw error(lineNumber, "'type' must be \"quad\" or \"sphere\"");

                        int objectIndex = int(objects.size()) - 1;

                        Vector objectCenter = objects[objectIndex].center();
                        Float objectRadius = objects[objectIndex].radius();

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
                    }
                    else if (command == "Background") {
                        backgroundCommandFound = true;

                        std::string type = parseValue<std::string>(ss, lineNumber, "type");

                        if (type == "equirectangular") background = Background::makeEquirectangular(parseSpectrumTexture(ss, lineNumber, "background", spectra, complexSpectra, spectrumTextures, spectrumTextureIndices));
                        else throw error(lineNumber, "'type' must be \"equirectangular\"");
                    }
                    else if (command == "Camera") {
                        cameraCommandFound = true;

                        Vector position = parseVector(ss, lineNumber, "position");
                        Vector corner = parseVector(ss, lineNumber, "corner");
                        Vector horizontal = parseVector(ss, lineNumber, "horizontal");
                        Vector vertical = parseVector(ss, lineNumber, "vertical");

                        if (horizontal.length() < EPSILON) throw error(lineNumber, "'horizontal' must be non-zero, got " + std::to_string(horizontal.length()));
                        if (vertical.length() < EPSILON) throw error(lineNumber, "'vertical' must be non-zero, got " + std::to_string(vertical.length()));
                        if (cross(horizontal, vertical).length() < EPSILON) throw error(lineNumber, "'horizontal' and 'vertical' must be non-parallel");

                        renderer.setCamera(position, corner, horizontal, vertical);
                    }
                    else throw error(lineNumber, "expected \"Background\", \"Camera\", \"Material\", \"Render\", \"Object\", or \"Texture\", got \"" + command + "\"");
                }

                if (!renderCommandFound) throw std::runtime_error(": expected \"Render\"");
                if (!backgroundCommandFound) throw std::runtime_error(": expected \"Background\"");
                if (!cameraCommandFound) throw std::runtime_error(": expected \"Camera\"");

                nodes = BVH::makeBVH(objects);

                for (int i = 0; i < int(objects.size()); i++) {
                    int material = objects[i].getMaterial();

                    if (materials[material].isEmissive()) {
                        lights.push_back(i);

                        Float lightPower = objects[i].area() * materials[material].averageEmission(spectra.data(), spectrumTextures.data());

                        lightPowers.push_back(lightPower);
                        totalLightPower += lightPower;
                    }
                }

                lights.push_back(-1);

                Float backgroundLightPower = background.area(sceneRadius) * background.average(spectra.data(), spectrumTextures.data());

                lightPowers.push_back(backgroundLightPower);
                totalLightPower += backgroundLightPower;
            }

        private:
            static std::runtime_error error(int lineNumber, const std::string & message) { return std::runtime_error(":" + std::to_string(lineNumber) + ": " + message); }

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

            static Float stoF(const std::string & s, size_t * index = nullptr) { return Float((sizeof(Float) == sizeof(float)) ? std::stof(s, index) : std::stod(s, index)); }

            template <typename T>
            static T parseValue(std::stringstream & ss, int lineNumber, const std::string & parameter) {
                T value;

                if (!(ss >> value)) throw error(lineNumber, "'" + parameter + "' is missing or invalid");

                return value;
            }

            static Complex parseComplex(std::string & token, int lineNumber, const std::string & parameter) {
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
                        else throw error(lineNumber, message);

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
                    throw error(lineNumber, message);
                }

                return Complex(real, imaginary);
            }

            static Vector parseVector(std::stringstream & ss, int lineNumber, const std::string & parameter) {
                std::string message = "'" + parameter + "' is missing or invalid";

                std::string token;

                if (!(ss >> token) || token != "(") throw error(lineNumber, message);

                Vector vector;

                if (!(ss >> vector[0])) throw error(lineNumber, message);
                if (!(ss >> vector[1])) throw error(lineNumber, message);
                if (!(ss >> vector[2])) throw error(lineNumber, message);

                if (!(ss >> token) || token != ")") throw error(lineNumber, message);

                return vector;
            }

            template <typename T>
            static int parseSpectrum(std::stringstream & ss, int lineNumber, const std::string & parameter, std::vector<DenseSpectrum<Float>> & spectra, std::vector<DenseSpectrum<Complex>> & complexSpectra) {
                std::string message = "'" + parameter + "' is missing or invalid";

                std::string token;

                if (!(ss >> token)) throw error(lineNumber, message);

                if (token != "(") {
                    if constexpr (std::is_same_v<T, Float>) {
                        try {
                            spectra.push_back(DenseSpectrum<Float>(stoF(token)));

                            return int(spectra.size() - 1);
                        } catch (const std::exception &) {
                            throw error(lineNumber, message);
                        }
                    }
                    else if constexpr (std::is_same_v<T, Complex>) {
                        complexSpectra.push_back(DenseSpectrum<Complex>(parseComplex(token, lineNumber, message)));

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
                        throw error(lineNumber, message);
                    }

                    if (lambda < CIE_LAMBDA_MIN || lambda > CIE_LAMBDA_MAX) throw error(lineNumber, message);

                    if (!(ss >> token)) throw error(lineNumber, message);

                    if constexpr (std::is_same_v<T, Float>) {
                        try {
                            samples[lambda] = stoF(token);
                        } catch (const std::exception &) {
                            throw error(lineNumber, message);
                        }
                    }
                    else if constexpr (std::is_same_v<T, Complex>) samples[lambda] = parseComplex(token, lineNumber, message);
                    else throw error(lineNumber, message);
                }

                if (token != ")" || samples.empty()) throw error(lineNumber, message);

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
                else throw error(lineNumber, message);
            }

            template <typename T>
            static std::vector<T> parseArray(std::stringstream & ss, int lineNumber, const std::string & parameter) {
                std::string message = "'" + parameter + "' is missing or invalid";

                std::string token;

                if (!(ss >> token) || token != "[") throw error(lineNumber, parameter);

                std::streampos pos = ss.tellg();

                std::vector<T> values;

                while (ss >> token) {
                    if (token == "]") return values;

                    if constexpr (std::is_same_v<T, Float>) {
                        try {
                            values.push_back(stoF(token));
                        } catch (const std::exception &) {
                            throw error(lineNumber, message);
                        }
                    }
                    else if constexpr (std::is_same_v<T, std::string>) values.push_back(token);
                    else throw error(lineNumber, message);

                    pos = ss.tellg();
                }

                throw error(lineNumber, message);
            }

            static std::vector<int> parseSpectrumArray(std::stringstream & ss, int lineNumber, const std::string & parameter, std::vector<DenseSpectrum<Float>> & spectra, std::vector<DenseSpectrum<Complex>> & complexSpectra) {
                std::string message = "'" + parameter + "' is missing or invalid";

                std::string token;

                if (!(ss >> token) || token != "[") throw error(lineNumber, parameter);

                std::streampos pos = ss.tellg();

                std::vector<int> values;

                while (ss >> token) {
                    if (token == "]") return values;

                    ss.seekg(pos);

                    values.push_back(parseSpectrum<Complex>(ss, lineNumber, parameter, spectra, complexSpectra));

                    pos = ss.tellg();
                }

                throw error(lineNumber, message);
            }

            static int parseSpectrumTexture(std::stringstream & ss, int lineNumber, const std::string & parameter, std::vector<DenseSpectrum<Float>> & spectra, std::vector<DenseSpectrum<Complex>> & complexSpectra, std::vector<SpectrumTexture> & spectrumTextures, const std::map<std::string, int> & spectrumTextureIndices) {
                std::streampos pos = ss.tellg();

                try {
                    spectrumTextures.push_back(SpectrumTexture::makeConstant(parseSpectrum<Float>(ss, lineNumber, parameter, spectra, complexSpectra)));

                    return int(spectrumTextures.size() - 1);
                }
                catch (const std::exception &) {
                    ss.seekg(pos);

                    try {
                        return spectrumTextureIndices.at(parseValue<std::string>(ss, lineNumber, parameter));
                    } catch (const std::out_of_range &) {
                        throw error(lineNumber, "'" + parameter + "' is not defined");
                    }
                }
            }
    };
#endif