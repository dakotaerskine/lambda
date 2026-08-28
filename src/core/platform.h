#pragma once

enum class ColorSpace {SRGB, REC2020, ACES2065};

#ifdef DOUBLE_PRECISION
    using Float = double;
#else
    using Float = float;
#endif

#ifdef __CUDACC__
    #define HOST_DEVICE __host__ __device__
    #define GLOBAL __global__
    #define MANAGED __managed__
#else
    #define HOST_DEVICE
    #define GLOBAL
    #define MANAGED
#endif