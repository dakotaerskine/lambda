#pragma once

#include <vector>

#include "utils.h"

#ifdef __CUDACC__
    template <typename T>
    class DeviceBuffer {
        public:
            DeviceBuffer() : pointer(nullptr) {}

            DeviceBuffer(const T * h_data_begin, const T * h_data_end) {
                int n = h_data_end - h_data_begin;
                checkCudaError(cudaMallocManaged(&pointer, n * sizeof(T)), "failed to allocate device memory");
                checkCudaError(cudaMemcpy(pointer, h_data_begin, n * sizeof(T), cudaMemcpyHostToDevice), "failed to copy to device memory");
            }

            DeviceBuffer(const std::vector<T> & h_data) {
                checkCudaError(cudaMallocManaged(&pointer, h_data.size() * sizeof(T)), "failed to allocate device memory");
                checkCudaError(cudaMemcpy(pointer, h_data.data(), h_data.size() * sizeof(T), cudaMemcpyHostToDevice), "failed to copy to device memory");
            }

            DeviceBuffer(int n) {
                checkCudaError(cudaMallocManaged(&pointer, n * sizeof(T)), "failed to allocate device memory");
                checkCudaError(cudaMemset(pointer, 0, n * sizeof(T)), "failed to initialize device memory");
            }

            ~DeviceBuffer() {
                if (pointer) checkCudaError(cudaFree(pointer), "failed to free device memory");
            }

            T * data() const { return pointer; }

        private:
            T * pointer;
    };

    template <typename T>
    using Buffer = DeviceBuffer<T>;
#else
    template <typename T>
    using Buffer = std::vector<T>;
#endif