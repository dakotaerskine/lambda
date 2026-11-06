cmake_minimum_required(VERSION 3.24)

project(lambda LANGUAGES CXX)

option(LAMBDA_CPU "Force CPU build" OFF)
option(LAMBDA_DOUBLE_PRECISION "Use double precision for floating point values" OFF)
option(LAMBDA_UPSAMPLING "Toggle spectral upsampling" ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CUDA_STANDARD 20)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)
 
set(LAMBDA_CUDA OFF)

if(NOT LAMBDA_CPU)
    include(CheckLanguage)

    check_language(CUDA)

    if(CMAKE_CUDA_COMPILER)
        set(CMAKE_CUDA_ARCHITECTURES native)

        enable_language(CUDA)

        set(LAMBDA_CUDA ON)
    endif()
endif()
 
find_package(OpenMP REQUIRED COMPONENTS CXX)
 
set(LAMBDA_SOURCE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(LAMBDA_DATA_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/data")

file(MAKE_DIRECTORY "${LAMBDA_DATA_DIRECTORY}")

set(LAMBDA_WARNINGS -Wall -Wextra -Wshadow -Wconversion)
 
list(JOIN LAMBDA_WARNINGS "," LAMBDA_CUDA_HOST_WARNINGS)
 
add_library(lambda_common INTERFACE)
 
target_include_directories(lambda_common INTERFACE "${LAMBDA_SOURCE_DIRECTORY}")
 
target_compile_definitions(lambda_common INTERFACE LAMBDA_DATA_DIRECTORY="${LAMBDA_DATA_DIRECTORY}")

target_compile_options(lambda_common INTERFACE "$<$<COMPILE_LANGUAGE:CXX>:${LAMBDA_WARNINGS}>" "$<$<COMPILE_LANGUAGE:CUDA>:-lineinfo;-Xcompiler=${LAMBDA_CUDA_HOST_WARNINGS},${OpenMP_CXX_FLAGS}>")
 
target_link_libraries(lambda_common INTERFACE OpenMP::OpenMP_CXX)
 
if(LAMBDA_UPSAMPLING)
    add_executable(lambda_upsampling ${LAMBDA_SOURCE_DIRECTORY}/upsampling.cpp)
 
    target_link_libraries(lambda_upsampling PRIVATE lambda_common)
 
    target_compile_definitions(lambda_upsampling PRIVATE LAMBDA_UPSAMPLING=0)
endif()
 
add_executable(lambda ${LAMBDA_SOURCE_DIRECTORY}/main.cpp)
 
target_link_libraries(lambda PRIVATE lambda_common)
 
target_include_directories(lambda SYSTEM PRIVATE "${LAMBDA_SOURCE_DIRECTORY}/ext")

target_compile_definitions(lambda PRIVATE LAMBDA_DOUBLE_PRECISION=$<BOOL:${LAMBDA_DOUBLE_PRECISION}> LAMBDA_UPSAMPLING=$<BOOL:${LAMBDA_UPSAMPLING}>)
 
if(LAMBDA_CUDA)
    set_source_files_properties(${LAMBDA_SOURCE_DIRECTORY}/main.cpp PROPERTIES LANGUAGE CUDA)
 
    set_target_properties(lambda PROPERTIES CUDA_SEPARABLE_COMPILATION ON)
 
    target_link_options(lambda PRIVATE "$<DEVICE_LINK:SHELL:-Xnvlink -w>")
endif()

if(LAMBDA_UPSAMPLING)
    foreach(space srgb rec2020 aces2065-1)
        set(output "${LAMBDA_DATA_DIRECTORY}/${space}.coeff")
 
        add_custom_command(OUTPUT ${output} COMMAND lambda_upsampling ${space} DEPENDS lambda_upsampling USES_TERMINAL VERBATIM)
 
        target_sources(lambda PRIVATE ${output})
    endforeach()
endif()
