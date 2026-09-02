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

class ParserStream {
    public:
        ParserStream(const std::string & _input) : input(_input), lineNumber(0) {}

        void load(const std::string & line) {
            ss.clear();
            ss.str(line);

            lineNumber++;
            columnNumber = 1;
        }

        void setSkipWhitespace(bool skip) {
            if (skip) ss >> std::skipws;
            else ss >> std::noskipws;
        }

        template <typename T>
        T next(const std::string & parameter = "", bool useCustom = true) {
            T value;

            if (!attemptNext<T>(value, true, useCustom)) raiseError("'" + parameter + "' is missing or invalid");

            return value;
        }

        template <typename T>
        T peek(bool useCustom = true) {
            T value = T();

            attemptNext<T>(value, false, useCustom);

            return value;
        }

        template <typename T>
        bool hasNext(bool useCustom = true) {
            T value;

            return attemptNext<T>(value, false, useCustom);
        }

        template <typename T>
        bool hasNext(const T & expected, bool useCustom = true) {
            T value;

            std::streampos current = ss.tellg();

            bool success = attemptNext<T>(value, true, useCustom);

            if (success && value != expected) {
                ss.clear();
                ss.seekg(current);

                success = false;
            }

            return success;
        }

        [[noreturn]] void raiseError(const std::string & message) const { fileError(input, lineNumber, columnNumber, message); }

    private:
        std::stringstream ss;
        std::string input;
        int lineNumber;
        int columnNumber;

        template <typename T>
        bool tryNext(T & value, bool useCustom = true) {
            bool success = true;

            if constexpr (std::is_same_v<T, std::string>) {
                if (!useCustom) success = bool(ss >> value);
                else success = tryNextString(value);
            }
            else if constexpr (std::is_same_v<T, Complex>) success = tryNextComplex(value);
            else if constexpr (std::is_same_v<T, Vector<Float>>) success = tryNextVector(value);
            else if constexpr (std::is_same_v<T, std::map<double, Float>>) success = tryNextSpectrum<Float>(value);
            else if constexpr (std::is_same_v<T, std::map<double, Complex>>) success = tryNextSpectrum<Complex>(value);
            else success = bool(ss >> value);

            return success;
        }

        template <typename T>
        bool attemptNext(T & value, bool consumeOnSuccess, bool useCustom = true) {
            if (ss.flags() & std::ios_base::skipws) ss >> std::ws;

            std::streampos current = ss.tellg();

            bool success = tryNext<T>(value, useCustom);

            if (!success || !consumeOnSuccess) {
                ss.clear();
                ss.seekg(current);
            }

            columnNumber = std::max(int(current) + 1, 1);

            return success;
        }

        bool tryNextString(std::string & value) {
            setSkipWhitespace(false);

            while (true) {
                int i = ss.peek();

                if (i == EOF || std::isspace(i) || i == '(' || i == ')' || i == '[' || i == ']') break;

                char c;

                if (!tryNext<char>(c)) break;

                value += c;
            }

            setSkipWhitespace(true);

            return !value.empty();
        }

        bool tryNextComplex(Complex & value) {
            if (hasNext<char>('i')) value = Complex(0, 1);
            else {
                double real = 0, imaginary = 0;

                if (hasNext<double>() && !tryNext<double>(real)) return false;

                setSkipWhitespace(false);

                if (hasNext<char>('i')) value = Complex(0, real);
                else {
                    if (hasNext<char>('+')) imaginary = 1;
                    else if (hasNext<char>('-')) imaginary = -1;
                    else {
                        value = Complex(real);

                        setSkipWhitespace(true);

                        return true;
                    }

                    if (!hasNext<char>('i')) {
                        double factor;

                        if (!tryNext<double>(factor) || !hasNext<char>('i')) {
                            setSkipWhitespace(true);

                            return false;
                        }

                        imaginary *= factor;
                    }

                    value = Complex(real, imaginary);
                }
            }

            setSkipWhitespace(true);

            return true;
        }

        bool tryNextVector(Vector<Float> & value) {
            if (!hasNext<char>('(')) return false;

            if (!tryNext<Float>(value[0])) return false;
            if (!tryNext<Float>(value[1])) return false;
            if (!tryNext<Float>(value[2])) return false;

            if (!hasNext<char>(')')) return false;

            return true;
        }

        template <typename T>
        bool tryNextSpectrum(std::map<double, T> & value) {
            if (!hasNext<char>('(')) return false;

            while (hasNext<char>()) {
                if (hasNext<char>(')')) return true;

                double lambda;

                if (!tryNext<double>(lambda)) return false;
                if (!tryNext<T>(value[lambda])) return false;
            }

            return false;
        }
};

class ParserRegistry {
    public:
        ParserRegistry(Payload & p) : payload(p) {}

        template <typename T>
        int add(const T & value, const std::string & name = "") {
            if constexpr (std::is_same_v<T, DenseSpectrum<Float>>) return add(value, name, payload.spectra, spectrumIndices);
            else if constexpr (std::is_same_v<T, DenseSpectrum<Complex>>) return add(value, name, payload.complexSpectra, complexSpectrumIndices);
            else if constexpr (std::is_same_v<T, Object>) {
                int index = int(payload.objects.size());

                if (!has<Object>(name)) add(index, name, payload.objectOffsets, objectIndices);

                payload.objects.push_back(value);

                return index;
            }
            else if constexpr (std::is_same_v<T, Material>) return add(value, name, payload.materials, materialIndices);
            else if constexpr (std::is_same_v<T, ScalarTexture>) return add(value, name, payload.scalarTextures, scalarTextureIndices);
            else if constexpr (std::is_same_v<T, SpectrumTexture>) return add(value, name, payload.spectrumTextures, spectrumTextureIndices);
            else return -1;
        }

        int addObjects(const std::vector<Object> & objects, const std::string & name = "") {
            int index = int(payload.objects.size());

            if (!objects.empty()) {
                if (!has<Object>(name)) add(index, name, payload.objectOffsets, objectIndices);

                payload.objects.insert(payload.objects.end(), objects.begin(), objects.end());
            }

            return index;
        }

        int addImage(std::vector<Float> image, ColorSpace space, int width, int height, int channels, const std::string & name = "") {
            if (!name.empty()) imageDataIndices[name] = int(imageIndices.size());

            imageSpaces.push_back(space);
            imageIndices.push_back(int(payload.images.size()));
            imageWidths.push_back(width);
            imageHeights.push_back(height);
            imageChannels.push_back(channels);

            payload.images.insert(payload.images.end(), image.begin(), image.end());

            return imageIndices.back();
        }

        template <typename T>
        bool has(const std::string & name) const {
            if constexpr (std::is_same_v<T, DenseSpectrum<Float>>) return spectrumIndices.contains(name);
            else if constexpr (std::is_same_v<T, DenseSpectrum<Complex>>) return complexSpectrumIndices.contains(name);
            else if constexpr (std::is_same_v<T, Object>) return objectIndices.contains(name);
            else if constexpr (std::is_same_v<T, Material>) return materialIndices.contains(name);
            else if constexpr (std::is_same_v<T, ScalarTexture>) return scalarTextureIndices.contains(name);
            else if constexpr (std::is_same_v<T, SpectrumTexture>) return spectrumTextureIndices.contains(name);
            else return false;
        }

        template <typename T>
        bool has(const std::string & name, int & index) const {
            if constexpr (std::is_same_v<T, DenseSpectrum<Float>>) return has(name, index, spectrumIndices);
            else if constexpr (std::is_same_v<T, DenseSpectrum<Complex>>) return has(name, index, complexSpectrumIndices);
            else if constexpr (std::is_same_v<T, Object>) return has(name, index, objectIndices);
            else if constexpr (std::is_same_v<T, Material>) return has(name, index, materialIndices);
            else if constexpr (std::is_same_v<T, ScalarTexture>) return has(name, index, scalarTextureIndices);
            else if constexpr (std::is_same_v<T, SpectrumTexture>) return has(name, index, spectrumTextureIndices);
            else return false;
        }

        bool hasImage(const std::string & name, ColorSpace & space, int & index, int & width, int & height, int & channels) const {
            int dataIndex;

            if (!has(name, dataIndex, imageDataIndices)) return false;

            space = imageSpaces[dataIndex];
            index = imageIndices[dataIndex];
            width = imageWidths[dataIndex];
            height = imageHeights[dataIndex];
            channels = imageChannels[dataIndex];

            return true;
        }

    private:
        Payload & payload;

        std::map<std::string, int> spectrumIndices;
        std::map<std::string, int> complexSpectrumIndices;
        std::map<std::string, int> objectIndices;
        std::map<std::string, int> materialIndices;
        std::map<std::string, int> scalarTextureIndices;
        std::map<std::string, int> spectrumTextureIndices;

        std::map<std::string, int> imageDataIndices;

        std::vector<ColorSpace> imageSpaces;
        std::vector<int> imageIndices, imageWidths, imageHeights, imageChannels;

        template <typename T>
        int add(const T & value, const std::string & name, std::vector<T> & container, std::map<std::string, int> & indices) {
            if (!name.empty()) indices[name] = int(container.size());

            container.push_back(value);

            return int(container.size()) - 1;
        }

        bool has(const std::string & name, int & index, const std::map<std::string, int> & indices) const {
            auto iterator = indices.find(name);

            if (iterator == indices.end()) return false;

            index = iterator->second;

            return true;
        }
};

class ParserValidator {
    public:
        ParserValidator(const ParserStream & s) : stream(s) {}

        void validate(bool condition, const std::string & message) const { if (!condition) stream.raiseError(message); }

        [[noreturn]] void valid(const std::string & parameter) const { stream.raiseError(prefix(parameter) + " is missing or invalid"); }

        void valid(const std::string & parameter, bool condition) const { if (!condition) valid(parameter); }

        template <typename T>
        void positive(const std::string & parameter, T value) const { validate(value > 0, prefix(parameter) + " must be positive" + suffix(std::to_string(value))); }

        template <typename T>
        void nonNegative(const std::string & parameter, T value) const { validate(value >= 0, prefix(parameter) + " must be non-negative" + suffix(std::to_string(value))); }

        template <typename T>
        void nonZero(const std::string & parameter, T value) const { validate(std::fabs(value) > EPSILON_SQUARED, prefix(parameter) + " must be non-zero" + suffix(std::to_string(value))); }

        template <typename T>
        void atLeast(const std::string & parameter, T value, T min) const { validate(value >= min, prefix(parameter) + " must be at least " + std::to_string(min) + suffix(std::to_string(value))); }

        template <typename T>
        void atMost(const std::string & parameter, T value, T max) const { validate(value <= max, prefix(parameter) + " must be at most " + std::to_string(max) + suffix(std::to_string(value))); }

        template <typename T>
        void inRange(const std::string & parameter, T value, T min, T max) const {
            atLeast(parameter, value, min);
            atMost(parameter, value, max);
        }

        template <typename... Ts>
        void defined(const std::string & parameter, const ParserRegistry & registry, const std::string & name, int & index) const { validate((registry.has<Ts>(name, index) || ...), prefix(parameter) + " is not defined"); }

        template <typename... Ts>
        void notDefined(const std::string & parameter, const ParserRegistry & registry, const std::string & name) const {
            int index;

            notDefined<Ts...>(parameter, registry, name, index);
        }

        template <typename... Ts>
        void notDefined(const std::string & parameter, const ParserRegistry & registry, const std::string & name, int & index) const { validate((!registry.has<Ts>(name, index) && ...), prefix(parameter) + " is already defined"); }

        void expected(const std::string & parameter, const std::string & value, const std::vector<std::string> & expectedValues) const {
            bool found = false;

            std::string comparisons = "";

            for (int j = 0; j < int(expectedValues.size()); j++) {
                if (expectedValues[j] == value) {
                    found = true;
                    break;
                }

                std::string comparison = "\"" + expectedValues[j] + "\"";

                if (j != 0) {
                    if (j == int(expectedValues.size()) - 1) {
                        if (j == 1) comparison = " or " + comparison;
                        else comparison = ", or " + comparison;
                    }
                    else comparison = ", " + comparison;
                }

                comparisons += comparison;
            }

            validate(found, (parameter.empty() ? "expected " : prefix(parameter) + " must be ") + comparisons + suffix("\"" + value + "\""));
        }

        void unexpected(char token) const { validate(token == '\0', "unexpected token \"" + std::string(1, token) + "\""); }

    private:
        const ParserStream & stream;

        static std::string prefix(const std::string & parameter) { return "'" + parameter + "'"; }

        static std::string suffix(const std::string & value) { return ", got " + value; }
};

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

        static void parseLRD(const std::string & input, Renderer & renderer, Payload & payload) {
            std::ifstream inputFile(input);

            if (!inputFile.is_open()) failedToOpenFileError(input);

            bool render = false;

            ParserRegistry registry(payload);

            registry.add<DenseSpectrum<Float>>(DenseSpectrum<Float>(0));
            registry.add<SpectrumTexture>(SpectrumTexture::makeConstant(0), "default");
            registry.add<ScalarTexture>(ScalarTexture::makeConstant(0), "default");
            registry.add<Material>(Material::makeLambertian(0), "default");
            registry.add<Object>(Object::makeSphere(0, Vector<Float>(), 1), "default");

            payload.background = Background::makeEquirectangular(0);

            std::string line;

            ParserStream stream(input);
            ParserValidator validator(stream);

            while (std::getline(inputFile, line)) {
                stream.load(stripComments(line));

                if (!stream.hasNext<char>()) continue;

                std::string command = stream.next<std::string>();

                if (!render) validator.expected("", command, {"Render"});

                validator.expected("", command, {"Background", "Camera", "Instance", "Material", "Render", "Object", "Texture"});

                if (command == "Render") {
                    render = true;

                    std::string spaceString = stream.next<std::string>("space");

                    validator.expected("space", spaceString, {"srgb", "rec2020", "aces2065-1"});

                    ColorSpace space = ColorSpace::SRGB;

                    if (spaceString == "rec2020") space = ColorSpace::REC2020;
                    else if (spaceString == "aces2065-1") space = ColorSpace::ACES2065;

                    int width = stream.next<int>("width");

                    validator.positive("width", width);

                    int height = stream.next<int>("height");

                    validator.positive("height", height);

                    int samples = stream.next<int>("samples");
                    int sqrtSamples = int(std::sqrt(samples));

                    validator.atLeast("samples", samples, 1);
                    validator.validate(samples == sqrtSamples * sqrtSamples, "'samples' must be a perfect square");

                    int depth = stream.next<int>("depth");

                    validator.nonNegative("depth", depth);

                    Float lambdaMin = stream.next<Float>("lambdaMin");

                    validator.inRange("lambdaMin", lambdaMin, Float(CIE_LAMBDA_MIN), Float(CIE_LAMBDA_MAX));

                    Float lambdaMax = stream.next<Float>("lambdaMax");

                    validator.inRange("lambdaMax", lambdaMax, Float(CIE_LAMBDA_MIN), Float(CIE_LAMBDA_MAX));
                    validator.validate(lambdaMin <= lambdaMax, "'lambdaMin' must be at most 'lambdaMax', got " + std::to_string(lambdaMin) + " and " + std::to_string(lambdaMax));

                    uint64_t seed = stream.next<uint64_t>("seed");

                    renderer = Renderer(space, width, height, samples, sqrtSamples, depth, lambdaMin, lambdaMax, seed);
                }
                else if (command == "Texture") {
                    std::string name = stream.next<std::string>("name");

                    validator.notDefined<ScalarTexture, SpectrumTexture>("name", registry, name);

                    std::string type = stream.next<std::string>("type");

                    validator.expected("type", type, {"scalar", "spectrum"});

                    if (type == "scalar") {
                        ScalarTexture texture;

                        std::string subtype = stream.next<std::string>("subtype");

                        validator.expected("subtype", subtype, {"constant", "image", "perlin", "worley"});

                        if (subtype == "constant") texture = ScalarTexture::makeConstant(stream.next<Float>("value"));
                        else if (subtype == "perlin" || subtype == "worley") {
                            Float min = stream.next<Float>("min");
                            Float max = stream.next<Float>("max");

                            validator.validate(min <= max, "'min' must be at most 'max', got " + std::to_string(min) + " and " + std::to_string(max));

                            Float frequency = stream.next<Float>("frequency");

                            if (subtype == "perlin") texture = ScalarTexture::makePerlin(min, max, frequency);
                            else if (subtype == "worley") texture = ScalarTexture::makeWorley(min, max, frequency);
                        }
                        else if (subtype == "image") {
                            std::string file = stream.next<std::string>("file");

                            ColorSpace space;
                            int index, width, height, channels;

                            if (!registry.hasImage(file, space, index, width, height, channels) || channels != 1) {
                                std::vector<Float> image = parseImage(file, 1, space, width, height);

                                index = registry.addImage(image, space, width, height, 1, file);
                            }

                            texture = ScalarTexture::makeImage(index, width, height);
                        }

                        registry.add<ScalarTexture>(texture, name);
                    }
                    else if (type == "spectrum") {
                        SpectrumTexture texture;

                        std::string subtype = stream.next<std::string>("subtype");

                        validator.expected("subtype", subtype, {"constant", "checker", "image", "scalar"});

                        if (subtype == "constant") texture = SpectrumTexture::makeConstant(parseSpectrum<Float>("value", stream, validator, registry));
                        else if (subtype == "checker") {
                            int value1 = parseSpectrum<Float>("value1", stream, validator, registry);
                            int value2 = parseSpectrum<Float>("value2", stream, validator, registry);

                            Float scale = stream.next<Float>("scale");

                            validator.nonZero("scale", scale);

                            texture = SpectrumTexture::makeChecker(value1, value2, scale);
                        }
                        else if (subtype == "scalar") {
                            std::string scalarTextureName = stream.next<std::string>("texture");

                            int scalarTextureIndex;

                            validator.defined<ScalarTexture>("texture", registry, scalarTextureName, scalarTextureIndex);

                            texture = SpectrumTexture::makeScalar(scalarTextureIndex);
                        }
                        else if (subtype == "image") {
                            std::string file = stream.next<std::string>("file");

                            ColorSpace space;
                            int index, width, height, channels;

                            if (!registry.hasImage(file, space, index, width, height, channels) || channels != 3) {
                                std::vector<Float> image = parseImage(file, 3, space, width, height);

                                index = registry.addImage(image, space, width, height, 3, file);
                            }

                            texture = SpectrumTexture::makeImage(space, index, width, height);
                        }

                        registry.add<SpectrumTexture>(texture, name);
                    }
                }
                else if (command == "Material") {
                    Material material;

                    std::string name = stream.next<std::string>("name");

                    validator.notDefined<Material>("name", registry, name);

                    std::string type = stream.next<std::string>("type");

                    validator.expected("type", type, {"dielectric", "emissive", "lambertian", "mirror", "thinfilm"});

                    if (type == "lambertian" || type == "mirror") {
                        int albedo = parseSpectrumTexture("albedo", stream, validator, registry);

                        const SpectrumTexture & albedoTexture = payload.spectrumTextures[albedo];

                        validator.atLeast("albedo", albedoTexture.min(payload.spectra.data(), payload.scalarTextures.data(), payload.images.data()), Float(0));
                        validator.atMost("albedo", albedoTexture.max(payload.spectra.data(), payload.scalarTextures.data(), payload.images.data()), Float(1));

                        if (type == "lambertian") material = Material::makeLambertian(albedo);
                        else if (type == "mirror") material = Material::makeMirror(albedo);
                    }
                    else if (type == "dielectric") {
                        int n0 = parseSpectrum<Float>("n0", stream, validator, registry);

                        for (int i = 0; i < CIE_LAMBDA_BINS; i++)
                            validator.positive("n0", payload.spectra[n0][i]);

                        int n1 = parseSpectrum<Float>("n1", stream, validator, registry);

                        for (int i = 0; i < CIE_LAMBDA_BINS; i++)
                            validator.positive("n1", payload.spectra[n1][i]);

                        material = Material::makeDielectric(n0, n1);
                    }
                    else if (type == "emissive") material = Material::makeEmissive(parseSpectrumTexture("emission", stream, validator, registry));
                    else if (type == "thinfilm") {
                        std::vector<int> n = parseArray<DenseSpectrum<Complex>>("n", stream, validator, registry);

                        validator.validate(n.size() >= 2, "'n' must have at least 2 entries, got " + std::to_string(n.size()));

                        for (int i = 0; i < int(n.size()); i++)
                            for (int j = 0; j < CIE_LAMBDA_BINS; j++)
                                validator.positive("n", payload.complexSpectra[n[i]][j].real());

                        int nOffset = int(payload.materialProperties.size());

                        payload.materialProperties.insert(payload.materialProperties.end(), n.begin(), n.end());

                        int numLayers = int(n.size() - 2);

                        std::vector<int> d = parseArray<ScalarTexture>("d", stream, validator, registry);

                        validator.validate(int(d.size()) == numLayers, "'d' must have " + std::to_string(numLayers) + " entries, got " + std::to_string(d.size()));

                        int dOffset = int(payload.materialProperties.size());

                        payload.materialProperties.insert(payload.materialProperties.end(), d.begin(), d.end());

                        for (int i = 0; i < numLayers; i++)
                            validator.positive("d", payload.scalarTextures[payload.materialProperties[dOffset + i]].min(payload.images.data()));

                        material = Material::makeThinFilm(numLayers, nOffset, dOffset);
                    }

                    registry.add<Material>(material, name);
                }
                else if (command == "Object") {
                    Object object;

                    std::string name = stream.next<std::string>("name");

                    int index;

                    validator.validate(!registry.has<Object>(name, index) || index == int(payload.objectOffsets.size()) - 1, "'name' is already defined");

                    std::string type = stream.next<std::string>("type");

                    validator.expected("type", type, {"mesh", "quad", "sphere", "tri"});

                    std::string materialName = stream.next<std::string>("material");

                    int material;

                    validator.defined<Material>("material", registry, materialName, material);

                    if (type == "sphere") {
                        Vector<Float> center = stream.next<Vector<Float>>("center");
                        Float radius = stream.next<Float>("radius");

                        validator.positive("radius", radius);

                        registry.add<Object>(Object::makeSphere(material, center, radius), name);
                    }
                    else if (type == "quad" || type == "tri") {
                        Vector<Float> corner = stream.next<Vector<Float>>("corner");
                        Vector<Float> horizontal = stream.next<Vector<Float>>("horizontal");

                        validator.nonZero("horizontal", horizontal.length());

                        Vector<Float> vertical = stream.next<Vector<Float>>("vertical");

                        validator.nonZero("vertical", vertical.length());
                        validator.validate(cross(horizontal, vertical).length() > EPSILON_SQUARED, "'horizontal' and 'vertical' must be non-parallel");

                        if (type == "quad") registry.add<Object>(Object::makeQuad(material, corner, horizontal, vertical), name);
                        else if (type == "tri") registry.add<Object>(Object::makeTri(material, corner, horizontal, vertical), name);
                    }
                    else if (type == "mesh") {
                        std::string file = stream.next<std::string>("file");

                        registry.addObjects(parseOBJ(file, material), name);
                    }
                }
                else if (command == "Instance") {
                    std::string objectName = stream.next<std::string>("object");

                    int objectIndex;

                    validator.defined<Object>("object", registry, objectName, objectIndex);

                    std::string materialName = stream.next<std::string>("material");

                    int object, count, material = -1;

                    if (materialName != "default") validator.defined<Material>("material", registry, materialName, material);

                    object = payload.objectOffsets[objectIndex];
                    count = objectIndex < int(payload.objectOffsets.size()) - 1 ? payload.objectOffsets[objectIndex + 1] - object : int(payload.objects.size()) - object;

                    Vector<Float> translation = stream.next<Vector<Float>>("translation");
                    Vector<Float> rotation = stream.next<Vector<Float>>("rotation");
                    Vector<Float> scale = stream.next<Vector<Float>>("scale");

                    validator.nonZero("scale", scale[0]);
                    validator.nonZero("scale", scale[1]);
                    validator.nonZero("scale", scale[2]);

                    Instance instance(object, count, material, translation, rotation, scale);

                    if (std::fabs(scale[0] - scale[1]) > EPSILON_SQUARED || std::fabs(scale[1] - scale[2]) > EPSILON_SQUARED)
                        for (int i = 0; i < count; i++)
                            validator.validate(!payload.materials[instance.getMaterial(payload.objects.data(), i)].isEmissive(), "'scale' must be uniform for emissive objects");

                    payload.instances.push_back(instance);
                }
                else if (command == "Background") {
                    std::string type = stream.next<std::string>("type");

                    validator.expected("type", type, {"equirectangular"});

                    if (type == "equirectangular") payload.background = Background::makeEquirectangular(parseSpectrumTexture("background", stream, validator, registry));
                }
                else if (command == "Camera") {
                    Vector<Float> position = stream.next<Vector<Float>>("position");
                    Vector<Float> corner = stream.next<Vector<Float>>("corner");
                    Vector<Float> horizontal = stream.next<Vector<Float>>("horizontal");

                    validator.nonZero("horizontal", horizontal.length());

                    Vector<Float> vertical = stream.next<Vector<Float>>("vertical");

                    validator.nonZero("vertical", vertical.length());
                    validator.validate(cross(horizontal, vertical).length() > EPSILON_SQUARED, "'horizontal' and 'vertical' must be non-parallel");

                    renderer.setCamera(position, corner, horizontal, vertical);
                }

                validator.unexpected(stream.peek<char>());
            }

            inputFile.close();

            payload.objectOffsets.push_back(int(payload.objects.size()));

            payload.nodes = BVH::makeBVH(payload.objects, payload.instances, payload.objectOffsets);

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

            if (payload.totalLightPower < EPSILON_SQUARED) payload.totalLightPower = 1;
        }

    private:
        static std::string stripComments(const std::string & s) {
            size_t commentPos = s.find('#');

            return commentPos == std::string::npos ? s : s.substr(0, commentPos);
        }

        template <typename T>
        static int parseSpectrum(const std::string & parameter, ParserStream & stream, const ParserValidator & validator, ParserRegistry & registry) {
            if (stream.hasNext<T>()) return registry.add<DenseSpectrum<T>>(DenseSpectrum<T>(stream.next<T>(parameter)));

            std::map<double, T> samples;

            if (hasExtension(stream.peek<std::string>(), ".spd")) {
                std::string file = stream.next<std::string>(parameter);

                int index;

                if (registry.has<DenseSpectrum<T>>(file, index)) return index;

                samples = parseSPD<T>(file);
            }
            else samples = stream.next<std::map<double, T>>(parameter);

            validator.valid("samples", !samples.empty());

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

            return registry.add<DenseSpectrum<T>>(spectrum);
        }

        static int parseScalarTexture(const std::string & parameter, ParserStream & stream, const ParserValidator & validator, ParserRegistry & registry) {
            if (stream.hasNext<Float>()) return registry.add<ScalarTexture>(ScalarTexture::makeConstant(stream.next<Float>()));

            int index;

            validator.defined<ScalarTexture>(parameter, registry, stream.next<std::string>(parameter), index);

            return index;
        }

        static int parseSpectrumTexture(const std::string & parameter, ParserStream & stream, const ParserValidator & validator, ParserRegistry & registry) {
            if (stream.hasNext<Float>() || stream.peek<char>() == '(' || hasExtension(stream.peek<std::string>(), ".spd")) return registry.add<SpectrumTexture>(SpectrumTexture::makeConstant(parseSpectrum<Float>(parameter, stream, validator, registry)));
            else {
                std::string name = stream.next<std::string>(parameter);

                int index;

                if (registry.has<SpectrumTexture>(name, index)) return index;

                validator.defined<ScalarTexture>(parameter, registry, name, index);

                return registry.add<SpectrumTexture>(SpectrumTexture::makeScalar(index));
            }
        }

        template <typename T>
        static std::vector<int> parseArray(const std::string & parameter, ParserStream & stream, const ParserValidator & validator, ParserRegistry & registry) {
            validator.valid(parameter, stream.hasNext<char>('['));

            std::vector<int> values;

            while (stream.hasNext<char>()) {
                if (stream.hasNext<char>(']')) return values;

                if constexpr (std::is_same_v<T, DenseSpectrum<Float>>) values.push_back(parseSpectrum<Float>(parameter, stream, validator, registry));
                else if constexpr (std::is_same_v<T, DenseSpectrum<Complex>>) values.push_back(parseSpectrum<Complex>(parameter, stream, validator, registry));
                else if constexpr (std::is_same_v<T, ScalarTexture>) values.push_back(parseScalarTexture(parameter, stream, validator, registry));
                else if constexpr (std::is_same_v<T, SpectrumTexture>) values.push_back(parseSpectrumTexture(parameter, stream, validator, registry));
                else values.push_back(-1);
            }

            validator.valid(parameter);
        }

        static std::vector<Float> parseImage(const std::string & input, int channels, ColorSpace & space, int & width, int & height) {
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

            Matrix<double, 3> colorTransform(1, 0, 0, 0, 1, 0, 0, 0, 1);

            bool needsTransform = false;

            if (channels == 3) {
                if (colorSpaceContains(ColorSpace::SRGB, rx, ry, gx, gy, bx, by, wx, wy)) space = ColorSpace::SRGB;
                else if (colorSpaceContains(ColorSpace::REC2020, rx, ry, gx, gy, bx, by, wx, wy)) space = ColorSpace::REC2020;
                else space = ColorSpace::ACES2065;

                needsTransform = std::fabs(rx - chromaticity(space, 0)) > EPSILON_SQUARED || std::fabs(ry - chromaticity(space, 1)) > EPSILON_SQUARED || std::fabs(gx - chromaticity(space, 2)) > EPSILON_SQUARED || std::fabs(gy - chromaticity(space, 3)) > EPSILON_SQUARED || std::fabs(bx - chromaticity(space, 4)) > EPSILON_SQUARED || std::fabs(by - chromaticity(space, 5)) > EPSILON_SQUARED || std::fabs(wx - chromaticity(space, 6)) > EPSILON_SQUARED || std::fabs(wy - chromaticity(space, 7)) > EPSILON_SQUARED;

                if (needsTransform) colorTransform = toRGBMatrix(space) * bradfordAdapt(wx, wy, chromaticity(space, 6), chromaticity(space, 7)) * toXYZMatrix(rx, ry, gx, gy, bx, by, wx, wy);
            }

            size_t totalElements = width * height * channels;

            std::vector<Float> image(totalElements);

            Vector<Float> color;

            for (size_t i = 0, j = 0; i < totalElements; i += channels, j += 4) {
                for (int k = 0; k < channels; k++) {
                    color[k] = isEXR ? (std::isfinite(dataEXR[j + k]) ? Float(dataEXR[j + k]) : Float(0)) : is32Bit ? (std::isfinite(data32[i + k]) ? Float(data32[i + k]) : Float(0)) : is16Bit ? Float(data16[i + k]) / Float(65535) : Float(data8[i + k]) / Float(255);

                    if (sRGB) color[k] = sRGBToLinear(color[k]);
                }

                if (needsTransform && channels == 3) color = transform(colorTransform, color);

                for (int k = 0; k < channels; k++)
                    image[i + k] = color[k];
            }

            if (data8) stbi_image_free(data8);
            if (data16) stbi_image_free(data16);
            if (data32) stbi_image_free(data32);
            if (dataEXR) free(dataEXR);

            return image;
        }

        static std::vector<Object> parseOBJ(const std::string & input, int material) {
            if (!hasExtension(input, ".obj")) fileError(input, "invalid file extension");

            std::ifstream inputFile(input);

            if (!inputFile.is_open()) failedToOpenFileError(input);

            std::vector<Vector<Float>> vertices;
            std::vector<Float> u, v;
            std::vector<Vector<Float>> normals;

            std::vector<int> vertexIndices, textureIndices, normalIndices;

            std::vector<Object> objects;

            std::string line;

            ParserStream stream(input);
            ParserValidator validator(stream);

            while (std::getline(inputFile, line)) {
                stream.load(stripComments(line));

                if (!stream.hasNext<char>()) continue;

                std::string tag = stream.next<std::string>("", false);

                if (tag == "v") {
                    Float x = stream.next<Float>("x");
                    Float y = stream.next<Float>("y");
                    Float z = stream.next<Float>("z");

                    vertices.push_back(Vector<Float>(x, y, z));
                }
                else if (tag == "vt") {
                    u.push_back(stream.next<Float>("u"));
                    v.push_back(stream.next<Float>("v"));
                }
                else if (tag == "vn") {
                    Float x = stream.next<Float>("x");
                    Float y = stream.next<Float>("y");
                    Float z = stream.next<Float>("z");

                    normals.push_back(Vector<Float>(x, y, z));
                }
                else if (tag == "f") {
                    vertexIndices.clear();
                    textureIndices.clear();
                    normalIndices.clear();

                    while (stream.hasNext<char>()) {
                        int vertexIndex = stream.next<int>("v");

                        if (vertexIndex < 0) vertexIndex += int(vertices.size());
                        else if (vertexIndex > 0) vertexIndex--;

                        validator.inRange("v", vertexIndex + 1, 1, int(vertices.size()));

                        vertexIndices.push_back(vertexIndex);

                        int textureIndex = -1, normalIndex = -1;

                        stream.setSkipWhitespace(false);

                        if (stream.hasNext<char>('/')) {
                            if (!stream.hasNext<char>('/')) {
                                textureIndex = stream.next<int>("vt");

                                if (textureIndex < 0) textureIndex += int(u.size());
                                else if (textureIndex >= 0) textureIndex--;

                                validator.inRange("vt", textureIndex + 1, 1, int(u.size()));

                                stream.hasNext<char>('/');
                            }

                            if (stream.hasNext<char>() && !std::isspace(stream.peek<char>())) {
                                normalIndex = stream.next<int>("vn");

                                if (normalIndex < 0) normalIndex += int(normals.size());
                                else if (normalIndex >= 0) normalIndex--;

                                validator.inRange("vn", normalIndex + 1, 1, int(normals.size()));
                                validator.valid("vn", !stream.hasNext<char>() || std::isspace(stream.peek<char>()));
                            }
                        }

                        stream.setSkipWhitespace(true);

                        textureIndices.push_back(textureIndex);
                        normalIndices.push_back(normalIndex);
                    }

                    validator.validate(vertexIndices.size() >= 3, "'f' must have at least 3 vertices");

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

                        validator.validate((vertex1 - vertex0).length() > EPSILON_SQUARED && (vertex2 - vertex0).length() > EPSILON_SQUARED && cross(vertex1 - vertex0, vertex2 - vertex0).length() > EPSILON_SQUARED, "'f' must be non-degenerate");

                        objects.push_back(Object::makeTri(material, vertex0, vertex1 - vertex0, vertex2 - vertex0, normal0, normal1, normal2, u0, u1, u2, v0, v1, v2));
                    }
                }
                else continue;

                validator.unexpected(stream.peek<char>());
            }

            inputFile.close();

            return objects;
        }

        template <typename T>
        static std::map<double, T> parseSPD(const std::string & input) {
            std::ifstream inputFile(input);

            if (!inputFile.is_open()) failedToOpenFileError(input);

            std::map<double, T> samples;

            std::string line;

            ParserStream stream(input);
            ParserValidator validator(stream);

            while (std::getline(inputFile, line)) {
                stream.load(stripComments(line));

                if (!stream.hasNext<char>()) continue;

                double lambda = stream.next<double>("lambda");
                samples[lambda] = stream.next<T>("value");

                validator.unexpected(stream.peek<char>());
            }

            inputFile.close();

            return samples;
        }
};