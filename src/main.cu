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

#include "camera.h"
#include "constants.h"
#include "config.h"
#include "object.h"
#include "parser.h"
#include "random.h"
#include "renderer.h"
#include "scene.h"
#include "spectrum.h"

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
    Camera camera;
    Spectrum background;
    std::vector<Object> objects;

    try {
        Parser::parseFile(input, config, camera, background, objects);
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

        Object * d_objects;

        checkCudaError(cudaMalloc(&d_objects, objects.size() * sizeof(Object)), "failed to allocate device memory for 'd_objects'");
        checkCudaError(cudaMemcpy(d_objects, objects.data(), objects.size() * sizeof(Object), cudaMemcpyHostToDevice), "failed to copy 'd_objects' to device memory");

        Float * d_output;

        checkCudaError(cudaMalloc(&d_output, config.totalPixels * 3 * sizeof(Float)), "failed to allocate device memory for 'd_output'");

        int * d_completed;

        checkCudaError(cudaMallocManaged(&d_completed, sizeof(int)), "failed to allocate device memory for 'd_completed'");
        checkCudaError(cudaMemset(d_completed, 0, sizeof(int)), "failed to initialize 'd_completed' to zero");

        int BLOCK_W = 16;
        int BLOCK_H = 16;

        dim3 block(BLOCK_W, BLOCK_H);
        dim3 grid((config.width + BLOCK_W - 1) / BLOCK_W, (config.height + BLOCK_H - 1) / BLOCK_H);

        renderKernel<<<grid, block>>>(d_output, d_completed, config, camera, Scene(background, d_objects, objects.size()), 42ULL);

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

        checkCudaError(cudaFree(d_objects), "failed to free device memory for 'd_objects'");
        checkCudaError(cudaFree(d_output), "failed to free device memory for 'd_output'");
        checkCudaError(cudaFree(d_completed), "failed to free device memory for 'd_completed'");
    }
    catch (const std::exception & e) {
        std::cerr << argv[0] << ": " << e.what() << std::endl;
        return 1;
    }

    std::string output = input;

    if (output.length() >= 4 && output.rfind(".lrd") == output.length() - 4) output.erase(output.size() - 4);

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
