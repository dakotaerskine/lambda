#pragma once

using Float = double;

#ifdef __CUDACC__
    #define HOST_DEVICE __host__ __device__
    #include <thrust/complex.h>
    using Complex = thrust::complex<double>;
#else
    #define HOST_DEVICE
    #include <complex>
    using Complex = std::complex<double>;
#endif