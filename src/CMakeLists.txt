cmake_minimum_required(VERSION 3.24)

project(pathtracer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

include(CheckLanguage)
check_language(CUDA)

if(CMAKE_CUDA_COMPILER AND NOT CPU)
    enable_language(CUDA)

    set(CMAKE_CUDA_STANDARD 20)
    set(CMAKE_CUDA_STANDARD_REQUIRED ON)
    set(CMAKE_CUDA_SEPARABLE_COMPILATION ON)
    set(CMAKE_CUDA_ARCHITECTURES native)

    add_executable(pathtracer main.cpp)

    set_source_files_properties(main.cpp PROPERTIES LANGUAGE CUDA)

    target_compile_options(pathtracer PRIVATE
        $<$<COMPILE_LANGUAGE:CUDA>:
            -lineinfo
            -Xcompiler=-Wall
            -Xcompiler=-Wextra
        >
    )

    target_link_options(pathtracer PRIVATE
        "$<DEVICE_LINK:SHELL:-Xnvlink -w>"
    )
else()
    find_package(OpenMP REQUIRED)
    target_link_libraries(pathtracer PRIVATE OpenMP::OpenMP_CXX)
    target_compile_definitions(pathtracer PRIVATE USE_OPENMP)

    add_executable(pathtracer main.cpp)

    target_compile_options(pathtracer PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:
            -Wall
            -Wextra
            -Wpedantic
        >
    )
endif()