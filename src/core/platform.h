#pragma once

#ifndef LAMBDA_DATA_DIRECTORY
    #define LAMBDA_DATA_DIRECTORY "data"
#endif

#ifndef LAMBDA_DOUBLE_PRECISION
    #define LAMBDA_DOUBLE_PRECISION 0
#endif

#ifndef LAMBDA_UPSAMPLING
    #define LAMBDA_UPSAMPLING 1
#endif

#ifdef __CUDACC__
    #define LAMBDA_HOST_DEVICE __host__ __device__
    #define LAMBDA_DEVICE __device__
    #define LAMBDA_DEVICE_NOINLINE LAMBDA_DEVICE __noinline__
    #define LAMBDA_GLOBAL __global__
    #define LAMBDA_CONSTANT __constant__
#else
    #define LAMBDA_HOST_DEVICE
    #define LAMBDA_DEVICE
    #define LAMBDA_DEVICE_NOINLINE
    #define LAMBDA_GLOBAL
    #define LAMBDA_CONSTANT
#endif

namespace lambda {
    #if LAMBDA_DOUBLE_PRECISION
        using Float = double;
    #else
        using Float = float;
    #endif
}