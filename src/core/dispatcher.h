#pragma once

#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

#include "core/allocator.h"
#include "core/constants.h"
#include "core/error.h"
#include "core/platform.h"
#include "render/integrator.h"

namespace lambda {
    #ifdef __CUDACC__
        LAMBDA_GLOBAL void renderKernel(Integrator * integrator, int * completed) {
            int px = blockIdx.x * blockDim.x + threadIdx.x;
            int py = blockIdx.y * blockDim.y + threadIdx.y;

            if (px >= integrator->film->getWidth() || py >= integrator->film->getHeight()) return;

            Random state(integrator->seed, py * integrator->film->getWidth() + px);

            integrator->renderPixel(px, py, state);

            atomicAdd_system(completed, 1);
        }
    #endif

    class Dispatcher {
        public:
            static Float dispatchRender(const std::string & program, Allocator & allocator, Integrator * integrator) {
                auto start = std::chrono::high_resolution_clock::now();

                int totalPixels = integrator->getFilm()->getWidth() * integrator->getFilm()->getHeight();

                allocator.prefetch();

                outputProgress(program, 0, totalPixels);

                int * completed = allocator.makeHost(0);

                std::thread thread = render(integrator, completed);

                int current = 0;

                while (current < totalPixels) {
                    current = std::atomic_ref<int>(*completed).load(std::memory_order_relaxed);

                    outputProgress(program, current, totalPixels);

                    if (!running()) break;

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
            static void outputProgress(const std::string & program, int completed, int totalPixels) { std::cout << "\r" << program << ": " << int(100.0 * completed / totalPixels) << "% complete" << std::flush; }

            #ifdef __CUDACC__
                static std::thread render(Integrator * integrator, int * completed) {
                    dim3 block(constants::BLOCK_W, constants::BLOCK_H);
                    dim3 grid((integrator->getFilm()->getWidth() + constants::BLOCK_W - 1) / constants::BLOCK_W, (integrator->getFilm()->getHeight() + constants::BLOCK_H - 1) / constants::BLOCK_H);

                    error::checkCudaError(cudaDeviceSetLimit(cudaLimitStackSize, constants::STACK_SIZE), "failed to set stack size");
                    error::checkCudaError(cudaDeviceSynchronize(), "failed to synchronize device");

                    renderKernel<<<grid, block>>>(integrator, completed);

                    error::checkCudaError(cudaGetLastError(), "failed to launch kernel");

                    return std::thread();
                }

                static bool running() { return cudaStreamQuery(0) == cudaErrorNotReady; }

                static void synchronize(std::thread &) { error::checkCudaError(cudaDeviceSynchronize(), "failed to synchronize device"); }
            #else
                static std::thread render(Integrator * integrator, int * completed) { return std::thread(&Integrator::renderImage, integrator, completed); }

                static bool running() { return true; }

                static void synchronize(std::thread & thread) { thread.join(); }
            #endif
    };
}