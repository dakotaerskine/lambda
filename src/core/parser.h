#pragma once

#include <bit>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <stb/stb_image.h>
#include <tinyexr/tinyexr.h>

#include "core/buffer.h"
#include "core/constants.h"
#include "core/context.h"
#include "core/error.h"
#include "core/platform.h"
#include "core/utils.h"
#include "core/writer.h"
#include "math/complex.h"
#include "math/spectrum.h"
#include "math/vector.h"
#include "render/renderer.h"
#include "scene/background.h"
#include "scene/instance.h"
#include "scene/material.h"
#include "scene/medium.h"
#include "scene/mesh.h"
#include "scene/object.h"
#include "scene/texture.h"

namespace lambda {
    class ParserStream {
        public:
            ParserStream(const std::string & _input) : input(_input), lineNumber(0), columnNumber(1) {}

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

            [[noreturn]] void raiseError(const std::string & message) const { error::fileError(input, lineNumber, columnNumber, message); }

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
                else if constexpr (std::is_same_v<T, Vector<int, 3>> || std::is_same_v<T, Vector<int, 4>> || std::is_same_v<T, Vector<Float, 2>> || std::is_same_v<T, Vector<Float, 3>>) success = tryNextVector(value);
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

                    bool hasReal = hasNext<double>();

                    if (hasReal && !tryNext<double>(real)) return false;

                    setSkipWhitespace(false);

                    if (hasNext<char>('i')) value = Complex(0, real);
                    else {
                        if (hasNext<char>('+')) imaginary = 1;
                        else if (hasNext<char>('-')) imaginary = -1;
                        else {
                            setSkipWhitespace(true);

                            if (hasReal) {
                                value = Complex(real);

                                return true;
                            }
                            else return false;
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

            template <typename T, int N>
            bool tryNextVector(Vector<T, N> & value) {
                if (!hasNext<char>('(')) return false;

                for (int i = 0; i < N; i++) if (!tryNext<T>(value[i])) return false;

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
            ParserRegistry(Context & c) : context(c) {}

            ~ParserRegistry() { context.flushObjects(); }

            template <typename T>
            DenseSpectrum<T> * addSpectrum(const DenseSpectrum<T> & spectrum, const std::string & name = "") {
                DenseSpectrum<T> * pointer = context.addSpectrum(spectrum);

                if (!name.empty()) {
                    if constexpr (std::is_same_v<T, Float>) spectrumPointers[name] = pointer;
                    else if constexpr (std::is_same_v<T, Complex>) complexSpectrumPointers[name] = pointer;
                }

                return pointer;
            }

            Float * addImage(std::vector<Float> image, ColorSpace space, int width, int height, int channels, const std::string & name = "") {
                Float * pointer = context.addImage(image.data(), int(std::ssize(image)));

                if (!name.empty()) imageIndices[name] = int(std::ssize(imagePointers));

                imagePointers.push_back(pointer);
                imageSpaces.push_back(space);
                imageWidths.push_back(width);
                imageHeights.push_back(height);
                imageChannels.push_back(channels);

                return pointer;
            }

            ScalarTexture * addScalarTexture(const ScalarTexture & texture, const std::string & name = "") {
                ScalarTexture * pointer = context.addScalarTexture(texture);

                if (!name.empty()) scalarTexturePointers[name] = pointer;

                return pointer;
            }

            SpectrumTexture * addSpectrumTexture(const SpectrumTexture & texture, const std::string & name = "") {
                SpectrumTexture * pointer = context.addSpectrumTexture(texture);

                if (!name.empty()) spectrumTexturePointers[name] = pointer;

                return pointer;
            }

            Medium * addMedium(const Medium & medium, const std::string & name = "") {
                Medium * pointer = context.addMedium(medium);

                if (!name.empty()) mediumPointers[name] = pointer;

                return pointer;
            }

            Material * addMaterial(const Material & material, const std::string & name = "") {
                Material * pointer = context.addMaterial(material);

                if (!name.empty()) materialPointers[name] = pointer;

                return pointer;
            }

            void addObject(const Object & object, const std::string & name = "") {
                if (!has<Object>(name)) {
                    context.flushObjects();

                    if (!name.empty()) objectIndices[name] = context.getObjectIndex();
                }

                context.addObject(object);
            }

            void addObjects(const std::vector<Object> & objects, const std::string & name = "") {
                if (!has<Object>(name)) {
                    context.flushObjects();

                    if (!name.empty()) objectIndices[name] = context.getObjectIndex();
                }

                context.addObjects(objects.data(), int(std::ssize(objects)));
            }

            template <typename T>
            bool has(const std::string & name) const {
                if constexpr (std::is_same_v<T, Object>) {
                    int index;

                    return has<Object>(name, index);
                }
                else {
                    T * pointer;

                    return has(name, pointer);
                }
            }

            template <typename T>
            bool has(const std::string & name, T * & pointer) const {
                if constexpr (std::is_same_v<T, DenseSpectrum<Float>>) return has(name, pointer, spectrumPointers);
                else if constexpr (std::is_same_v<T, DenseSpectrum<Complex>>) return has(name, pointer, complexSpectrumPointers);
                else if constexpr (std::is_same_v<T, ScalarTexture>) return has(name, pointer, scalarTexturePointers);
                else if constexpr (std::is_same_v<T, SpectrumTexture>) return has(name, pointer, spectrumTexturePointers);
                else if constexpr (std::is_same_v<T, Medium>) return has(name, pointer, mediumPointers);
                else if constexpr (std::is_same_v<T, Material>) return has(name, pointer, materialPointers);
                else return false;
            }

            template <typename T>
            bool has(const std::string & name, int & index) const {
                if constexpr (std::is_same_v<T, Object>) return has(name, index, objectIndices);
                else return false;
            }

            bool hasImage(const std::string & name, Float * & pointer, ColorSpace & space, int & width, int & height, int & channels) const {
                int index;

                if (!has(name, index, imageIndices)) return false;

                space = imageSpaces[index];
                pointer = imagePointers[index];
                width = imageWidths[index];
                height = imageHeights[index];
                channels = imageChannels[index];

                return true;
            }

        private:
            Context & context;

            std::map<std::string, DenseSpectrum<Float> *> spectrumPointers;
            std::map<std::string, DenseSpectrum<Complex> *> complexSpectrumPointers;
            std::map<std::string, ScalarTexture *> scalarTexturePointers;
            std::map<std::string, SpectrumTexture *> spectrumTexturePointers;
            std::map<std::string, Medium *> mediumPointers;
            std::map<std::string, Material *> materialPointers;

            std::map<std::string, int> objectIndices;
            std::map<std::string, int> imageIndices;

            std::vector<Float *> imagePointers;
            std::vector<ColorSpace> imageSpaces;
            std::vector<int> imageWidths, imageHeights, imageChannels;

            template <typename T>
            bool has(const std::string & name, T & pointer, const std::map<std::string, T> & indices) const {
                auto iterator = indices.find(name);

                if (iterator == indices.end()) return false;

                pointer = iterator->second;

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
            void zero(const std::string & parameter, T value) const { validate(value == 0, prefix(parameter) + " must be zero" + suffix(std::to_string(value))); }

            template <typename T>
            void nonZero(const std::string & parameter, T value) const { validate(value != 0, prefix(parameter) + " must be non-zero" + suffix(std::to_string(value))); }

            template <typename T>
            void atLeast(const std::string & parameter, T value, T min) const { validate(value >= min, prefix(parameter) + " must be at least " + std::to_string(min) + suffix(std::to_string(value))); }

            template <typename T>
            void atMost(const std::string & parameter, T value, T max) const { validate(value <= max, prefix(parameter) + " must be at most " + std::to_string(max) + suffix(std::to_string(value))); }

            template <typename T>
            void inRange(const std::string & parameter, T value, T min, T max) const {
                atLeast(parameter, value, min);
                atMost(parameter, value, max);
            }

            template <typename T>
            void greaterThan(const std::string & parameter, T value, T min) const { validate(value > min, prefix(parameter) + " must be greater than " + std::to_string(min) + suffix(std::to_string(value))); }

            template <typename T>
            void lessThan(const std::string & parameter, T value, T max) const { validate(value < max, prefix(parameter) + " must be less than " + std::to_string(max) + suffix(std::to_string(value))); }

            template <typename T>
            void inRangeExclusive(const std::string & parameter, T value, T min, T max) const {
                greaterThan(parameter, value, min);
                lessThan(parameter, value, max);
            }

            template <typename T>
            void defined(const std::string & parameter, const ParserRegistry & registry, const std::string & name, T * & pointer) const { validate(registry.has<T>(name, pointer), prefix(parameter) + " is not defined"); }

            template <typename T>
            void notDefined(const std::string & parameter, const ParserRegistry & registry, const std::string & name) const {
                T * pointer;

                validate(!registry.has<T>(name, pointer), prefix(parameter) + " is already defined");
            }

            template <typename T>
            void notDefined(const std::string & parameter, const ParserRegistry & registry, const std::string & name, T * & pointer) const { validate((!registry.has<T>(name, pointer)), prefix(parameter) + " is already defined"); }

            void expected(const std::string & parameter, const std::string & value, const std::vector<std::string> & expectedValues) const {
                bool found = false;

                std::string comparisons = "";

                for (int j = 0; j < std::ssize(expectedValues); j++) {
                    if (expectedValues[j] == value) {
                        found = true;
                        break;
                    }

                    std::string comparison = "\"" + expectedValues[j] + "\"";

                    if (j != 0) {
                        if (j == int(std::ssize(expectedValues)) - 1) {
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
            static bool hasValidExtension(const std::string & output) { return utils::hasExtension(output, ".lrd"); }

            static void parseArguments(int argc, char * argv[], std::string & program, std::string & input, std::string & output) {
                if (argc > 0) program = argv[0];

                if (argc != 2 && argc != 3) error::error("invalid number of arguments");

                input = argv[1];

                if (!hasValidExtension(input)) error::fileError(input, "invalid file extension");

                if (argc == 3) {
                    output = argv[2];

                    if (!Writer::hasValidExtension(output)) error::fileError(output, "invalid file extension");
                } else {
                    output = input;

                    output.erase(std::ssize(output) - 4);

                    output += ".exr";
                }
            }

            static bool parseUpsamplingTables(const std::string & input, float * scale, float * lut) {
                std::ifstream inputFile(input, std::ios::binary);

                if (!inputFile) return false;

                char header[4];

                inputFile.read(header, 4);

                if (!inputFile || std::memcmp(header, "SPEC", 4) != 0) return false;

                uint32_t resolution;

                inputFile.read(reinterpret_cast<char *>(&resolution), sizeof(uint32_t));

                if (!inputFile || resolution != constants::UPSAMPLING_RESOLUTION) return false;

                inputFile.read(reinterpret_cast<char *>(scale), sizeof(float) * resolution);
                inputFile.read(reinterpret_cast<char *>(lut), sizeof(float) * 3 * resolution * resolution * resolution * 3);

                if (!inputFile) return false;

                return true;
            }

            static void parseLRD(const std::string & input, Context & context) {
                std::ifstream inputFile(input);

                if (!inputFile.is_open()) error::failedToOpenFileError(input);

                bool render = false;

                ParserRegistry registry(context);

                context.setBackground(Background::makeEquirectangular(context.addSpectrumTexture(SpectrumTexture::makeConstant(context.addSpectrum(DenseSpectrum<Float>(0)))), Vector<Float, 3>()));

                std::string line;

                ParserStream stream(input);
                ParserValidator validator(stream);

                while (std::getline(inputFile, line)) {
                    stream.load(stripComments(line));

                    if (!stream.hasNext<char>()) continue;

                    std::string command = stream.next<std::string>();

                    if (!render) validator.expected("", command, {"Render"});
                    else validator.expected("", command, {"Background", "Camera", "Instance", "Material", "Medium", "Object", "Texture"});

                    if (command == "Render") {
                        render = true;

                        std::string spaceString = stream.next<std::string>("space");

                        validator.expected("space", spaceString, {"srgb", "rec2020", "aces2065-1"});

                        ColorSpace space;

                        if (spaceString == "srgb") space = ColorSpace::SRGB;
                        else if (spaceString == "rec2020") space = ColorSpace::REC2020;
                        else space = ColorSpace::ACES2065;

                        int width = stream.next<int>("width");

                        validator.positive("width", width);

                        int height = stream.next<int>("height");

                        validator.positive("height", height);

                        int samples = stream.next<int>("samples");
                        int sqrtSamples = int(std::lround(std::sqrt(samples)));

                        validator.atLeast("samples", samples, 1);
                        validator.validate(samples == sqrtSamples * sqrtSamples, "'samples' must be a perfect square");

                        int depth = stream.next<int>("depth");

                        validator.nonNegative("depth", depth);

                        Float lambdaMin = stream.next<Float>("lambdaMin");

                        validator.inRange("lambdaMin", lambdaMin, Float(constants::CIE_LAMBDA_MIN), Float(constants::CIE_LAMBDA_MAX));

                        Float lambdaMax = stream.next<Float>("lambdaMax");

                        validator.inRange("lambdaMax", lambdaMax, Float(constants::CIE_LAMBDA_MIN), Float(constants::CIE_LAMBDA_MAX));
                        validator.validate(lambdaMin < lambdaMax, "'lambdaMin' must be less than 'lambdaMax', got " + std::to_string(lambdaMin) + " and " + std::to_string(lambdaMax));

                        uint64_t seed = stream.next<uint64_t>("seed");

                        context.getRenderer()->setRender(space, width, height, samples, depth, lambdaMin, lambdaMax, seed);
                    }
                    else if (command == "Texture") {
                        std::string name = stream.next<std::string>("name");

                        validator.notDefined<ScalarTexture>("name", registry, name);
                        validator.notDefined<SpectrumTexture>("name", registry, name);

                        std::string type = stream.next<std::string>("type");

                        validator.expected("type", type, {"scalar", "spectrum"});

                        if (type == "scalar") {
                            ScalarTexture texture;

                            std::string subtype = stream.next<std::string>("subtype");

                            validator.expected("subtype", subtype, {"checker", "constant", "image", "mix", "perlin", "scale", "worley"});

                            if (subtype == "constant") texture = ScalarTexture::makeConstant(stream.next<Float>("value"));
                            else if (subtype == "scale") {
                                ScalarTexture * texture1 = parseScalarTexture("texture1", stream, validator, registry);
                                ScalarTexture * texture2 = parseScalarTexture("texture2", stream, validator, registry);

                                texture = ScalarTexture::makeScale(texture1, texture2);
                            }
                            else if (subtype == "mix") {
                                ScalarTexture * texture1 = parseScalarTexture("texture1", stream, validator, registry);
                                ScalarTexture * texture2 = parseScalarTexture("texture2", stream, validator, registry);
                                ScalarTexture * factor = parseScalarTexture("factor", stream, validator, registry);

                                validator.atLeast("factor", factor->getMin(), Float(0));
                                validator.atMost("factor", factor->getMax(), Float(1));

                                texture = ScalarTexture::makeMix(texture1, texture2, factor);
                            }
                            else if (subtype == "checker") {
                                Float value1 = stream.next<Float>("value1");
                                Float value2 = stream.next<Float>("value2");

                                Float uScale = stream.next<Float>("uScale");

                                validator.nonZero("uScale", uScale);

                                Float vScale = stream.next<Float>("vScale");

                                validator.nonZero("vScale", vScale);

                                Float uOffset = stream.next<Float>("uOffset");
                                Float vOffset = stream.next<Float>("vOffset");

                                texture = ScalarTexture::makeChecker(value1, value2, uScale, vScale, uOffset, vOffset);
                            }
                            else if (subtype == "perlin" || subtype == "worley") {
                                Float frequency = stream.next<Float>("frequency");

                                validator.nonNegative("frequency", frequency);

                                Float roughness = stream.next<Float>("roughness");

                                validator.inRange("roughness", roughness, Float(0), Float(1));

                                int octaves = stream.next<int>("octaves");

                                validator.atLeast("octaves", octaves, 1);

                                if (subtype == "perlin") texture = ScalarTexture::makePerlin(frequency, roughness, octaves);
                                else if (subtype == "worley") texture = ScalarTexture::makeWorley(frequency, roughness, octaves);
                            }
                            else if (subtype == "image") {
                                std::string file = stream.next<std::string>("file");

                                Float * pointer;
                                ColorSpace space;
                                int width, height, channels;

                                if (!registry.hasImage(file, pointer, space, width, height, channels) || channels != 1) {
                                    std::vector<Float> image = parseImage(file, 1, space, width, height);

                                    pointer = registry.addImage(image, space, width, height, 1, file);
                                }

                                Float uScale = stream.next<Float>("uScale");

                                validator.nonZero("uScale", uScale);

                                Float vScale = stream.next<Float>("vScale");

                                validator.nonZero("vScale", vScale);

                                Float uOffset = stream.next<Float>("uOffset");
                                Float vOffset = stream.next<Float>("vOffset");

                                texture = ScalarTexture::makeImage(pointer, width, height, uScale, vScale, uOffset, vOffset);
                            }

                            registry.addScalarTexture(texture, name);
                        }
                        else if (type == "spectrum") {
                            SpectrumTexture texture;

                            std::string subtype = stream.next<std::string>("subtype");

                            validator.expected("subtype", subtype, {"constant", "checker", "image", "mix", "scalar", "scale"});

                            if (subtype == "constant") texture = SpectrumTexture::makeConstant(parseSpectrum<Float>("value", stream, validator, registry));
                            else if (subtype == "scale") {
                                SpectrumTexture * texture1 = parseSpectrumTexture("texture1", stream, validator, registry);
                                SpectrumTexture * texture2 = parseSpectrumTexture("texture2", stream, validator, registry);

                                texture = SpectrumTexture::makeScale(texture1, texture2);
                            }
                            else if (subtype == "mix") {
                                SpectrumTexture * texture1 = parseSpectrumTexture("texture1", stream, validator, registry);
                                SpectrumTexture * texture2 = parseSpectrumTexture("texture2", stream, validator, registry);
                                ScalarTexture * factor = parseScalarTexture("factor", stream, validator, registry);

                                validator.atLeast("factor", factor->getMin(), Float(0));
                                validator.atMost("factor", factor->getMax(), Float(1));

                                texture = SpectrumTexture::makeMix(texture1, texture2, factor);
                            }
                            else if (subtype == "checker") {
                                DenseSpectrum<Float> * value1 = parseSpectrum<Float>("value1", stream, validator, registry);
                                DenseSpectrum<Float> * value2 = parseSpectrum<Float>("value2", stream, validator, registry);

                                Float uScale = stream.next<Float>("uScale");

                                validator.nonZero("uScale", uScale);

                                Float vScale = stream.next<Float>("vScale");

                                validator.nonZero("vScale", vScale);

                                Float uOffset = stream.next<Float>("uOffset");
                                Float vOffset = stream.next<Float>("vOffset");

                                texture = SpectrumTexture::makeChecker(value1, value2, uScale, vScale, uOffset, vOffset);
                            }
                            else if (subtype == "scalar") {
                                ScalarTexture * scalarTexture = parseScalarTexture("texture", stream, validator, registry);

                                texture = SpectrumTexture::makeScalar(scalarTexture);
                            }
                            else if (subtype == "image") {
                                std::string file = stream.next<std::string>("file");

                                Float * pointer;
                                ColorSpace space;
                                int width, height, channels;

                                if (!registry.hasImage(file, pointer, space, width, height, channels) || channels != 3) {
                                    std::vector<Float> image = parseImage(file, 3, space, width, height);

                                    pointer = registry.addImage(image, space, width, height, 3, file);
                                }

                                Float uScale = stream.next<Float>("uScale");

                                validator.nonZero("uScale", uScale);

                                Float vScale = stream.next<Float>("vScale");

                                validator.nonZero("vScale", vScale);

                                Float uOffset = stream.next<Float>("uOffset");
                                Float vOffset = stream.next<Float>("vOffset");

                                texture = SpectrumTexture::makeImage(pointer, space, width, height, uScale, vScale, uOffset, vOffset);
                            }

                            registry.addSpectrumTexture(texture, name);
                        }
                    }
                    else if (command == "Medium") {
                        Medium medium;

                        std::string name = stream.next<std::string>("name");

                        validator.notDefined<Medium>("name", registry, name);

                        std::string type = stream.next<std::string>("type");

                        validator.expected("type", type, {"homogeneous"});

                        if (type == "homogeneous") {
                            DenseSpectrum<Float> * sigmaA = parseSpectrum<Float>("sigmaA", stream, validator, registry);

                            validator.nonNegative("sigmaA", sigmaA->min());

                            DenseSpectrum<Float> * sigmaS = parseSpectrum<Float>("sigmaS", stream, validator, registry);

                            validator.nonNegative("sigmaS", sigmaS->min());

                            Float g = stream.next<Float>("g");

                            validator.inRangeExclusive("g", g, Float(-1), Float(1));

                            medium = Medium::makeHomogeneous(sigmaA, sigmaS, g);
                        }

                        registry.addMedium(medium, name);
                    }
                    else if (command == "Material") {
                        Material material;

                        std::string name = stream.next<std::string>("name");

                        validator.notDefined<Material>("name", registry, name);

                        std::string type = stream.next<std::string>("type");

                        validator.expected("type", type, {"coated", "conductor", "dielectric", "emissive", "lambertian", "mirror"});

                        if (type == "lambertian" || type == "mirror") {
                            SpectrumTexture * albedo = parseSpectrumTexture("albedo", stream, validator, registry);

                            validator.atLeast("albedo", albedo->getMin(), Float(0));
                            validator.atMost("albedo", albedo->getMax(), Float(1));

                            if (type == "lambertian") material = Material::makeLambertian(albedo);
                            else if (type == "mirror") material = Material::makeMirror(albedo);
                        }
                        else if (type == "conductor") {
                            DenseSpectrum<Float> * n0 = parseSpectrum<Float>("n0", stream, validator, registry);

                            for (int i = 0; i < constants::CIE_LAMBDA_BINS; i++) validator.positive("n0", (*n0)[i]);

                            DenseSpectrum<Complex> * n1 = parseSpectrum<Complex>("n1", stream, validator, registry);

                            for (int i = 0; i < constants::CIE_LAMBDA_BINS; i++) {
                                validator.positive("n1", (*n1)[i].real());
                                validator.positive("n1", (*n1)[i].imag());
                            }

                            ScalarTexture * uAlpha = parseScalarTexture("uAlpha", stream, validator, registry);

                            Float uAlphaMin = uAlpha->getMin();

                            validator.atLeast("uAlpha", uAlphaMin, Float(0));
                            validator.atMost("uAlpha", uAlpha->getMax(), Float(1));

                            ScalarTexture * vAlpha = parseScalarTexture("vAlpha", stream, validator, registry);

                            Float vAlphaMin = vAlpha->getMin();

                            if (uAlphaMin == 0) validator.zero("vAlpha", vAlphaMin);
                            else validator.greaterThan("vAlpha", vAlphaMin, Float(0));

                            validator.atMost("vAlpha", vAlpha->getMax(), Float(1));

                            material = Material::makeConductor(n0, n1, uAlpha, vAlpha);
                        }
                        else if (type == "dielectric") {
                            DenseSpectrum<Float> * n0 = parseSpectrum<Float>("n0", stream, validator, registry);

                            for (int i = 0; i < constants::CIE_LAMBDA_BINS; i++) validator.positive("n0", (*n0)[i]);

                            DenseSpectrum<Float> * n1 = parseSpectrum<Float>("n1", stream, validator, registry);

                            for (int i = 0; i < constants::CIE_LAMBDA_BINS; i++) validator.positive("n1", (*n1)[i]);

                            ScalarTexture * uAlpha = parseScalarTexture("uAlpha", stream, validator, registry);

                            Float uAlphaMin = uAlpha->getMin();

                            validator.atLeast("uAlpha", uAlphaMin, Float(0));
                            validator.atMost("uAlpha", uAlpha->getMax(), Float(1));

                            ScalarTexture * vAlpha = parseScalarTexture("vAlpha", stream, validator, registry);

                            Float vAlphaMin = vAlpha->getMin();

                            validator.atLeast("vAlpha", vAlphaMin, Float(0));
                            validator.atMost("vAlpha", vAlpha->getMax(), Float(1));

                            if (type == "dielectric") material = Material::makeDielectric(n0, n1, uAlpha, vAlpha);
                        }
                        else if (type == "coated") {
                            std::vector<DenseSpectrum<Complex> *> n = parseArray<DenseSpectrum<Complex> *>("n", stream, validator, registry);

                            validator.validate(std::ssize(n) >= 1, "'n' must have at least 1 entry, got " + std::to_string(std::ssize(n)));

                            for (int i = 0; i < std::ssize(n); i++)
                                for (int j = 0; j < constants::CIE_LAMBDA_BINS; j++) {
                                    validator.positive("n", (*(n[i]))[j].real());
                                    validator.nonNegative("n", (*(n[i]))[j].imag());
                                }

                            DenseSpectrum<Complex> ** nPointer = context.addMaterialProperty(n.data(), int(std::ssize(n)));

                            std::vector<ScalarTexture *> d = parseArray<ScalarTexture *>("d", stream, validator, registry);

                            validator.validate(std::ssize(d) == std::ssize(n), "'d' must have " + std::to_string(std::ssize(n)) + " entries, got " + std::to_string(std::ssize(d)));

                            for (int i = 0; i < std::ssize(d); i++) validator.positive("d", d[i]->getMin());

                            ScalarTexture ** dPointer = context.addMaterialProperty(d.data(), int(std::ssize(d)));

                            std::string substrateName = stream.next<std::string>("substrate");

                            Material * substrate;

                            validator.defined<Material>("substrate", registry, substrateName, substrate);

                            validator.validate(substrate->isIdealFresnel(), "'substrate' must be \"conductor\" or \"dielectric\"");

                            material = Material::makeCoated(int(std::ssize(n)), nPointer, dPointer, substrate);
                        }
                        else if (type == "emissive") {
                            SpectrumTexture * emission = parseSpectrumTexture("emission", stream, validator, registry);

                            material = Material::makeEmissive(emission);
                        }

                        registry.addMaterial(material, name);
                    }
                    else if (command == "Object") {
                        Object object;

                        std::string name = stream.next<std::string>("name");

                        int index;

                        validator.validate(!registry.has<Object>(name, index) || index == context.getObjectIndex(), "'name' is already defined");

                        std::string type = stream.next<std::string>("type");

                        validator.expected("type", type, {"cylinder", "disk", "patch", "sphere", "tri"});

                        std::string materialName = stream.next<std::string>("material");

                        Material * material = nullptr;

                        if (materialName != "none") validator.defined<Material>("material", registry, materialName, material);

                        std::string medium0Name = stream.next<std::string>("medium0");

                        Medium * medium0 = nullptr;

                        if (medium0Name != "none") validator.defined<Medium>("medium0", registry, medium0Name, medium0);

                        std::string medium1Name = stream.next<std::string>("medium1");

                        Medium * medium1 = nullptr;

                        if (medium1Name != "none") validator.defined<Medium>("medium1", registry, medium1Name, medium1);

                        if (type == "sphere") {
                            Float radius = stream.next<Float>("radius");

                            validator.positive("radius", radius);

                            registry.addObject(Object::makeSphere(material, medium0, medium1, radius), name);
                        }
                        else if (type == "disk") {
                            Float radius = stream.next<Float>("radius");

                            validator.positive("radius", radius);

                            registry.addObject(Object::makeDisk(material, medium0, medium1, radius), name);
                        }
                        else if (type == "cylinder") {
                            Float radius = stream.next<Float>("radius");

                            validator.positive("radius", radius);

                            Float height = stream.next<Float>("height");

                            validator.positive("height", height);

                            registry.addObject(Object::makeCylinder(material, medium0, medium1, radius, height), name);
                        }
                        else if (type == "tri") {
                            std::vector<Vector<Float, 3>> vertices, normals;
                            std::vector<Vector<Float, 2>> textureCoordinates;
                            std::vector<int> faces;

                            if (stream.hasNext<std::string>()) {
                                std::string file = stream.next<std::string>("file");

                                for (const Vector<int, 3> & face : parseMesh<3>(file, vertices, normals, textureCoordinates)) {
                                    faces.push_back(face[0]);
                                    faces.push_back(face[1]);
                                    faces.push_back(face[2]);
                                }
                            }
                            else {
                                vertices = parseArray<Vector<Float, 3>>("vertices", stream, validator, registry);
                                normals = parseArray<Vector<Float, 3>>("normals", stream, validator, registry);

                                validator.validate(std::ssize(normals) == 0 || std::ssize(normals) == std::ssize(vertices), "'normals' must have " + std::to_string(std::ssize(vertices)) + " entries, got " + std::to_string(std::ssize(normals)));

                                textureCoordinates = parseArray<Vector<Float, 2>>("textureCoordinates", stream, validator, registry);

                                validator.validate(std::ssize(textureCoordinates) == 0 || std::ssize(textureCoordinates) == std::ssize(vertices), "'textureCoordinates' must have " + std::to_string(std::ssize(vertices)) + " entries, got " + std::to_string(std::ssize(textureCoordinates)));

                                for (const Vector<int, 3> & face : parseArray<Vector<int, 3>>("faces", stream, validator, registry)) {
                                    validator.inRange("faces", face[0], 0, int(std::ssize(vertices)) - 1);
                                    validator.inRange("faces", face[1], 0, int(std::ssize(vertices)) - 1);
                                    validator.inRange("faces", face[2], 0, int(std::ssize(vertices)) - 1);

                                    validator.validate(face[0] != face[1] && face[1] != face[2] && face[2] != face[0], "'faces' must be non-degenerate");

                                    faces.push_back(face[0]);
                                    faces.push_back(face[1]);
                                    faces.push_back(face[2]);
                                }
                            }

                            Vector<Float, 3> * verticesPointer = context.addMeshVertices(vertices.data(), int(std::ssize(vertices)));
                            Vector<Float, 3> * normalsPointer = std::ssize(normals) > 0 ? context.addMeshNormals(normals.data(), int(std::ssize(normals))) : nullptr;
                            Vector<Float, 2> * textureCoordinatesPointer = std::ssize(textureCoordinates) > 0 ? context.addMeshTextureCoordinates(textureCoordinates.data(), int(std::ssize(textureCoordinates))) : nullptr;
                            int * facesPointer = context.addMeshFaces(faces.data(), int(std::ssize(faces)));

                            Mesh * mesh = context.addMesh(Mesh::makeTri(verticesPointer, normalsPointer, textureCoordinatesPointer, facesPointer));

                            std::vector<Object> objects;

                            for (int i = 0; i < std::ssize(faces) / 3; i++)
                                objects.push_back(Object::makeTri(material, medium0, medium1, mesh, i));

                            registry.addObjects(objects, name);
                        }
                        else if (type == "patch") {
                            std::vector<Vector<Float, 3>> vertices, normals;
                            std::vector<Vector<Float, 2>> textureCoordinates;
                            std::vector<int> faces;

                            if (stream.hasNext<std::string>()) {
                                std::string file = stream.next<std::string>("file");

                                for (const Vector<int, 4> & face : parseMesh<4>(file, vertices, normals, textureCoordinates)) {
                                    faces.push_back(face[0]);
                                    faces.push_back(face[1]);
                                    faces.push_back(face[2]);
                                    faces.push_back(face[3]);
                                }
                            }
                            else {
                                vertices = parseArray<Vector<Float, 3>>("vertices", stream, validator, registry);
                                normals = parseArray<Vector<Float, 3>>("normals", stream, validator, registry);

                                validator.validate(std::ssize(normals) == 0 || std::ssize(normals) == std::ssize(vertices), "'normals' must have " + std::to_string(std::ssize(vertices)) + " entries, got " + std::to_string(std::ssize(normals)));

                                textureCoordinates = parseArray<Vector<Float, 2>>("textureCoordinates", stream, validator, registry);

                                validator.validate(std::ssize(textureCoordinates) == 0 || std::ssize(textureCoordinates) == std::ssize(vertices), "'textureCoordinates' must have " + std::to_string(std::ssize(vertices)) + " entries, got " + std::to_string(std::ssize(textureCoordinates)));

                                for (const Vector<int, 4> & face : parseArray<Vector<int, 4>>("faces", stream, validator, registry)) {
                                    validator.inRange("faces", face[0], 0, int(std::ssize(vertices)) - 1);
                                    validator.inRange("faces", face[1], 0, int(std::ssize(vertices)) - 1);
                                    validator.inRange("faces", face[2], 0, int(std::ssize(vertices)) - 1);
                                    validator.inRange("faces", face[3], 0, int(std::ssize(vertices)) - 1);

                                    validator.validate(face[0] != face[1] && face[0] != face[2] && face[0] != face[3] && face[1] != face[2] && face[1] != face[3] && face[2] != face[3], "'faces' must be non-degenerate");

                                    faces.push_back(face[0]);
                                    faces.push_back(face[1]);
                                    faces.push_back(face[2]);
                                    faces.push_back(face[3]);
                                }
                            }

                            Vector<Float, 3> * verticesPointer = context.addMeshVertices(vertices.data(), int(std::ssize(vertices)));
                            Vector<Float, 3> * normalsPointer = std::ssize(normals) > 0 ? context.addMeshNormals(normals.data(), int(std::ssize(normals))) : nullptr;
                            Vector<Float, 2> * textureCoordinatesPointer = std::ssize(textureCoordinates) > 0 ? context.addMeshTextureCoordinates(textureCoordinates.data(), int(std::ssize(textureCoordinates))) : nullptr;
                            int * facesPointer = context.addMeshFaces(faces.data(), int(std::ssize(faces)));

                            Mesh * mesh = context.addMesh(Mesh::makeQuad(verticesPointer, normalsPointer, textureCoordinatesPointer, facesPointer));

                            std::vector<Object> objects;

                            for (int i = 0; i < std::ssize(faces) / 4; i++)
                                objects.push_back(Object::makePatch(material, medium0, medium1, mesh, i));

                            registry.addObjects(objects, name);
                        }
                    }
                    else if (command == "Instance") {
                        std::string objectName = stream.next<std::string>("object");

                        int index;

                        validator.validate(registry.has<Object>(objectName, index), "'object' is not defined");

                        if (index == context.getObjectIndex()) context.flushObjects();

                        int count = context.getObjectCount(index);

                        std::string materialName = stream.next<std::string>("material");

                        Material * material = nullptr;

                        if (materialName != "none") validator.defined<Material>("material", registry, materialName, material);

                        std::string medium0Name = stream.next<std::string>("medium0");

                        Medium * medium0 = nullptr;

                        if (medium0Name != "none") validator.defined<Medium>("medium0", registry, medium0Name, medium0);

                        std::string medium1Name = stream.next<std::string>("medium1");

                        Medium * medium1 = nullptr;

                        if (medium1Name != "none") validator.defined<Medium>("medium1", registry, medium1Name, medium1);

                        Vector<Float, 3> translation = stream.next<Vector<Float, 3>>("translation");
                        Vector<Float, 3> rotation = stream.next<Vector<Float, 3>>("rotation");
                        Vector<Float, 3> scale = stream.next<Vector<Float, 3>>("scale");

                        validator.nonZero("scale", scale[0]);
                        validator.nonZero("scale", scale[1]);
                        validator.nonZero("scale", scale[2]);

                        Instance instance(context.getObject(context.getOffset(index)), count, material, medium0, medium1, translation, rotation, scale);

                        if (scale[0] != scale[1] || scale[1] != scale[2])
                            for (int i = 0; i < count; i++) validator.validate(!instance.getMaterial(i) || !instance.getMaterial(i)->isEmissive(), "'scale' must be uniform for emissive objects");

                        context.addInstance(instance);
                    }
                    else if (command == "Background") {
                        Background background;

                        std::string type = stream.next<std::string>("type");

                        validator.expected("type", type, {"equirectangular", "octahedral"});

                        SpectrumTexture * emission = parseSpectrumTexture("emission", stream, validator, registry);

                        Vector<Float, 3> rotation = stream.next<Vector<Float, 3>>("rotation");

                        if (type == "equirectangular") background = Background::makeEquirectangular(emission, rotation);
                        else if (type == "octahedral") background = Background::makeOctahedral(emission, rotation);

                        context.setBackground(background);
                    }
                    else if (command == "Camera") {
                        Camera camera;

                        std::string type = stream.next<std::string>("type");

                        validator.expected("type", type, {"orthographic", "perspective", "spherical"});

                        std::string mediumName = stream.next<std::string>("medium");

                        Medium * medium = nullptr;

                        if (mediumName != "none") validator.defined<Medium>("medium", registry, mediumName, medium);

                        Vector<Float, 3> position = stream.next<Vector<Float, 3>>("position");
                        Vector<Float, 3> lookAt = stream.next<Vector<Float, 3>>("lookAt");

                        validator.nonZero("lookAt' - 'position", (lookAt - position).length());

                        Vector<Float, 3> up = stream.next<Vector<Float, 3>>("up");

                        validator.nonZero("up", up.length());
                        validator.validate(cross(lookAt - position, up).length() > constants::EPSILON_SQUARED, "'lookAt' - 'position' and 'up' must be non-parallel");

                        if (type == "perspective") {
                            Float fov = stream.next<Float>("fov");

                            validator.inRangeExclusive("fov", fov, Float(0), constants::PI);

                            camera = Camera::makePerspective(medium, position, lookAt, up, fov, Float(context.getRenderer()->getWidth()) / Float(context.getRenderer()->getHeight()));
                        }
                        else if (type == "orthographic") {
                            Float height = stream.next<Float>("height");

                            validator.positive("height", height);

                            camera = Camera::makeOrthographic(medium, position, lookAt, up, height, Float(context.getRenderer()->getWidth()) / Float(context.getRenderer()->getHeight()));
                        }
                        else if (type == "spherical") camera = Camera::makeSpherical(medium, position, lookAt, up);

                        context.setCamera(camera);
                    }

                    validator.unexpected(stream.peek<char>());
                }

                validator.validate(render, "expected \"Render\"");

                inputFile.close();
            }

        private:
            static std::string stripComments(const std::string & s) {
                size_t commentPos = s.find('#');

                return commentPos == std::string::npos ? s : s.substr(0, commentPos);
            }

            template <typename T>
            static T parseBinary(std::ifstream & inputFile, const std::string & input, const std::string & parameter, bool littleEndian) {
                T value;

                inputFile.read(reinterpret_cast<char *>(&value), sizeof(T));

                if (!inputFile) error::fileError(input, "'" + parameter + "' is missing or invalid");

                if ((std::endian::native == std::endian::big && littleEndian) || (std::endian::native == std::endian::little && !littleEndian)) {
                    unsigned char * pointer = reinterpret_cast<unsigned char *>(&value);

                    std::reverse(pointer, pointer + sizeof(T));
                }

                return value;
            }

            template <typename T>
            static std::vector<T> parseArray(const std::string & parameter, ParserStream & stream, const ParserValidator & validator, ParserRegistry & registry) {
                validator.valid(parameter, stream.hasNext<char>('['));

                std::vector<T> values;

                while (stream.hasNext<char>()) {
                    if (stream.hasNext<char>(']')) return values;

                    if constexpr (std::is_same_v<T, DenseSpectrum<Float> *>) values.push_back(parseSpectrum<Float>(parameter, stream, validator, registry));
                    else if constexpr (std::is_same_v<T, DenseSpectrum<Complex> *>) values.push_back(parseSpectrum<Complex>(parameter, stream, validator, registry));
                    else if constexpr (std::is_same_v<T, ScalarTexture *>) values.push_back(parseScalarTexture(parameter, stream, validator, registry));
                    else if constexpr (std::is_same_v<T, SpectrumTexture *>) values.push_back(parseSpectrumTexture(parameter, stream, validator, registry));
                    else values.push_back(stream.next<T>(parameter));
                }

                validator.valid(parameter);
            }


            static ScalarTexture * parseScalarTexture(const std::string & parameter, ParserStream & stream, const ParserValidator & validator, ParserRegistry & registry) {
                if (stream.hasNext<Float>()) return registry.addScalarTexture(ScalarTexture::makeConstant(stream.next<Float>()));

                ScalarTexture * texture;

                validator.defined<ScalarTexture>(parameter, registry, stream.next<std::string>(parameter), texture);

                return texture;
            }

            static SpectrumTexture * parseSpectrumTexture(const std::string & parameter, ParserStream & stream, const ParserValidator & validator, ParserRegistry & registry) {
                if (stream.hasNext<Float>() || stream.peek<char>() == '(' || utils::hasExtension(stream.peek<std::string>(), ".spd")) return registry.addSpectrumTexture(SpectrumTexture::makeConstant(parseSpectrum<Float>(parameter, stream, validator, registry)));
                else {
                    std::string name = stream.next<std::string>(parameter);

                    SpectrumTexture * texture;

                    if (registry.has<SpectrumTexture>(name, texture)) return texture;

                    ScalarTexture * scalarTexture;

                    validator.defined<ScalarTexture>(parameter, registry, name, scalarTexture);

                    return registry.addSpectrumTexture(SpectrumTexture::makeScalar(scalarTexture));
                }
            }

            template <typename T>
            static DenseSpectrum<T> * parseSpectrum(const std::string & parameter, ParserStream & stream, const ParserValidator & validator, ParserRegistry & registry) {
                if (stream.hasNext<T>()) return registry.addSpectrum(DenseSpectrum<T>(stream.next<T>(parameter)));

                std::string name;

                std::map<double, T> samples;

                if (utils::hasExtension(stream.peek<std::string>(), ".spd")) {
                    std::string file = stream.next<std::string>(parameter);

                    DenseSpectrum<T> * pointer;

                    if (registry.has<DenseSpectrum<T>>(file, pointer)) return pointer;

                    samples = parseSPD<T>(file);

                    name = file;
                }
                else samples = stream.next<std::map<double, T>>(parameter);

                validator.valid("samples", !samples.empty());

                DenseSpectrum<T> spectrum;

                auto iterator = samples.begin();

                double sampleMin = iterator->first;
                double sampleMax = samples.rbegin()->first;

                for (int i = 0; i < constants::CIE_LAMBDA_BINS; i++) {
                    if (i <= sampleMin - constants::CIE_LAMBDA_MIN || std::ssize(samples) == 1) spectrum[i] = samples[sampleMin];
                    else if (i >= sampleMax - constants::CIE_LAMBDA_MIN) spectrum[i] = samples[sampleMax];
                    else {
                        double lambda = i + constants::CIE_LAMBDA_MIN;

                        while (std::next(iterator) != samples.end() && std::next(iterator)->first < lambda) iterator++;

                        double min = iterator->first;
                        double max = std::next(iterator)->first;

                        spectrum[i] = T((max - lambda) / (max - min) * samples[min] + (lambda - min) / (max - min) * samples[max]);
                    }
                }

                return registry.addSpectrum(spectrum, name);
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

                Float rx = utils::chromaticity(ColorSpace::SRGB, 0);
                Float ry = utils::chromaticity(ColorSpace::SRGB, 1);
                Float gx = utils::chromaticity(ColorSpace::SRGB, 2);
                Float gy = utils::chromaticity(ColorSpace::SRGB, 3);
                Float bx = utils::chromaticity(ColorSpace::SRGB, 4);
                Float by = utils::chromaticity(ColorSpace::SRGB, 5);
                Float wx = utils::chromaticity(ColorSpace::SRGB, 6);
                Float wy = utils::chromaticity(ColorSpace::SRGB, 7);

                if (isEXR) {
                    EXRVersion version;

                    if (ParseEXRVersionFromFile(&version, input.c_str()) == TINYEXR_SUCCESS) {
                        EXRHeader header;

                        InitEXRHeader(&header);

                        if (ParseEXRHeaderFromFile(&header, &version, input.c_str(), &errorEXR) == TINYEXR_SUCCESS) {
                            for (int i = 0; i < header.num_custom_attributes; i++) {
                                if (std::strcmp(header.custom_attributes[i].name, "chromaticities") == 0 && header.custom_attributes[i].size == sizeof(float) * 8) {
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
                        }

                        FreeEXRHeader(&header);
                        FreeEXRErrorMessage(errorEXR);
                    }

                    if (LoadEXR(&dataEXR, &width, &height, input.c_str(), &errorEXR) != TINYEXR_SUCCESS) {
                        std::string error = errorEXR ? " (" + std::string(errorEXR) + ")" : "";

                        FreeEXRErrorMessage(errorEXR);

                        error::fileError(input, "failed to open file" + error);
                    }
                }
                else if (is32Bit) data32 = stbi_loadf(input.c_str(), &width, &height, &nativeChannels, channels);
                else if (is16Bit) data16 = stbi_load_16(input.c_str(), &width, &height, &nativeChannels, channels);
                else data8 = stbi_load(input.c_str(), &width, &height, &nativeChannels, channels);

                if (!data8 && !data16 && !data32 && !dataEXR) {
                    const char * failureReason = stbi_failure_reason();

                    std::string error = failureReason ? " (" + std::string(failureReason) + ")" : "";

                    if (error == " (unknown image type)") error::fileError(input, "invalid file extension");
                    else error::fileError(input, "failed to open file" + error);
                }

                space = ColorSpace::SRGB;

                Matrix<double, 3> colorTransform;

                bool needsTransform = false;

                if (channels == 3) {
                    if (utils::colorSpaceContains(ColorSpace::SRGB, rx, ry, gx, gy, bx, by, wx, wy)) space = ColorSpace::SRGB;
                    else if (utils::colorSpaceContains(ColorSpace::REC2020, rx, ry, gx, gy, bx, by, wx, wy)) space = ColorSpace::REC2020;
                    else space = ColorSpace::ACES2065;

                    needsTransform = std::fabs(rx - utils::chromaticity(space, 0)) > constants::EPSILON_SQUARED || std::fabs(ry - utils::chromaticity(space, 1)) > constants::EPSILON_SQUARED || std::fabs(gx - utils::chromaticity(space, 2)) > constants::EPSILON_SQUARED || std::fabs(gy - utils::chromaticity(space, 3)) > constants::EPSILON_SQUARED || std::fabs(bx - utils::chromaticity(space, 4)) > constants::EPSILON_SQUARED || std::fabs(by - utils::chromaticity(space, 5)) > constants::EPSILON_SQUARED || std::fabs(wx - utils::chromaticity(space, 6)) > constants::EPSILON_SQUARED || std::fabs(wy - utils::chromaticity(space, 7)) > constants::EPSILON_SQUARED;

                    if (needsTransform) colorTransform = utils::toRGBMatrix(space) * utils::bradfordAdapt(wx, wy, utils::chromaticity(space, 6), utils::chromaticity(space, 7)) * utils::toXYZMatrix(rx, ry, gx, gy, bx, by, wx, wy);
                }

                int totalElements = width * height * channels;

                std::vector<Float> image(totalElements);

                Vector<Float, 3> color;

                for (int i = 0, j = 0; i < totalElements; i += channels, j += 4) {
                    for (int k = 0; k < channels; k++) {
                        color[k] = isEXR ? (std::isfinite(dataEXR[j + k]) ? Float(dataEXR[j + k]) : Float(0)) : is32Bit ? (std::isfinite(data32[i + k]) ? Float(data32[i + k]) : Float(0)) : is16Bit ? Float(data16[i + k]) / Float(65535) : Float(data8[i + k]) / Float(255);

                        if (sRGB) color[k] = utils::sRGBToLinear(color[k]);
                    }

                    if (needsTransform && channels == 3) color = colorTransform * color;

                    for (int k = 0; k < channels; k++) image[i + k] = color[k];
                }

                if (data8) stbi_image_free(data8);
                if (data16) stbi_image_free(data16);
                if (data32) stbi_image_free(data32);
                if (dataEXR) free(dataEXR);

                return image;
            }

            template <int N>
            static std::vector<Vector<int, N>> parseMesh(const std::string & input, std::vector<Vector<Float, 3>> & meshVertices, std::vector<Vector<Float, 3>> & meshNormals, std::vector<Vector<Float, 2>> & meshTextureCoordinates) {
                std::ifstream inputFile(input, std::ios::in | std::ios::binary);

                if (!inputFile.is_open()) error::failedToOpenFileError(input);

                std::vector<Vector<Float, 3>> vertices, normals;
                std::vector<Vector<Float, 2>> textureCoordinates;

                bool customNormals = false;
                bool customTextureCoordinates = false;

                std::vector<int> vertexIndices, textureIndices, normalIndices;

                std::map<std::tuple<int, int, int>, int> indexMap;

                std::vector<Vector<int, N>> faces;

                std::string line;

                ParserStream stream(input);
                ParserValidator validator(stream);

                if (utils::hasExtension(input, ".obj")) {
                    while (std::getline(inputFile, line)) {
                        stream.load(stripComments(line));

                        if (!stream.hasNext<char>()) continue;

                        std::string tag = stream.next<std::string>("", false);

                        if (tag == "v") {
                            Float x = stream.next<Float>("x");
                            Float y = stream.next<Float>("y");
                            Float z = stream.next<Float>("z");

                            vertices.push_back(Vector<Float, 3>(x, y, z));
                        }
                        else if (tag == "vn") {
                            Float x = stream.next<Float>("x");
                            Float y = stream.next<Float>("y");
                            Float z = stream.next<Float>("z");

                            normals.push_back(Vector<Float, 3>(x, y, z));
                        }
                        else if (tag == "vt") {
                            Float u = stream.next<Float>("u");
                            Float v = stream.next<Float>("v");

                            textureCoordinates.push_back(Vector<Float, 2>(u, v));
                        }
                        else if (tag == "f") {
                            vertexIndices.clear();
                            textureIndices.clear();
                            normalIndices.clear();

                            while (stream.hasNext<char>()) {
                                int vertexIndex = stream.next<int>("v");

                                if (vertexIndex < 0) vertexIndex += int(std::ssize(vertices));
                                else if (vertexIndex > 0) vertexIndex--;

                                validator.inRange("v", vertexIndex + 1, 1, int(std::ssize(vertices)));

                                vertexIndices.push_back(vertexIndex);

                                int textureIndex = -1, normalIndex = -1;

                                stream.setSkipWhitespace(false);

                                if (stream.hasNext<char>('/')) {
                                    if (!stream.hasNext<char>('/')) {
                                        textureIndex = stream.next<int>("vt");

                                        if (textureIndex < 0) textureIndex += int(std::ssize(textureCoordinates));
                                        else if (textureIndex >= 0) textureIndex--;

                                        validator.inRange("vt", textureIndex + 1, 1, int(std::ssize(textureCoordinates)));

                                        stream.hasNext<char>('/');

                                        customTextureCoordinates = true;
                                    }

                                    if (stream.hasNext<char>() && !std::isspace(stream.peek<char>())) {
                                        normalIndex = stream.next<int>("vn");

                                        if (normalIndex < 0) normalIndex += int(std::ssize(normals));
                                        else if (normalIndex >= 0) normalIndex--;

                                        validator.inRange("vn", normalIndex + 1, 1, int(std::ssize(normals)));
                                        validator.valid("vn", !stream.hasNext<char>() || std::isspace(stream.peek<char>()));

                                        customNormals = true;
                                    }
                                }

                                stream.setSkipWhitespace(true);

                                textureIndices.push_back(textureIndex);
                                normalIndices.push_back(normalIndex);
                            }

                            validator.validate(std::ssize(vertexIndices) >= N, "'f' must have at least " + std::to_string(N) + " vertices");
                            validator.validate(std::ssize(vertexIndices) % (N - 2) == 0, "'f' must have a multiple of " + std::to_string(N - 2) + " vertices");

                            for (int i = 0; i < std::ssize(vertexIndices); i++) {
                                auto iterator = indexMap.find(std::make_tuple(vertexIndices[i], textureIndices[i], normalIndices[i]));

                                if (iterator == indexMap.end()) {
                                    indexMap[std::make_tuple(vertexIndices[i], textureIndices[i], normalIndices[i])] = int(std::ssize(meshVertices));

                                    meshVertices.push_back(vertices[vertexIndices[i]]);
                                    meshNormals.push_back(normalIndices[i] >= 0 ? normals[normalIndices[i]] : Vector<Float, 3>(0, 0, 0));
                                    meshTextureCoordinates.push_back(textureIndices[i] >= 0 ? textureCoordinates[textureIndices[i]] : Vector<Float, 2>(0, 0));
                                }
                            }

                            Vector<int, N> face;

                            face[0] = indexMap.at(std::make_tuple(vertexIndices[0], textureIndices[0], normalIndices[0]));

                            for (int i = 1; i < std::ssize(vertexIndices) - N + 2; i += N - 2) {
                                for (int j = 0; j < N - 1; j++) {
                                    face[j + 1] = indexMap.at(std::make_tuple(vertexIndices[i + j], textureIndices[i + j], normalIndices[i + j]));

                                    for (int k = 0; k < j; k++) validator.validate(vertexIndices[i + k] != vertexIndices[i + j], "'f' must be non-degenerate");
                                }

                                faces.push_back(face);
                            }
                        }
                        else continue;

                        validator.unexpected(stream.peek<char>());
                    }

                    if (!customNormals) meshNormals.clear();
                    if (!customTextureCoordinates) meshTextureCoordinates.clear();
                }
                else if (utils::hasExtension(input, ".ply")) {
                    bool ply = false;
                    bool format = false;

                    std::string formatType;

                    std::vector<std::string> elementNames;
                    std::vector<int> elementCounts;
                    std::vector<std::vector<std::tuple<std::string, bool, std::string, std::string>>> elementProperties;

                    while (std::getline(inputFile, line)) {
                        stream.load(line);

                        std::string tag = stream.next<std::string>("", false);

                        if (!ply) validator.expected("", tag, {"ply"});
                        else if (!format) validator.expected("", tag, {"format"});
                        else validator.expected("", tag, {"comment", "element", "end_header", "obj_info", "property"});

                        if (tag == "ply") ply = true;
                        else if (tag == "format") {
                            formatType = stream.next<std::string>("type");

                            validator.expected("type", formatType, {"ascii", "binary_little_endian", "binary_big_endian"});

                            Float version = stream.next<Float>("version");

                            validator.expected("version", std::to_string(version), {std::to_string(Float(1.0))});

                            format = true;
                        }
                        else if (tag == "element") {
                            std::string name = stream.next<std::string>("name");

                            int count = stream.next<int>("count");

                            validator.nonNegative("count", count);

                            elementNames.push_back(name);
                            elementCounts.push_back(count);
                            elementProperties.push_back(std::vector<std::tuple<std::string, bool, std::string, std::string>>());

                            if (name == "vertex") meshVertices.reserve(meshVertices.size() + count);
                            else if (name == "face") faces.reserve(faces.size() + count);
                        }
                        else if (tag == "property") {
                            std::string type = stream.next<std::string>("type");

                            validator.expected("type", type, {"char", "int8", "uchar", "uint8", "short", "int16", "ushort", "uint16", "int", "int32", "uint", "uint32", "float", "float32", "double", "float64", "list"});

                            if (type != "list") {
                                std::string name = stream.next<std::string>("name");

                                if (elementNames.back() == "vertex" && (name == "nx" || name == "ny" || name == "nz")) {
                                    customNormals = true;

                                    meshNormals.reserve(meshVertices.size());
                                }
                                else if (elementNames.back() == "vertex" && (name == "s" || name == "t" || name == "u" || name == "v")) {
                                    customTextureCoordinates = true;

                                    meshTextureCoordinates.reserve(meshVertices.size());
                                }

                                elementProperties.back().push_back(std::make_tuple(name, false, type, ""));
                            }
                            else {
                                std::string countType = stream.next<std::string>("countType");

                                validator.expected("countType", countType, {"char", "int8", "uchar", "uint8", "short", "int16", "ushort", "uint16", "int", "int32", "uint", "uint32"});

                                std::string valueType = stream.next<std::string>("valueType");

                                validator.expected("valueType", valueType, {"char", "int8", "uchar", "uint8", "short", "int16", "ushort", "uint16", "int", "int32", "uint", "uint32", "float", "float32", "double", "float64"});

                                std::string name = stream.next<std::string>("name");

                                elementProperties.back().push_back(std::make_tuple(name, true, countType, valueType));
                            }
                        }
                        else if (tag == "comment" || tag == "obj_info") continue;
                        else if (tag == "end_header") break;
                    }

                    validator.validate(ply, "expected \"ply\"");
                    validator.validate(format, "expected \"format\"");

                    for (int i = 0; i < std::ssize(elementNames); i++) {
                        for (int j = 0; j < elementCounts[i]; j++) {
                            if (formatType == "ascii") {
                                std::getline(inputFile, line);

                                stream.load(line);
                            }

                            if (elementNames[i] == "vertex") {
                                Vector<Float, 3> vertex, normal;
                                Vector<Float, 2> textureCoordinate;

                                for (const std::tuple<std::string, bool, std::string, std::string> & property : elementProperties[i]) {
                                    std::string name = std::get<0>(property);
                                    bool isList = std::get<1>(property);
                                    std::string countType = std::get<2>(property);
                                    std::string valueType = std::get<3>(property);

                                    if (!isList) {
                                        Float value = parsePLYValue<Float>(inputFile, input, stream, formatType, name, countType);

                                        if (name == "x") vertex[0] = value;
                                        else if (name == "y") vertex[1] = value;
                                        else if (name == "z") vertex[2] = value;
                                        else if (name == "nx") normal[0] = value;
                                        else if (name == "ny") normal[1] = value;
                                        else if (name == "nz") normal[2] = value;
                                        else if (name == "s" || name == "u") textureCoordinate[0] = value;
                                        else if (name == "t" || name == "v") textureCoordinate[1] = value;
                                    }
                                    else {
                                        int count = parsePLYValue<int>(inputFile, input, stream, formatType, name, countType);

                                        for (int k = 0; k < count; k++) parsePLYValue<Float>(inputFile, input, stream, formatType, name, valueType);
                                    }
                                }

                                meshVertices.push_back(vertex);
                                if (customNormals) meshNormals.push_back(normal);
                                if (customTextureCoordinates) meshTextureCoordinates.push_back(textureCoordinate);
                            }
                            else if (elementNames[i] == "face") {
                                for (const std::tuple<std::string, bool, std::string, std::string> & property : elementProperties[i]) {
                                    std::string name = std::get<0>(property);
                                    bool isList = std::get<1>(property);
                                    std::string countType = std::get<2>(property);
                                    std::string valueType = std::get<3>(property);

                                    vertexIndices.clear();

                                    if (!isList) parsePLYValue<Float>(inputFile, input, stream, formatType, name, countType);
                                    else {
                                        int count = parsePLYValue<int>(inputFile, input, stream, formatType, name, countType);

                                        for (int k = 0; k < count; k++) vertexIndices.push_back(parsePLYValue<int>(inputFile, input, stream, formatType, name, valueType));

                                        if (name == "vertex_index" || name == "vertex_indices") {
                                            validator.validate(count >= N, "'" + name + "' must have at least " + std::to_string(N) + " entries");
                                            validator.validate(count % (N - 2) == 0, "'" + name + "' must have a multiple of " + std::to_string(N - 2) + " entries");

                                            for (int k = 0; k < std::ssize(vertexIndices); k++) validator.inRange(name, vertexIndices[k], 0, int(std::ssize(meshVertices)) - 1);

                                            Vector<int, N> face;

                                            face[0] = vertexIndices[0];

                                            for (int k = 1; k < std::ssize(vertexIndices) - N + 2; k += N - 2) {
                                                for (int l = 0; l < N - 1; l++) {
                                                    face[l + 1] = vertexIndices[k + l];

                                                    for (int m = 0; m < l; m++) validator.validate(vertexIndices[k + m] != vertexIndices[k + l], "'" + name + "' must be non-degenerate");
                                                }

                                                faces.push_back(face);
                                            }
                                        }
                                    }
                                }
                            }
                            else {
                                for (const std::tuple<std::string, bool, std::string, std::string> & property : elementProperties[i]) {
                                    std::string name = std::get<0>(property);
                                    bool isList = std::get<1>(property);
                                    std::string countType = std::get<2>(property);
                                    std::string valueType = std::get<3>(property);

                                    if (!isList) parsePLYValue<Float>(inputFile, input, stream, formatType, name, countType);
                                    else {
                                        int count = parsePLYValue<int>(inputFile, input, stream, formatType, name, countType);

                                        for (int k = 0; k < count; k++) parsePLYValue<Float>(inputFile, input, stream, formatType, name, valueType);
                                    }
                                }
                            }
                        }
                    }
                }
                else error::fileError(input, "invalid file extension");

                inputFile.close();

                return faces;
            }

            template <typename T>
            static T parsePLYValue(std::ifstream & inputFile, const std::string & input, ParserStream & stream, const std::string & formatType, const std::string & name, const std::string & type) {
                if (formatType == "ascii") {
                    if (type == "char" || type == "int8" || type == "short" || type == "int16" || type == "int" || type == "int32") return T(stream.next<int>(name));
                    else if (type == "uchar" || type == "uint8" || type == "ushort" || type == "uint16" || type == "uint" || type == "uint32") return T(stream.next<unsigned int>(name));
                    else if (type == "float" || type == "float32") return T(stream.next<float>(name));
                    else if (type == "double" || type == "float64") return T(stream.next<double>(name));
                }
                else {
                    bool littleEndian = formatType == "binary_little_endian";

                    if (type == "char" || type == "int8") return T(parseBinary<int8_t>(inputFile, input, name, littleEndian));
                    else if (type == "uchar" || type == "uint8") return T(parseBinary<uint8_t>(inputFile, input, name, littleEndian));
                    else if (type == "short" || type == "int16") return T(parseBinary<int16_t>(inputFile, input, name, littleEndian));
                    else if (type == "ushort" || type == "uint16") return T(parseBinary<uint16_t>(inputFile, input, name, littleEndian));
                    else if (type == "int" || type == "int32") return T(parseBinary<int32_t>(inputFile, input, name, littleEndian));
                    else if (type == "uint" || type == "uint32") return T(parseBinary<uint32_t>(inputFile, input, name, littleEndian));
                    else if (type == "float" || type == "float32") return T(parseBinary<float>(inputFile, input, name, littleEndian));
                    else if (type == "double" || type == "float64") return T(parseBinary<double>(inputFile, input, name, littleEndian));
                }

                return T();
            }

            template <typename T>
            static std::map<double, T> parseSPD(const std::string & input) {
                std::ifstream inputFile(input);

                if (!inputFile.is_open()) error::failedToOpenFileError(input);

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
}