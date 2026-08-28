#pragma once

#include <vector>

#include "core/error.h"
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

            Buffer(const T * h_data_begin, size_t n) {
                checkCudaError(cudaMallocManaged(&pointer, n * sizeof(T)), "failed to allocate device memory");
                checkCudaError(cudaMemcpy(pointer, h_data_begin, n * sizeof(T), cudaMemcpyHostToDevice), "failed to copy to device memory");
            }

            Buffer(size_t n) {
                checkCudaError(cudaMallocManaged(&pointer, n * sizeof(T)), "failed to allocate device memory");
                checkCudaError(cudaMemset(pointer, 0, n * sizeof(T)), "failed to initialize device memory");
            }

            ~Buffer() { if (pointer) checkCudaError(cudaFree(pointer), "failed to free device memory"); }

            Buffer(const Buffer<T> &) = delete;
            Buffer<T> & operator=(const Buffer<T> &) = delete;

            Buffer<T> & operator=(Buffer<T> && other) {
                if (this != &other) {
                    if (pointer) checkCudaError(cudaFree(pointer), "failed to free device memory");

                    pointer = other.pointer;
                    other.pointer = nullptr;
                }

                return *this;
            }

            T * data() { return pointer; }

        private:
            T * pointer;
    };
#else
    template <typename T>
    class Buffer {
        public:
            Buffer() {}

            Buffer(const std::vector<T> & h_data) : vector(h_data) {}

            Buffer(const T * h_data_begin, size_t n) : vector(h_data_begin, h_data_begin + n) {}

            Buffer(size_t n) : vector(n) {}

            Buffer(const Buffer<T> &) = delete;
            Buffer<T> & operator=(const Buffer<T> &) = delete;

            Buffer<T> & operator=(Buffer<T> && other) {
                if (this != &other) vector = std::move(other.vector);

                return *this;
            }

            T * data() { return vector.data(); }

        private:
            std::vector<T> vector;
    };
#endif