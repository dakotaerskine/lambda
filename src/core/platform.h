#pragma once

using Float = double;

#ifdef __CUDACC__
    #define HOST_DEVICE __host__ __device__
    #define GLOBAL __global__
    #define MANAGED __managed__
    #include <cuda_runtime.h>
    #include <thrust/complex.h>
    using Complex = thrust::complex<double>;
    HOST_DEVICE inline Float absoluteValue(const Complex & c) { return thrust::abs((thrust::complex<double>)c); }
    HOST_DEVICE inline Complex exponential(const Complex & c) { return thrust::exp((thrust::complex<double>)c); }
    HOST_DEVICE inline Complex squareRoot(const Complex & c) { return thrust::sqrt((thrust::complex<double>)c); }
    inline void checkCudaError(cudaError_t err, const std::string & message) {
        if (err != cudaSuccess) {
            std::string error = cudaGetErrorString(err);
            error[0] = std::tolower(error[0]);
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
    inline Float absoluteValue(const Complex & c) { return std::abs((std::complex<double>)c); }
    inline Complex exponential(const Complex & c) { return std::exp((std::complex<double>)c); }
    inline Complex squareRoot(const Complex & c) { return std::sqrt((std::complex<double>)c); }
#endif