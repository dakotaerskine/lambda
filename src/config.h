#pragma once

#include "platform.h"

class Config {
    public:
        HOST_DEVICE Config() : width(0), height(0), totalPixels(0), depth(0), samples(0), sqrtSamples(0), wavelengthSamples(0), lambdaMin(0), lambdaMax(0), lambdaStep(0) {}

        int width, height, totalPixels, depth, samples, sqrtSamples, wavelengthSamples;
        Float lambdaMin, lambdaMax, lambdaStep;
};