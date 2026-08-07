cmake_minimum_required(VERSION 3.18)

project(pathtracer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

add_executable(pathtracer_cpu main.cpp)

target_compile_options(pathtracer_cpu PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:
        -Wall
        -Wextra
        -Wpedantic
    >
)

find_package(OpenMP REQUIRED)
target_link_libraries(pathtracer_cpu PRIVATE OpenMP::OpenMP_CXX)
target_compile_definitions(pathtracer_cpu PRIVATE USE_OPENMP)

include(CheckLanguage)
check_language(CUDA)

if(CMAKE_CUDA_COMPILER)
    enable_language(CUDA)

    set(CMAKE_CUDA_STANDARD 17)
    set(CMAKE_CUDA_SEPARABLE_COMPILATION ON)
    set(CMAKE_CUDA_ARCHITECTURES native)

    add_executable(pathtracer_gpu main.cu)

    target_compile_options(pathtracer_gpu PRIVATE
        $<$<COMPILE_LANGUAGE:CUDA>:
            -lineinfo
            -Xcompiler=-Wall
            -Xcompiler=-Wextra
        >
    )

    target_link_options(pathtracer_gpu PRIVATE
        "$<DEVICE_LINK:SHELL:-Xnvlink -w>"
    )
endif()