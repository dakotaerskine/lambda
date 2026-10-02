#pragma once

#ifdef __CUDACC__
    #define LAMBDA_HOST_DEVICE __host__ __device__
    #define LAMBDA_HOST_DEVICE_NOINLINE LAMBDA_HOST_DEVICE __noinline__
    #define LAMBDA_GLOBAL __global__
    #define LAMBDA_MANAGED __managed__
#else
    #define LAMBDA_HOST_DEVICE
    #define LAMBDA_HOST_DEVICE_NOINLINE
    #define LAMBDA_GLOBAL
    #define LAMBDA_MANAGED
#endif

namespace lambda {
    enum class ColorSpace {SRGB, REC2020, ACES2065};

    #ifdef LAMBDA_DOUBLE_PRECISION
        using Float = double;
    #else
        using Float = float;
    #endif
}