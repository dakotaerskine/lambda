#pragma once

#include <cctype>
#include <cerrno>
#include <clocale>
#include <cstring>
#include <iostream>
#include <new>
#include <stdexcept>
#include <string>

namespace lambda {
    class Error : public std::runtime_error {
        public:
            using std::runtime_error::runtime_error;

            static Error length(int length) { return Error("length " + std::to_string(length) + " is invalid"); }

            virtual std::string format(const std::string & program) const { return program + ": " + std::runtime_error::what(); }
    };

    class FileError : public Error {
        public:
            FileError(const std::string & _file, int _line, int _column, const std::string & _message) : Error(_message), file(_file), line(_line), column(_column) {}

            FileError(const std::string & _file, const std::string & _message) : FileError(_file, 0, 0, _message) {}

            static FileError failedToOpen(const std::string & file) {
                const char * rawError = strerror_l(errno, LC_GLOBAL_LOCALE);

                std::string errorString = (rawError && rawError[0] != '\0') ? std::string(rawError) : "";

                if (!errorString.empty()) {
                    errorString[0] = char(std::tolower(errorString[0]));

                    errorString = " (" + errorString + ")";
                }

                return FileError(file, "failed to open file" + errorString);
            }

            std::string format(const std::string &) const override { return file + (line > 0 ? ":" + std::to_string(line) + ":" + std::to_string(column) : "") + ": " + Error::what(); }

        private:
            std::string file;
            int line;
            int column;
    };

    #ifdef __CUDACC__
        class CudaError : public Error {
            public:
                using Error::Error;

                static void check(cudaError_t err, const std::string & message) {
                    if (err != cudaSuccess) {
                        std::string errorString = cudaGetErrorString(err);

                        if (!errorString.empty()) {
                            errorString[0] = char(std::tolower(errorString[0]));

                            errorString = " (" + errorString + ")";
                        }

                        throw CudaError(message + errorString);
                    }
                }
        };
    #endif

    class ErrorHandler {
        public:
            ErrorHandler() = delete;

            static void handle(const std::string & program, const std::exception & exception) {
                const Error * error = dynamic_cast<const Error *>(&exception);

                std::cerr << (program.empty() ? exception.what() : error ? error->format(program) : program + ": " + exception.what()) << std::endl;
            }
    };
}