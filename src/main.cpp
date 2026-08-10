#include <algorithm>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <vector>

#ifdef USE_OPENMP
    #include <omp.h>
#endif

#include "core/buffer.h"
#include "core/constants.h"
#include "core/parser.h"
#include "core/platform.h"
#include "core/utils.h"
#include "math/random.h"
#include "math/spectrum.h"
#include "render/renderer.h"
#include "scene/background.h"
#include "scene/material.h"
#include "scene/object.h"
#include "scene/scene.h"
#include "scene/texture.h"

int main(int argc, char * argv[]) {
    if (argc != 2) {
        std::cerr << argv[0] << ": invalid number of arguments" << std::endl;
        return 1;
    }

    std::string input(argv[1]);

    if (input.length() < 4 || input.rfind(".lrd") != input.length() - 4) {
        std::cerr << input << ": invalid file extension" << std::endl;
        return 1;
    }

    Renderer renderer;
    std::vector<DenseSpectrum<Float>> spectra;
    std::vector<DenseSpectrum<Complex>> complexSpectra;
    Background background;
    std::vector<Object> objects;
    std::vector<int> lights;
    std::vector<Float> lightPowers;
    Float totalLightPower = 0;
    std::vector<Material> materials;
    std::vector<int> materialProperties;
    std::vector<ScalarTexture> scalarTextures;
    std::vector<SpectrumTexture> spectrumTextures;

    try {
        Parser::parseFile(input, renderer, spectra, complexSpectra, background, objects, lights, lightPowers, totalLightPower, materials, materialProperties, scalarTextures, spectrumTextures);
    }
    catch (const std::exception & e) {
        std::cerr << input << e.what() << std::endl;
        return 1;
    }

    std::vector<Float> h_output(renderer.getTotalPixels() * 3);

    try {
        Buffer<int> d_PERMUTATION(&h_PERMUTATION[0], &h_PERMUTATION[PERMUTATION_SIZE - 1] + 1);
        Buffer<double> d_CIE_XYZ_1931(&h_CIE_XYZ_1931[0][0], &h_CIE_XYZ_1931[CIE_LAMBDA_BINS - 1][2] + 1);
        Buffer<double> d_CIE_D65(&h_CIE_D65[0], &h_CIE_D65[CIE_D65_LAMBDA_BINS - 1] + 1);

        PERMUTATION = d_PERMUTATION.data();
        CIE_XYZ_1931 = d_CIE_XYZ_1931.data();
        CIE_D65 = d_CIE_D65.data();

        Buffer<DenseSpectrum<Float>> d_spectra(spectra);
        Buffer<DenseSpectrum<Complex>> d_complexSpectra(complexSpectra);
        Buffer<Object> d_objects(objects);
        Buffer<int> d_lights(lights);
        Buffer<Float> d_lightPowers(lightPowers);
        Buffer<Material> d_materials(materials);
        Buffer<int> d_materialProperties(materialProperties);
        Buffer<ScalarTexture> d_scalarTextures(scalarTextures);
        Buffer<SpectrumTexture> d_spectrumTextures(spectrumTextures);

        renderer.setScene(d_spectra.data(), d_complexSpectra.data(), background, d_objects.data(), objects.size(), d_lights.data(), lights.size(), d_lightPowers.data(), totalLightPower, d_materials.data(), d_materialProperties.data(), d_scalarTextures.data(), d_spectrumTextures.data());

        Buffer<Float> d_output(h_output.size());

        renderer.setOutputBuffer(d_output.data());

        Buffer<int> d_completed(1);

        std::cerr << "\r" << argv[0] << ": 0% complete" << std::flush;

        #ifdef __CUDACC__
            int BLOCK_W = 16;
            int BLOCK_H = 16;

            dim3 block(BLOCK_W, BLOCK_H);
            dim3 grid((renderer.getWidth() + BLOCK_W - 1) / BLOCK_W, (renderer.getHeight() + BLOCK_H - 1) / BLOCK_H);

            renderKernel<<<grid, block>>>(d_completed.data(), renderer, 42ULL);

            checkCudaError(cudaGetLastError(), "failed to launch kernel");
        #else
            std::thread thread(&Renderer::renderImage, &renderer, d_completed.data(), 42ULL);
        #endif

        int completed = 0;

        while (completed < renderer.getTotalPixels()) {
            #ifdef __CUDACC__
                checkCudaError(cudaPeekAtLastError(), "\nfailed to execute kernel");
            #endif

            completed = *(volatile int *)d_completed.data();

            #ifdef USE_OPENMP
                #pragma omp critical
            #endif
            {
                std::cerr << "\r" << argv[0] << ": " << int(100.0 * completed / renderer.getTotalPixels()) << "% complete" << std::flush;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        #ifdef __CUDACC__
            checkCudaError(cudaDeviceSynchronize(), "failed to synchronize device");

            checkCudaError(cudaMemcpy(h_output.data(), d_output.data(), renderer.getTotalPixels() * 3 * sizeof(Float), cudaMemcpyDeviceToHost), "failed to copy to host memory");
        #else
            thread.join();

            h_output = d_output;
        #endif

        std::cerr << "\r" << argv[0] << ": 100% complete" << std::flush << std::endl;
    }
    catch (const std::exception & e) {
        std::cerr << argv[0] << ": " << e.what() << std::endl;
        return 1;
    }

    std::string output = input;

    output.erase(output.size() - 4);

    output += ".ppm";

    std::ofstream outputFile(output);

    if (!outputFile.is_open()) {
        std::string error = std::strerror(errno);
        error[0] = std::tolower(error[0]);
        std::cerr << output + ": failed to open file (" + error + ")" << std::endl;
        return 1;
    }

    outputFile << "P3\n" << renderer.getWidth() << " " << renderer.getHeight() << "\n255\n";

    std::mt19937 generator(0);
    std::uniform_real_distribution<Float> distribution(-0.5, 0.5);

    for (int i = 0; i < renderer.getTotalPixels(); i++) {
        int r = std::clamp((int)std::round(255.0 * h_output[i * 3 + 0] + distribution(generator)), 0, 255);
        int g = std::clamp((int)std::round(255.0 * h_output[i * 3 + 1] + distribution(generator)), 0, 255);
        int b = std::clamp((int)std::round(255.0 * h_output[i * 3 + 2] + distribution(generator)), 0, 255);

        outputFile << r << " " << g << " " << b << "\n";
    }

    outputFile.close();

    return 0;
}