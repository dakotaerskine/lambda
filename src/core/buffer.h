#pragma once

#include <cstring>
#include <vector>

#include "core/utils.h"

#ifdef __CUDACC__
    template <typename T>
    class Buffer {
        public:
            Buffer() : pointer(nullptr) {}

            Buffer(const std::vector<T> & h_data) {
                checkCudaError(cudaMallocManaged(&pointer, h_data.size() * sizeof(T)), "failed to allocate device memory");
                checkCudaError(cudaMemcpy(pointer, h_data.data(), h_data.size() * sizeof(T), cudaMemcpyHostToDevice), "failed to copy to device memory");
            }

            Buffer(const T * h_data_begin, const T * h_data_end) {
                int n = int(h_data_end - h_data_begin);
                checkCudaError(cudaMallocManaged(&pointer, n * sizeof(T)), "failed to allocate device memory");
                checkCudaError(cudaMemcpy(pointer, h_data_begin, n * sizeof(T), cudaMemcpyHostToDevice), "failed to copy to device memory");
            }

            Buffer(long unsigned int n) {
                checkCudaError(cudaMallocManaged(&pointer, n * sizeof(T)), "failed to allocate device memory");
                checkCudaError(cudaMemset(pointer, 0, n * sizeof(T)), "failed to initialize device memory");
            }

            ~Buffer() { if (pointer) checkCudaError(cudaFree(pointer), "failed to free device memory"); }

            Buffer(const Buffer<T> &) = delete;
            Buffer<T> & operator=(const Buffer<T> &) = delete;

            T * data() const { return pointer; }

        private:
            T * pointer;
    };
#else
    template <typename T>
    using Buffer = std::vector<T>;
#endif