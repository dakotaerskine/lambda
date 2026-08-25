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

    template <typename T>
    class ConstantBuffer {
        public:
            ConstantBuffer() : pointer(nullptr), size(0) {}

            ConstantBuffer(const T * h_data, size_t _size) {
                size = _size;

                checkCudaError(cudaMallocManaged(&pointer, size * sizeof(T)), "failed to allocate device memory");
                checkCudaError(cudaMemcpy(pointer, h_data, size * sizeof(T), cudaMemcpyHostToDevice), "failed to copy to device memory");
            }

            ~ConstantBuffer() { if (pointer) checkCudaError(cudaFree(pointer), "failed to free device memory"); }

            ConstantBuffer(const ConstantBuffer &) = delete;
            ConstantBuffer & operator=(const ConstantBuffer &) = delete;

            void copy(void * destination) const { checkCudaError(cudaMemcpyToSymbol(destination, pointer, size * sizeof(T), 0, cudaMemcpyDeviceToDevice), "failed to copy to device memory"); }

        private:
            T * pointer;
            size_t size;
    };
#else
    template <typename T>
    using Buffer = std::vector<T>;

    template <typename T>
    class ConstantBuffer {
        public:
            ConstantBuffer() : pointer(nullptr), size(0) {}

            ConstantBuffer(const T * h_data, size_t _size) {
                size = _size;

                pointer = new T[size];

                std::memcpy(pointer, h_data, size * sizeof(T));
            }

            ~ConstantBuffer() { if (pointer) delete [] pointer; }

            ConstantBuffer(const ConstantBuffer &) = delete;
            ConstantBuffer & operator=(const ConstantBuffer &) = delete;

            void copy(void * destination) const { std::memcpy(destination, pointer, size * sizeof(T)); }

        private:
            T * pointer;
            size_t size;
    };
#endif