#pragma once

#include <iterator>
#include <vector>

#include "core/constants.h"
#include "core/error.h"
#include "core/utils.h"

namespace lambda {
    template <typename T>
    class Buffer {
        public:
            Buffer() : current(0), total(0) {}

            Buffer(const T * begin, int n) {
                if (n < 0) error::lengthError(n);

                current = 0;
                total = 0;

                if (n > 0) {
                    T * pointer = ensureCapacity(n, true);

                    std::copy(begin, begin + n, pointer);

                    current += n;
                    total += n;
                }
            }

            Buffer(int n) {
                if (n < 0) error::lengthError(n);

                current = 0;
                total = 0;

                if (n > 0) {
                    ensureCapacity(n, true);

                    current += n;
                    total += n;
                }
            }

            Buffer(const Buffer<T> &) = delete;
            Buffer<T> & operator=(const Buffer<T> &) = delete;

            ~Buffer() { for (T * pointer : pointers) free(pointer); }

            void reserve(int n) {
                if (n < 0) error::lengthError(n);
                ensureCapacity(n, true);
            }

            T * extend(int n) {
                if (n < 0) error::lengthError(n);

                if (n == 0) return pointers.empty() ? nullptr : pointers.back() + current;

                T * pointer = ensureCapacity(n);

                current += n;
                total += n;

                return pointer;
            }

            T & operator[](int i) {
                if (i < 0 || i >= total) error::outOfRangeError(i);

                for (int j = 0; j < std::ssize(pointers); j++) {
                    if (i < sizes[j]) return pointers[j][i];

                    i -= sizes[j];
                }

                error::outOfRangeError(i);
            }

            const T & operator[](int i) const {
                if (i < 0 || i >= total) error::outOfRangeError(i);

                for (int j = 0; j < std::ssize(pointers); j++) {
                    if (i < sizes[j]) return pointers[j][i];

                    i -= sizes[j];
                }

                error::outOfRangeError(i);
            }

            T & back() {
                if (total == 0) error::outOfRangeError(0);

                if (current == 0) return pointers[std::ssize(pointers) - 2][sizes[std::ssize(pointers) - 2] - 1];

                return pointers.back()[current - 1];
            }

            const T & back() const {
                if (total == 0) error::outOfRangeError(0);

                if (current == 0) return pointers[std::ssize(pointers) - 2][sizes[std::ssize(pointers) - 2] - 1];

                return pointers.back()[current - 1];
            }

            T * push(const T & value) {
                T * pointer = ensureCapacity(1);

                *pointer = value;

                current++;
                total++;

                return pointer;
            }

            T * push(const T * begin, int n) {
                if (n < 0) error::lengthError(n);

                if (n == 0) return pointers.empty() ? nullptr : pointers.back() + current;

                T * pointer = ensureCapacity(n);

                std::copy(begin, begin + n, pointer);

                current += n;
                total += n;

                return pointer;
            }

            T * data(int i = 0) {
                if (total == 0 && i == 0) return nullptr;

                if (i < 0 || i >= total) error::outOfRangeError(i);

                for (int j = 0; j < std::ssize(pointers); j++) {
                    if (i < sizes[j]) return pointers[j] + i;

                    i -= sizes[j];
                }

                error::outOfRangeError(i);
            }

            int size() const { return total; }

            bool empty() const { return total == 0; }

        private:
            std::vector<T *> pointers;
            std::vector<int> sizes;
            int current, total;

            T * ensureCapacity(int n, bool exact = false) {
                int capacity = sizes.empty() ? 0 : sizes.back();

                if (current + n > capacity) {
                    if (!pointers.empty()) sizes.back() = current;

                    int newSize = exact ? n : std::max(n, constants::CHUNK_SIZE);

                    T * pointer = allocate(newSize);

                    pointers.push_back(pointer);
                    sizes.push_back(newSize);

                    current = 0;
                }

                return pointers.back() + current;
            }

            #ifdef __CUDACC__
                T * allocate(int n) {
                    T * pointer;

                    error::checkCudaError(cudaMallocManaged(&pointer, n * sizeof(T)), "failed to allocate device memory");

                    for (int i = 0; i < n; i++) pointer[i] = T();

                    return pointer;
                }

                void free(T * pointer) { error::checkCudaError(cudaFree(pointer), "failed to free device memory"); }
            #else
                T * allocate(int n) {
                    T * pointer = new T[n]();

                    return pointer;
                }

                void free(T * pointer) { delete [] pointer; }
            #endif
    };
}