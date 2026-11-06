#pragma once

#include <cstddef>
#include <cstdlib>
#include <new>
#include <type_traits>
#include <utility>
#include <vector>

#include "core/error.h"

namespace lambda {
    class Allocator {
        public:
            Allocator() {}

            Allocator(const Allocator &) = delete;

            Allocator & operator=(const Allocator &) = delete;

            ~Allocator() {
                for (void * pointer : hostAllocations) freeHost(pointer);
                for (const std::pair<void *, size_t> & pair : allocations) free(pair.first);
            }

            template <typename T>
            requires (std::is_trivially_destructible_v<T> && alignof(T) <= alignof(std::max_align_t))
            T * makeHost(const T & value) {
                T * pointer = rawHost<T>(1);

                new (pointer) T(value);

                return pointer;
            }

            template <typename T>
            requires (std::is_trivially_destructible_v<T> && alignof(T) <= alignof(std::max_align_t))
            T * make(const T & value) {
                T * pointer = raw<T>(1);

                new (pointer) T(value);

                return pointer;
            }

            template <typename T>
            requires (std::is_trivially_destructible_v<T> && alignof(T) <= alignof(std::max_align_t))
            T * allocate(int n) {
                if (n < 0) throw Error::length(n);

                T * pointer = raw<T>(n);

                for (int i = 0; i < n; i++) new (pointer + i) T();

                return pointer;
            }

            template <typename T>
            requires (std::is_trivially_destructible_v<T> && alignof(T) <= alignof(std::max_align_t))
            T * allocate(const T * begin, int n) {
                if (n < 0) throw Error::length(n);

                T * pointer = raw<T>(n);

                for (int i = 0; i < n; i++) new (pointer + i) T(begin[i]);

                return pointer;
            }

            #ifdef __CUDACC__
                void prefetch() {
                    int device = 0;

                    CudaError::check(cudaGetDevice(&device), "failed to get device");

                    int concurrent = 0;

                    CudaError::check(cudaDeviceGetAttribute(&concurrent, cudaDevAttrConcurrentManagedAccess, device), "failed to get device attribute");

                    if (!concurrent) return;

                    for (const std::pair<void *, size_t> & pair : allocations) CudaError::check(cudaMemPrefetchAsync(pair.first, pair.second, device), "failed to prefetch device memory");
                }
            #else
                void prefetch() {}
            #endif

        private:
            std::vector<void *> hostAllocations;
            std::vector<std::pair<void *, size_t>> allocations;

            #ifdef __CUDACC__
                template <typename T>
                T * rawHost(int n) {
                    if (n < 0) throw Error::length(n);

                    if (n == 0) return nullptr;

                    void * pointer;

                    CudaError::check(cudaMallocHost(&pointer, n * sizeof(T)), "failed to allocate host memory");

                    hostAllocations.push_back(pointer);

                    return (T *)pointer;
                }

                template <typename T>
                T * raw(int n) {
                    if (n < 0) throw Error::length(n);

                    if (n == 0) return nullptr;

                    size_t bytes = n * sizeof(T);

                    void * pointer;

                    CudaError::check(cudaMallocManaged(&pointer, bytes), "failed to allocate device memory");

                    allocations.push_back(std::make_pair(pointer, bytes));

                    return (T *)pointer;
                }

                void free(void * pointer) { CudaError::check(cudaFree(pointer), "failed to free device memory"); }

                void freeHost(void * pointer) { CudaError::check(cudaFreeHost(pointer), "failed to free host memory"); }
            #else
                template <typename T>
                T * rawHost(int n) {
                    if (n < 0) throw Error::length(n);

                    if (n == 0) return nullptr;

                    void * pointer = std::malloc(n * sizeof(T));

                    if (!pointer) throw std::bad_alloc();

                    hostAllocations.push_back(pointer);

                    return (T *)pointer;
                }

                template <typename T>
                T * raw(int n) {
                    if (n < 0) throw Error::length(n);

                    if (n == 0) return nullptr;

                    size_t bytes = n * sizeof(T);

                    void * pointer = std::malloc(bytes);

                    if (!pointer) throw std::bad_alloc();

                    allocations.push_back(std::make_pair(pointer, bytes));

                    return (T *)pointer;
                }

                void free(void * pointer) { std::free(pointer); }

                void freeHost(void * pointer) { free(pointer); }
            #endif
    };
}