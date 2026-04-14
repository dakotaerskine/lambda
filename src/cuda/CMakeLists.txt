cmake_minimum_required(VERSION 3.18)

project(pathtracer LANGUAGES CXX CUDA)

set(CMAKE_CUDA_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)

set(CMAKE_CUDA_SEPARABLE_COMPILATION ON)

add_executable(pathtracer main.cu renderer.cu)

target_compile_options(pathtracer PRIVATE
    $<$<COMPILE_LANGUAGE:CUDA>:
        --use_fast_math
        -arch=sm_89
        -lineinfo
    >
)

target_link_libraries(pathtracer PRIVATE curand)