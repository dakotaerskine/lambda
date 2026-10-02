#pragma once

#include <cstdint>

#include "core/platform.h"

namespace lambda {
    class Random {
        public:
            LAMBDA_HOST_DEVICE Random(uint64_t seed, uint64_t stream) {
                state = 0;
                inc = (stream << 1) | 1;
                step();
                state += seed;
                step();
            }

            LAMBDA_HOST_DEVICE Float next() { return Float(step() >> 8) * Float(0x1p-24); }

        private:
            uint64_t state, inc;

            LAMBDA_HOST_DEVICE uint32_t step() {
                uint64_t old = state;
                state = old * 6364136223846793005ULL + inc;
                uint32_t xorshifted = uint32_t(((old >> 18u) ^ old) >> 27u);
                uint32_t rot = uint32_t(old >> 59u);
                return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
            }
    };
}