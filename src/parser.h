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

#include "camera.h"
#include "config.h"
#include "constants.h"
#include "material.h"
#include "object.h"
#include "platform.h"
#include "spectrum.h"
#include "texture.h"
#include "vector.h"

class Parser {
    public:
        static void parseFile(const std::string & input, Config & config, Camera & camera, Spectrum & background, std::vector<Object> & objects) {
            std::ifstream inputFile(input);

            if (!inputFile.is_open()) {
                std::string error = std::strerror(errno);
                error[0] = std::tolower(error[0]);
                throw std::runtime_error(": failed to open file (" + error + ")");
            }

            std::string line;
            int lineNumber = 0;
            bool renderCommandFound = false;
            bool backgroundCommandFound = false;
            bool cameraCommandFound = false;

            std::map<std::string, Material> materials;
            std::map<std::string, ScalarTexture> scalarTextures;
            std::map<std::string, SpectrumTexture> spectrumTextures;

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

                    if (!(ss >> config.width)) throw error(lineNumber, "'width' is missing or invalid");
                    if (config.width <= 0) throw error(lineNumber, "'width' must be positive, got " + std::to_string(config.width));

                    if (!(ss >> config.height)) throw error(lineNumber, "'height' is missing or invalid");
                    if (config.height <= 0) throw error(lineNumber, "'height' must be positive, got " + std::to_string(config.height));

                    if (!(ss >> config.depth)) throw error(lineNumber, "'depth' is missing or invalid");
                    if (config.depth < 0) throw error(lineNumber, "'depth' must be non-negative, got " + std::to_string(config.depth));

                    if (!(ss >> config.samples)) throw error(lineNumber, "'samples' is missing or invalid");
                    if (config.samples < 1) throw error(lineNumber, "'samples' must be at least 1, got " + std::to_string(config.samples));

                    if (!(ss >> config.wavelengthSamples)) throw error(lineNumber, "'wavelengthSamples' is missing or invalid");
                    if (config.wavelengthSamples < 2) throw error(lineNumber, "'wavelengthSamples' must be at least 2, got " + std::to_string(config.wavelengthSamples));
                    if (config.wavelengthSamples > MAX_WAVELENGTH_SAMPLES) throw error(lineNumber, "'wavelengthSamples' must be at most " + std::to_string(MAX_WAVELENGTH_SAMPLES) + ", got " + std::to_string(config.wavelengthSamples));

                    if (!(ss >> config.lambdaMin)) throw error(lineNumber, "'lambdaMin' is missing or invalid");
                    if (config.lambdaMin < 0) throw error(lineNumber, "'lambdaMin' must be non-negative, got " + std::to_string(config.lambdaMin));
                    if (config.lambdaMin < CIE_LAMBDA_MIN) throw error(lineNumber, "'lambdaMin' must be at least " + std::to_string(CIE_LAMBDA_MIN) + ", got " + std::to_string(config.lambdaMin));

                    if (!(ss >> config.lambdaMax)) throw error(lineNumber, "'lambdaMax' is missing or invalid");
                    if (config.lambdaMax < 0) throw error(lineNumber, "'lambdaMax' must be non-negative, got " + std::to_string(config.lambdaMax));
                    if (config.lambdaMax > CIE_LAMBDA_MAX) throw error(lineNumber, "'lambdaMax' must be at most " + std::to_string(CIE_LAMBDA_MAX) + ", got " + std::to_string(config.lambdaMax));

                    if (config.lambdaMin >= config.lambdaMax) throw error(lineNumber, "'lambdaMin' must be at most 'lambdaMax', got " + std::to_string(config.lambdaMin) + " and " + std::to_string(config.lambdaMax));

                    config.totalPixels = config.width * config.height;

                    config.sqrtSamples = int(std::sqrt(config.samples));

                    if (config.sqrtSamples * config.sqrtSamples != config.samples) throw error(lineNumber, "'samples' must be a perfect square, got " + std::to_string(config.samples));

                    config.lambdaStep = Float(config.lambdaMax - config.lambdaMin) / Float(config.wavelengthSamples - 1);
                }
                else if (command == "Background") {
                    backgroundCommandFound = true;

                    background = parseSpectrum(ss, config.wavelengthSamples, lineNumber, "'background' is missing or invalid");
                }
                else if (command == "Camera") {
                    cameraCommandFound = true;

                    Vector position = parseVector(ss, lineNumber, "'position' is missing or invalid");
                    Vector corner = parseVector(ss, lineNumber, "'corner' is missing or invalid");
                    Vector horizontal = parseVector(ss, lineNumber, "'horizontal' is missing or invalid");
                    Vector vertical = parseVector(ss, lineNumber, "'vertical' is missing or invalid");

                    camera = Camera(position, corner, horizontal, vertical);
                }
                else if (command == "Texture") {
                    std::string name, type, subtype;

                    if (!(ss >> name)) throw error(lineNumber, "'name' is missing or invalid");
                    if (!(ss >> type)) throw error(lineNumber, "'type' is missing or invalid");
                    if (!(ss >> subtype)) throw error(lineNumber, "'subtype' is missing or invalid");

                    if (scalarTextures.find(name) != scalarTextures.end() || spectrumTextures.find(name) != spectrumTextures.end()) throw error(lineNumber, "'name' is already defined");

                    if (type == "scalar") {
                        if (subtype == "constant") {
                            Float value;

                            if (!(ss >> value)) throw error(lineNumber, "'value' is missing or invalid");

                            scalarTextures[name] = ScalarTexture::makeConstant(value);
                        }
                        else if (subtype == "perlin") {
                            Float min, max, frequency;

                            if (!(ss >> min)) throw error(lineNumber, "'min' is missing or invalid");
                            if (!(ss >> max)) throw error(lineNumber, "'max' is missing or invalid");
                            if (!(ss >> frequency)) throw error(lineNumber, "'frequency' is missing or invalid");

                            if (min > max) throw error(lineNumber, "'min' must be at most 'max', got " + std::to_string(min) + " and " + std::to_string(max));

                            scalarTextures[name] = ScalarTexture::makePerlin(min, max, frequency);
                        }
                        else if (subtype == "worley") {
                            Float min, max, frequency;

                            if (!(ss >> min)) throw error(lineNumber, "'min' is missing or invalid");
                            if (!(ss >> max)) throw error(lineNumber, "'max' is missing or invalid");
                            if (!(ss >> frequency)) throw error(lineNumber, "'frequency' is missing or invalid");

                            if (min > max) throw error(lineNumber, "'min' must be at most 'max', got " + std::to_string(min) + " and " + std::to_string(max));

                            scalarTextures[name] = ScalarTexture::makeWorley(min, max, frequency);
                        }
                        else throw error(lineNumber, "'subtype' must be \"constant\", \"perlin\", or \"worley\"");
                    }
                    else if (type == "spectrum") {
                        if (subtype == "constant") spectrumTextures[name] = SpectrumTexture::makeConstant(parseSpectrum(ss, config.wavelengthSamples, lineNumber, "'spectrum' is missing or invalid"));
                        else if (subtype == "checker") {
                            Spectrum value1 = parseSpectrum(ss, config.wavelengthSamples, lineNumber, "'value1' is missing or invalid");
                            Spectrum value2 = parseSpectrum(ss, config.wavelengthSamples, lineNumber, "'value2' is missing or invalid");

                            Float scale;

                            if (!(ss >> scale)) throw error(lineNumber, "'scale' is missing or invalid");

                            spectrumTextures[name] = SpectrumTexture::makeChecker(value1, value2, scale);
                        }
                        else throw error(lineNumber, "'subtype' must be \"constant\" or \"checker\"");
                    }
                    else throw error(lineNumber, "'type' must be \"scalar\" or \"spectrum\"");
                }
                else if (command == "Material") {
                    std::string name, type;

                    if (!(ss >> name)) throw error(lineNumber, "'name' is missing or invalid");
                    if (!(ss >> type)) throw error(lineNumber, "'type' is missing or invalid");

                    if (materials.find(name) != materials.end()) throw error(lineNumber, "'name' is already defined");

                    if (type == "lambertian") {
                        std::string albedoName;

                        if (!(ss >> albedoName)) throw error(lineNumber, "'albedo' is missing or invalid");

                        SpectrumTexture albedo;

                        try {
                            albedo = spectrumTextures.at(albedoName);
                        } catch (const std::out_of_range &) {
                            throw error(lineNumber, "'albedo' is not defined");
                        }

                        materials[name] = Material::makeLambertian(albedo);
                    }
                    else if (type == "metal") {
                        std::string albedoName;

                        if (!(ss >> albedoName)) throw error(lineNumber, "'albedo' is missing or invalid");

                        SpectrumTexture albedo;

                        try {
                            albedo = spectrumTextures.at(albedoName);
                        } catch (const std::out_of_range &) {
                            throw error(lineNumber, "'albedo' is not defined");
                        }

                        materials[name] = Material::makeMetal(albedo);
                    }
                    else if (type == "dielectric") {
                        Float n0, n1;

                        if (!(ss >> n0)) throw error(lineNumber, "'n0' is missing or invalid");
                        if (n0 <= 0) throw error(lineNumber, "'n0' must be positive, got " + std::to_string(n0));

                        if (!(ss >> n1)) throw error(lineNumber, "'n1' is missing or invalid");
                        if (n1 <= 0) throw error(lineNumber, "'n1' must be positive, got " + std::to_string(n1));

                        materials[name] = Material::makeDielectric(n0, n1);
                    }
                    else if (type == "emissive") {
                        std::string emissionName;

                        if (!(ss >> emissionName)) throw error(lineNumber, "'emission' is missing or invalid");

                        SpectrumTexture emission;

                        try {
                            emission = spectrumTextures.at(emissionName);
                        } catch (const std::out_of_range &) {
                            throw error(lineNumber, "'emission' is not defined");
                        }

                        materials[name] = Material::makeEmissive(emission);
                    }
                    else if (type == "thinfilm") {
                        std::vector<Float> n = parseArray(ss, lineNumber, "'n' is missing or invalid");

                        if (n.size() < 2) throw error(lineNumber, "'n' must have at least 2 entries, got " + std::to_string(n.size()));

                        int numLayers = n.size() - 2;

                        if (numLayers > MAX_THIN_FILM_LAYERS) throw error(lineNumber, "'n' must have at most " + std::to_string(MAX_THIN_FILM_LAYERS + 2) + " entries, got " + std::to_string(n.size()));

                        std::vector<std::string> dNames = parseStringArray(ss, lineNumber, "'d' is missing or invalid");
                        if ((int)dNames.size() != numLayers) throw error(lineNumber, "'d' must have " + std::to_string(numLayers) + " entries, got " + std::to_string(dNames.size()));

                        std::vector<ScalarTexture> d(numLayers);

                        for (int i = 0; i < numLayers; i++)
                            try {
                                d[i] = scalarTextures.at(dNames[i]);
                            } catch (const std::out_of_range &) {
                                throw error(lineNumber, "'d' is not defined");
                            }

                        materials[name] = Material::makeThinFilm(numLayers, n.data(), d.data());
                    }
                    else throw error(lineNumber, "'type' must be \"dielectric\", \"emissive\", \"lambertian\", \"metal\", or \"thinfilm\"");
                }
                else if (command == "Object") {
                    std::string type, materialName;
                    if (!(ss >> type)) throw error(lineNumber, "'type' is missing or invalid");
                    if (!(ss >> materialName)) throw error(lineNumber, "'material' is missing or invalid");

                    Material material;

                    try {
                        material = materials.at(materialName);
                    } catch (const std::out_of_range &) {
                        throw error(lineNumber, "'material' is not defined");
                    }

                    if (type == "sphere") {
                        Vector center = parseVector(ss, lineNumber, "'center' is missing or invalid");

                        Float radius;

                        if (!(ss >> radius)) throw error(lineNumber, "'radius' is missing or invalid");
                        if (radius <= 0) throw error(lineNumber, "'radius' must be positive, got " + std::to_string(radius));

                        objects.push_back(Object::makeSphere(material, center, radius));
                    }
                    else if (type == "plane") {
                        Vector point = parseVector(ss, lineNumber, "'point' is missing or invalid");
                        Vector normal = parseVector(ss, lineNumber, "'normal' is missing or invalid");

                        if (normal.length() < EPSILON) throw error(lineNumber, "'normal' must be non-zero, got " + std::to_string(normal.length()));

                        objects.push_back(Object::makePlane(material, point, normal));
                    }
                    else throw error(lineNumber, "'type' must be \"plane\" or \"sphere\"");
                }
                else throw error(lineNumber, "expected \"Background\", \"Camera\", \"Material\", \"Render\", \"Object\", or \"Texture\", got \"" + command + "\"");
            }

            if (!renderCommandFound) throw std::runtime_error(": expected \"Render\"");
            if (!backgroundCommandFound) throw std::runtime_error(": expected \"Background\"");
            if (!cameraCommandFound) throw std::runtime_error(": expected \"Camera\"");
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

        static std::vector<Float> parseArray(std::stringstream & ss, int lineNumber, const std::string & message) {
            std::string token;

            if (!(ss >> token) || token != "[") throw error(lineNumber, message);

            std::vector<Float> values;

            while (ss >> token) {
                if (token == "]") return values;

                try {
                    values.push_back(std::stod(token));
                } catch (const std::exception &) {
                    throw error(lineNumber, message);
                }
            }

            throw error(lineNumber, message);
        }

        static std::vector<std::string> parseStringArray(std::stringstream & ss, int lineNumber, const std::string & message) {
            std::string token;

            if (!(ss >> token) || token != "[") throw error(lineNumber, message);

            std::vector<std::string> values;

            while (ss >> token) {
                if (token == "]") return values;

                values.push_back(token);
            }

            throw error(lineNumber, message);
        }

        static Vector parseVector(std::stringstream & ss, int lineNumber, const std::string & message) {
            std::string token;

            if (!(ss >> token) || token != "(") throw error(lineNumber, message);

            Vector vector;

            if (!(ss >> vector[0])) throw error(lineNumber, message);
            if (!(ss >> vector[1])) throw error(lineNumber, message);
            if (!(ss >> vector[2])) throw error(lineNumber, message);

            if (!(ss >> token) || token != ")") throw error(lineNumber, message);

            return vector;
        }

        static Spectrum parseSpectrum(std::stringstream & ss, int n, int lineNumber, const std::string & message) {
            std::string token;

            if (!(ss >> token) || token != "(") throw error(lineNumber, message);

            Spectrum spectrum(n);
            Float value;

            for (int i = 0; i < n; i++) {
                if (!(ss >> value)) throw error(lineNumber, message);

                spectrum[i] = value;
            }

            if (!(ss >> token) || token != ")") throw error(lineNumber, message);

            return spectrum;
        }
};