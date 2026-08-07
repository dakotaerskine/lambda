#include <cctype>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include <cuda_runtime.h>

#include "background.h"
#include "camera.h"
#include "constants.h"
#include "config.h"
#include "material.h"
#include "object.h"
#include "parser.h"
#include "random.h"
#include "renderer.h"
#include "scene.h"
#include "spectrum.h"
#include "texture.h"

static void checkCudaError(cudaError_t err, const std::string & message) {
    if (err != cudaSuccess) {
        std::string error = cudaGetErrorString(err);
        error[0] = std::tolower(error[0]);
        throw std::runtime_error(message + " (" + error + ")");
    }
}

__global__ void renderKernel(Float * __restrict__ outputBuffer, int * __restrict__ d_completed, Config config, Camera camera, Scene scene, uint64_t seed) {
    int px = blockIdx.x * blockDim.x + threadIdx.x;
    int py = blockIdx.y * blockDim.y + threadIdx.y;

    if (px >= config.width || py >= config.height) return;

    Random state(seed, py * config.width + px);

    Renderer::renderPixel(px, py, outputBuffer, config, camera, scene, state);

    atomicAdd(d_completed, 1);
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

    try {
        checkCudaError(cudaMemcpyToSymbol(d_PERMUTATION, PERMUTATION, 256 * sizeof(int)), "failed to copy 'PERMUTATION' to device memory");
        checkCudaError(cudaMemcpyToSymbol(d_CIE_XYZ_1931, CIE_XYZ_1931, 471 * 3 * sizeof(double)), "failed to copy 'CIE_XYZ_1931' to device memory");
        checkCudaError(cudaMemcpyToSymbol(d_CIE_D65, CIE_D65, 531 * sizeof(double)), "failed to copy 'CIE_D65' to device memory");

        DenseSpectrum<Float> * d_spectra;

        checkCudaError(cudaMalloc(&d_spectra, spectra.size() * sizeof(DenseSpectrum<Float>)), "failed to allocate device memory for 'd_spectra'");
        checkCudaError(cudaMemcpy(d_spectra, spectra.data(), spectra.size() * sizeof(DenseSpectrum<Float>), cudaMemcpyHostToDevice), "failed to copy 'd_spectra' to device memory");

        DenseSpectrum<Complex> * d_complexSpectra;

        checkCudaError(cudaMalloc(&d_complexSpectra, complexSpectra.size() * sizeof(DenseSpectrum<Complex>)), "failed to allocate device memory for 'd_complexSpectra'");
        checkCudaError(cudaMemcpy(d_complexSpectra, complexSpectra.data(), complexSpectra.size() * sizeof(DenseSpectrum<Complex>), cudaMemcpyHostToDevice), "failed to copy 'd_complexSpectra' to device memory");

        Background * d_background;

        checkCudaError(cudaMalloc(&d_background, sizeof(Background)), "failed to allocate device memory for 'd_background'");
        checkCudaError(cudaMemcpy(d_background, &background, sizeof(Background), cudaMemcpyHostToDevice), "failed to copy 'd_background' to device memory");

        Object * d_objects;

        checkCudaError(cudaMalloc(&d_objects, objects.size() * sizeof(Object)), "failed to allocate device memory for 'd_objects'");
        checkCudaError(cudaMemcpy(d_objects, objects.data(), objects.size() * sizeof(Object), cudaMemcpyHostToDevice), "failed to copy 'd_objects' to device memory");

        Material * d_materials;

        checkCudaError(cudaMalloc(&d_materials, materials.size() * sizeof(Material)), "failed to allocate device memory for 'd_materials'");
        checkCudaError(cudaMemcpy(d_materials, materials.data(), materials.size() * sizeof(Material), cudaMemcpyHostToDevice), "failed to copy 'd_materials' to device memory");

        int * d_materialProperties;

        checkCudaError(cudaMalloc(&d_materialProperties, materialProperties.size() * sizeof(int)), "failed to allocate device memory for 'd_materialProperties'");
        checkCudaError(cudaMemcpy(d_materialProperties, materialProperties.data(), materialProperties.size() * sizeof(int), cudaMemcpyHostToDevice), "failed to copy 'd_materialProperties' to device memory");

        ScalarTexture * d_scalarTextures;

        checkCudaError(cudaMalloc(&d_scalarTextures, scalarTextures.size() * sizeof(ScalarTexture)), "failed to allocate device memory for 'd_scalarTextures'");
        checkCudaError(cudaMemcpy(d_scalarTextures, scalarTextures.data(), scalarTextures.size() * sizeof(ScalarTexture), cudaMemcpyHostToDevice), "failed to copy 'd_scalarTextures' to device memory");

        SpectrumTexture * d_spectrumTextures;

        checkCudaError(cudaMalloc(&d_spectrumTextures, spectrumTextures.size() * sizeof(SpectrumTexture)), "failed to allocate device memory for 'd_spectrumTextures'");
        checkCudaError(cudaMemcpy(d_spectrumTextures, spectrumTextures.data(), spectrumTextures.size() * sizeof(SpectrumTexture), cudaMemcpyHostToDevice), "failed to copy 'd_spectrumTextures' to device memory");

        Float * d_output;

        checkCudaError(cudaMalloc(&d_output, config.totalPixels * 3 * sizeof(Float)), "failed to allocate device memory for 'd_output'");

        int * d_completed;

        checkCudaError(cudaMallocManaged(&d_completed, sizeof(int)), "failed to allocate device memory for 'd_completed'");
        checkCudaError(cudaMemset(d_completed, 0, sizeof(int)), "failed to initialize 'd_completed' to zero");

        int BLOCK_W = 16;
        int BLOCK_H = 16;

        dim3 block(BLOCK_W, BLOCK_H);
        dim3 grid((config.width + BLOCK_W - 1) / BLOCK_W, (config.height + BLOCK_H - 1) / BLOCK_H);

        renderKernel<<<grid, block>>>(d_output, d_completed, config, camera, Scene(d_spectra, d_complexSpectra, d_background, d_objects, objects.size(), d_materials, d_materialProperties, d_scalarTextures, d_spectrumTextures), 42ULL);

        checkCudaError(cudaGetLastError(), "failed to launch kernel");

        int completed = 0;

        while (completed < config.totalPixels) {
            completed = *(volatile int *)d_completed;

            std::cerr << "\r" << argv[0] << ": " << int(100.0 * completed / config.totalPixels) << "% complete" << std::flush;

            cudaError_t err = cudaPeekAtLastError();

            if (err != cudaSuccess) {
                std::cerr << std::endl;
                checkCudaError(err, "failed to execute kernel");
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        checkCudaError(cudaDeviceSynchronize(), "failed to synchronize device");

        std::cerr << "\r" << argv[0] << ": 100% complete" << std::flush << std::endl;

        checkCudaError(cudaMemcpy(h_output.data(), d_output, config.totalPixels * 3 * sizeof(Float), cudaMemcpyDeviceToHost), "failed to copy 'd_output' to host memory");

        checkCudaError(cudaFree(d_spectra), "failed to free device memory for 'd_spectra'");
        checkCudaError(cudaFree(d_complexSpectra), "failed to free device memory for 'd_complexSpectra'");
        checkCudaError(cudaFree(d_background), "failed to free device memory for 'd_background'");
        checkCudaError(cudaFree(d_objects), "failed to free device memory for 'd_objects'");
        checkCudaError(cudaFree(d_materials), "failed to free device memory for 'd_materials'");
        checkCudaError(cudaFree(d_materialProperties), "failed to free device memory for 'd_materialProperties'");
        checkCudaError(cudaFree(d_scalarTextures), "failed to free device memory for 'd_scalarTextures'");
        checkCudaError(cudaFree(d_spectrumTextures), "failed to free device memory for 'd_spectrumTextures'");
        checkCudaError(cudaFree(d_output), "failed to free device memory for 'd_output'");
        checkCudaError(cudaFree(d_completed), "failed to free device memory for 'd_completed'");
    }
    catch (const std::exception & e) {
        std::cerr << argv[0] << ": " << e.what() << std::endl;
        return 1;
    }

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
