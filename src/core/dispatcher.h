#pragma once

#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

#include "core/buffer.h"
#include "core/constants.h"
#include "core/context.h"
#include "core/error.h"
#include "core/platform.h"
#include "render/renderer.h"

namespace lambda {
    #ifdef __CUDACC__
        LAMBDA_GLOBAL void renderKernel(Renderer * renderer, std::atomic<int> & completed) {
            int px = blockIdx.x * blockDim.x + threadIdx.x;
            int py = blockIdx.y * blockDim.y + threadIdx.y;

            if (px >= renderer->width || py >= renderer->height) return;

            Random state(renderer->seed, py * renderer->width + px);

            renderer->renderPixel(px, py, state);

            atomicAdd(reinterpret_cast<int *>(&completed), 1);
        }
    #endif

    class Dispatcher {
        public:
            static Float dispatchRender(const std::string & program, Renderer * renderer) {
                auto start = std::chrono::high_resolution_clock::now();

                int totalPixels = renderer->getTotalPixels();

                outputProgress(program, 0, totalPixels);

                std::atomic<int> completed(0);

                std::thread thread = render(renderer, completed);

                while (completed < totalPixels) {
                    outputProgress(program, completed, totalPixels);

                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }

                synchronize(thread);

                outputProgress(program, totalPixels, totalPixels);

                std::cout << std::endl;

                auto end = std::chrono::high_resolution_clock::now();

                std::chrono::duration<Float> duration = end - start;

                return duration.count();
            }

        private:
            static void outputProgress(const std::string & program, const std::atomic<int> & completed, int totalPixels) { std::cout << "\r" << program << ": " << int(100.0 * completed.load() / totalPixels) << "% complete" << std::flush; }

            #ifdef __CUDACC__
                static std::thread render(Renderer * renderer, std::atomic<int> & completed) {
                    dim3 block(constants::BLOCK_W, constants::BLOCK_H);
                    dim3 grid((renderer->getWidth() + constants::BLOCK_W - 1) / constants::BLOCK_W, (renderer->getHeight() + constants::BLOCK_H - 1) / constants::BLOCK_H);

                    error::checkCudaError(cudaDeviceSetLimit(cudaLimitStackSize, constants::STACK_SIZE), "failed to set stack size");
                    error::checkCudaError(cudaDeviceSynchronize(), "failed to synchronize device");

                    renderKernel<<<grid, block>>>(renderer, completed);

                    error::checkCudaError(cudaGetLastError(), "failed to launch kernel");

                    return std::thread();
                }

                static void synchronize(std::thread &) { error::checkCudaError(cudaDeviceSynchronize(), "failed to synchronize device"); }
            #else
                static std::thread render(Renderer * renderer, std::atomic<int> & completed) { return std::thread(&Renderer::renderImage, renderer, std::ref(completed)); }

                static void synchronize(std::thread & thread) { thread.join(); }
            #endif
    };
}