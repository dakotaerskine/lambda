#include <atomic>
#include <cctype>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#ifdef USE_OPENMP
    #include <omp.h>
#endif

#include "background.h"
#include "camera.h"
#include "config.h"
#include "material.h"
#include "object.h"
#include "parser.h"
#include "random.h"
#include "renderer.h"
#include "scene.h"
#include "spectrum.h"
#include "texture.h"

void renderCPU(const std::string & arg, Float * outputBuffer, const Config & config, const Camera & camera, const Scene & scene, uint64_t seed) {
    std::atomic<int> completed(0);

    std::cerr << "\r" << arg << ": 0% complete" << std::flush;

    #ifdef USE_OPENMP
        #pragma omp parallel for schedule(guided)
    #endif

    for (int py = 0; py < config.height; py++) {
        for (int px = 0; px < config.width; px++) {
            Random state(seed, py * config.width + px);

            Renderer::renderPixel(px, py, outputBuffer, config, camera, scene, state);
        }

        completed += config.width;

        #ifdef USE_OPENMP
            #pragma omp critical
        #endif
        {
            std::cerr << "\r" << arg << ": " << int(100.0 * completed / config.totalPixels) << "% complete" << std::flush;
        }
    }

    std::cerr << "\r" << arg << ": 100% complete" << std::flush << std::endl;
}

int main(int argc, char * argv[]) {
    if (argc != 2) {
        std::cerr << argv[0] << ": missing input file" << std::endl;
        return 1;
    }

    std::string input(argv[1]);

    Config config;
    std::vector<DenseSpectrum<Float>> spectra;
    std::vector<DenseSpectrum<Complex>> complexSpectra;
    Background background;
    Camera camera;
    std::vector<Object> objects;
    std::vector<Material> materials;
    std::vector<int> materialProperties;
    std::vector<ScalarTexture> scalarTextures;
    std::vector<SpectrumTexture> spectrumTextures;

    try {
        Parser::parseFile(input, config, spectra, complexSpectra, background, camera, objects, materials, materialProperties, scalarTextures, spectrumTextures);
    }
    catch (const std::exception & e) {
        std::cerr << input << e.what() << std::endl;
        return 1;
    }

    std::vector<Float> h_output(config.totalPixels * 3);

    renderCPU(argv[0], h_output.data(), config, camera, Scene(spectra.data(), complexSpectra.data(), &background, objects.data(), objects.size(), materials.data(), materialProperties.data(), scalarTextures.data(), spectrumTextures.data()), 42ULL);

    std::string output = input;

    if (output.length() >= 4 && output.rfind(".lrd") == output.length() - 4) output.erase(output.size() - 4);
    else {
        std::cerr << input << ": invalid input file extension (expected \".lrd\")" << std::endl;
        return 1;
    }

    output += ".ppm";

    std::ofstream outputFile(output);

    if (!outputFile.is_open()) {
        std::string error = std::strerror(errno);
        error[0] = std::tolower(error[0]);
        std::cerr << output + ": failed to open file (" + error + ")" << std::endl;
        return 1;
    }

    outputFile << "P3\n" << config.width << " " << config.height << "\n255\n";

    for (int i = 0; i < config.totalPixels; i++)
        outputFile << (int)(255.999 * h_output[i * 3 + 0]) << " " << (int)(255.999 * h_output[i * 3 + 1]) << " " << (int)(255.999 * h_output[i * 3 + 2]) << "\n";

    outputFile.close();

    return 0;
}