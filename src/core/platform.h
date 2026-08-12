#pragma once

using Float = float;

#ifdef __CUDACC__
    #define HOST_DEVICE __host__ __device__
    #define GLOBAL __global__
    #define MANAGED __managed__
    #include <cuda_runtime.h>
    #include <thrust/complex.h>
    using Complex = thrust::complex<double>;
    HOST_DEVICE inline Float absC(const Complex & c) { return Float(thrust::abs((thrust::complex<double>)c)); }
    HOST_DEVICE inline Complex expC(const Complex & c) { return thrust::exp((thrust::complex<double>)c); }
    HOST_DEVICE inline Complex sqrtC(const Complex & c) { return thrust::sqrt((thrust::complex<double>)c); }
    inline void checkCudaError(cudaError_t err, const std::string & message) {
        if (err != cudaSuccess) {
            std::string error = cudaGetErrorString(err);
            error[0] = char(std::tolower(error[0]));
            throw std::runtime_error(message + " (" + error + ")");
        }
    }
#else
    #define HOST_DEVICE
    #define GLOBAL
    #define MANAGED inline
    #include <cmath>
    #include <complex>
    using Complex = std::complex<double>;
    inline Float absC(const Complex & c) { return Float(std::abs((std::complex<double>)c)); }
    inline Complex expC(const Complex & c) { return std::exp((std::complex<double>)c); }
    inline Complex sqrtC(const Complex & c) { return std::sqrt((std::complex<double>)c); }
#endif

HOST_DEVICE inline Float acosF(Float x) { return Float(sizeof(Float) == sizeof(float) ? acosf(x) : acos(x)); }
HOST_DEVICE inline Float atan2F(Float y, Float x) { return Float(sizeof(Float) == sizeof(float) ? atan2f(y, x) : atan2(y, x)); }
HOST_DEVICE inline Float cosF(Float x) { return Float(sizeof(Float) == sizeof(float) ? cosf(x) : cos(x)); }
HOST_DEVICE inline Float fabsF(Float x) { return Float(sizeof(Float) == sizeof(float) ? fabsf(x) : fabs(x)); }
HOST_DEVICE inline Float floorF(Float x) { return Float(sizeof(Float) == sizeof(float) ? floorf(x) : floor(x)); }
HOST_DEVICE inline Float fmaxF(Float a, Float b) { return Float(sizeof(Float) == sizeof(float) ? fmaxf(a, b) : fmax(a, b)); }
HOST_DEVICE inline Float fminF(Float a, Float b) { return Float(sizeof(Float) == sizeof(float) ? fminf(a, b) : fmin(a, b)); }
HOST_DEVICE inline Float fmodF(Float x, Float y) { return Float(sizeof(Float) == sizeof(float) ? fmodf(x, y) : fmod(x, y)); }
HOST_DEVICE inline Float powF(Float x, Float y) { return Float(sizeof(Float) == sizeof(float) ? powf(x, y) : pow(x, y)); }
HOST_DEVICE inline Float sinF(Float x) { return Float(sizeof(Float) == sizeof(float) ? sinf(x) : sin(x)); }
HOST_DEVICE inline Float sqrtF(Float x) { return Float(sizeof(Float) == sizeof(float) ? sqrtf(x) : sqrt(x)); }