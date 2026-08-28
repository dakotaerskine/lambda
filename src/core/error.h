#pragma once

#include <cctype>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>

[[noreturn]] inline void error(const std::string & message) {
    throw std::runtime_error(message);
}

[[noreturn]] inline void fileError(const std::string & file, int lineNumber, const std::string & message) {
    if (lineNumber > 0) error(file + ":" + std::to_string(lineNumber) + ": " + message);
    else error(file + ": " + message);
}

[[noreturn]] inline void fileError(const std::string & file, const std::string & message) { fileError(file, 0, message); }

[[noreturn]] inline void invalidArgumentError(const std::string & function) {
    throw std::invalid_argument(function);
}

[[noreturn]] inline void failedToOpenFileError(const std::string & file) {
    std::string errorString = std::strerror(errno);
    errorString[0] = char(std::tolower(errorString[0]));
    fileError(file, "failed to open file (" + errorString + ")");
}

#ifdef __CUDACC__
    inline void checkCudaError(cudaError_t err, const std::string & message) {
        if (err != cudaSuccess) {
            std::string errorString = cudaGetErrorString(err);
            errorString[0] = char(std::tolower(errorString[0]));
            error(message + " (" + errorString + ")");
        }
    }
#endif

inline std::string errorMessage(const std::string & program, const std::exception & e) {
    std::string errorString = e.what();

    if (errorString.find(": ") == std::string::npos) errorString = program + ": " + errorString;
    
    return errorString;
}