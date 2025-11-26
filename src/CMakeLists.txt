cmake_minimum_required(VERSION 3.10)

# Project name
project(pathtracer)

# Use C++17 (or higher)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add your executable
add_executable(pathtracer
    main.cpp
)

# If you later add .cpp files for implementations instead of all headers,
# list them here as well, e.g. sphere.cpp, vector.cpp, etc.