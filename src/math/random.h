#pragma once

#include <cstdint>

#include "core/platform.h"

class Random {
    public:
        HOST_DEVICE Random(uint64_t seed, uint64_t stream) {
            state = 0;
            inc = (stream << 1) | 1;
            step();
            state += seed;
            step();
        }

        HOST_DEVICE Float next() {
            uint32_t x = step();
            return Float(x) / Float(4294967296.0);
        }

    private:
        uint64_t state, inc;

        HOST_DEVICE uint32_t step() {
            uint64_t old = state;
            state = old * 6364136223846793005ULL + inc;
            uint32_t xorshifted = ((old >> 18u) ^ old) >> 27u;
            uint32_t rot = old >> 59u;
            return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
        }
};