cmake_minimum_required(VERSION 3.10)

# Project name
project(pathtracer)

# Use C++20 (or higher)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find OpenMP
find_package(OpenMP)

# Add your executable
add_executable(pathtracer
    main.cpp
)

# Link OpenMP and define USE_OPENMP if found
if(OpenMP_CXX_FOUND)
    target_link_libraries(pathtracer PUBLIC OpenMP::OpenMP_CXX)
    target_compile_definitions(pathtracer PRIVATE USE_OPENMP)
    message(STATUS "OpenMP found - parallelization enabled")
else()
    message(STATUS "OpenMP not found - running in serial mode")
endif()

# If you later add .cpp files for implementations instead of all headers,
# list them here as well, e.g. sphere.cpp, vector.cpp, etc.