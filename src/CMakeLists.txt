cmake_minimum_required(VERSION 3.24)

project(lambda LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(CPU "Force CPU build" OFF)

if (NOT CPU)
    include(CheckLanguage)
    check_language(CUDA)

    if(CMAKE_CUDA_COMPILER)
        enable_language(CUDA)

        set(CMAKE_CUDA_ARCHITECTURES native)
    endif()
endif()

add_executable(lambda main.cpp)

target_include_directories(lambda PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if(CMAKE_CUDA_COMPILER AND NOT CPU)
    set_source_files_properties(main.cpp PROPERTIES LANGUAGE CUDA)

    set_target_properties(lambda PROPERTIES CUDA_STANDARD 20 CUDA_STANDARD_REQUIRED ON CUDA_SEPARABLE_COMPILATION ON)

    target_compile_options(lambda PRIVATE -lineinfo -Xcompiler=-Wall,-Wextra,-Wshadow,-Wconversion,-Wfloat-equal)

    target_link_options(lambda PRIVATE "$<DEVICE_LINK:SHELL:-Xnvlink -w>")
else()
    set_target_properties(lambda PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)

    find_package(OpenMP REQUIRED)
    target_link_libraries(lambda PRIVATE OpenMP::OpenMP_CXX)
    target_compile_definitions(lambda PRIVATE USE_OPENMP)

    target_compile_options(lambda PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wfloat-equal)
endif()