#pragma once

#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

#include "core/buffer.h"
#include "core/error.h"
#include "core/platform.h"
#include "render/renderer.h"

#ifdef __CUDACC__
    GLOBAL void renderKernel(Renderer renderer, int * d_completed) {
        int px = blockIdx.x * blockDim.x + threadIdx.x;
        int py = blockIdx.y * blockDim.y + threadIdx.y;

        if (px >= renderer.width || py >= renderer.height) return;

        Random state(renderer.seed, py * renderer.width + px);

        renderer.renderPixel(px, py, state);

        atomicAdd(d_completed, 1);
    }
#endif

class Dispatcher {
    public:
        static Float dispatchRender(const std::string & program, Renderer & renderer) {
            Buffer<int> d_completed(1);

            auto start = std::chrono::high_resolution_clock::now();

            outputProgress(program, 0, renderer.getTotalPixels());

            std::thread thread = render(renderer, d_completed);

            int completed = 0;

            while (completed < renderer.getTotalPixels()) {
                completed = *(volatile int *)d_completed.data();

                outputProgress(program, completed, renderer.getTotalPixels());

                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }

            synchronize(thread);

            outputProgress(program, renderer.getTotalPixels(), renderer.getTotalPixels());

            std::cout << std::endl;

            auto end = std::chrono::high_resolution_clock::now();

            std::chrono::duration<Float> duration = end - start;

            return duration.count();
        }

    private:
        static void outputProgress(const std::string & program, int completed, int totalPixels) { std::cout << "\r" << program << ": " << int(100.0 * completed / totalPixels) << "% complete" << std::flush; }

        #ifdef __CUDACC__
            static std::thread render(Renderer & renderer, Buffer<int> & d_completed) {
                int BLOCK_W = 16;
                int BLOCK_H = 16;

                dim3 block(BLOCK_W, BLOCK_H);
                dim3 grid((renderer.getWidth() + BLOCK_W - 1) / BLOCK_W, (renderer.getHeight() + BLOCK_H - 1) / BLOCK_H);

                checkCudaError(cudaDeviceSynchronize(), "failed to synchronize device");

                renderKernel<<<grid, block>>>(renderer, d_completed.data());

                checkCudaError(cudaGetLastError(), "failed to launch kernel");

                return std::thread();
            }

            static void synchronize(std::thread &) { checkCudaError(cudaDeviceSynchronize(), "failed to synchronize device"); }
        #else
            static std::thread render(Renderer & renderer, Buffer<int> & d_completed) { return std::thread(&Renderer::renderImage, &renderer, d_completed.data()); }

            static void synchronize(std::thread & thread) { thread.join(); }
        #endif
};