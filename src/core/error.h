#pragma once

#include <cctype>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>

namespace lambda {
    namespace error {
        class FileError : public std::runtime_error {
            public:
                using std::runtime_error::runtime_error;
        };

        [[noreturn]] inline void error(const std::string & message) {
            throw std::runtime_error(message);
        }

        [[noreturn]] inline void fileError(std::string file, int line, int column, const std::string & message) {
            if (line > 0) file += ":" + std::to_string(line) + ":" + std::to_string(column);

            throw FileError(file + ": " + message);
        }

        [[noreturn]] inline void fileError(const std::string & file, const std::string & message) { fileError(file, 0, 0, message); }

        [[noreturn]] inline void failedToOpenFileError(const std::string & file) {
            std::string errorString;

            errorString.resize(1024);

            if (strerror_r(errno, errorString.data(), errorString.size())) errorString = "";

            if (!errorString.empty()) {
                errorString[0] = char(std::tolower(errorString[0]));

                errorString = " (" + errorString + ")";
            }

            fileError(file, "failed to open file" + errorString);
        }

        [[noreturn]] inline void lengthError(int length) { throw std::length_error("length " + std::to_string(length) + " is invalid"); }

        [[noreturn]] inline void outOfRangeError(int i) { throw std::out_of_range("index " + std::to_string(i) + " is out of range"); }

        #ifdef __CUDACC__
            inline void checkCudaError(cudaError_t err, const std::string & message) {
                if (err != cudaSuccess) {
                    std::string errorString = cudaGetErrorString(err);

                    if (!errorString.empty()) {
                        errorString[0] = char(std::tolower(errorString[0]));

                        errorString = " (" + errorString + ")";
                    }
                    error(message + errorString);
                }
            }
        #endif

        inline std::string message(const std::string & program, const std::exception & e) {
            std::string errorString = e.what();

            if (!dynamic_cast<const FileError *>(&e)) errorString = program + ": " + errorString;

            return errorString;
        }
    }
}