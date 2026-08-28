#pragma once

#include <vector>

#include "core/platform.h"
#include "math/complex.h"
#include "math/spectrum.h"
#include "scene/background.h"
#include "scene/bvh.h"
#include "scene/instance.h"
#include "scene/material.h"
#include "scene/object.h"
#include "scene/texture.h"

class Payload {
    public:
        std::vector<DenseSpectrum<Float>> spectra;
        std::vector<DenseSpectrum<Complex>> complexSpectra;
        Background background;
        std::vector<Object> objects;
        std::vector<Instance> instances;
        std::vector<BVHNode> nodes;
        std::vector<int> lightInstances;
        std::vector<int> lightObjects;
        std::vector<Float> lightPowers;
        Float totalLightPower = 0;
        std::vector<Material> materials;
        std::vector<int> materialProperties;
        std::vector<ScalarTexture> scalarTextures;
        std::vector<SpectrumTexture> spectrumTextures;
        std::vector<Float> images;
};