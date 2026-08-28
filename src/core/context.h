#pragma once

#include <vector>

#include "core/buffer.h"
#include "core/payload.h"
#include "render/renderer.h"

class Context {
    public:
        Context(const Payload & payload, Renderer & renderer) : spectra(payload.spectra), complexSpectra(payload.complexSpectra), objects(payload.objects), instances(payload.instances), nodes(payload.nodes), lightInstances(payload.lightInstances), lightObjects(payload.lightObjects), lightPowers(payload.lightPowers), materials(payload.materials), materialProperties(payload.materialProperties), scalarTextures(payload.scalarTextures), spectrumTextures(payload.spectrumTextures), images(payload.images), output(renderer.getTotalPixels() * 3) {
            renderer.setScene(spectra.data(), complexSpectra.data(), payload.background, objects.data(), instances.data(), nodes.data(), lightInstances.data(), lightObjects.data(), int(payload.lightInstances.size()), lightPowers.data(), payload.totalLightPower, materials.data(), materialProperties.data(), scalarTextures.data(), spectrumTextures.data(), images.data());
            renderer.setBuffer(output.data());
        }

    private:
        Buffer<DenseSpectrum<Float>> spectra;
        Buffer<DenseSpectrum<Complex>> complexSpectra;
        Buffer<Object> objects;
        Buffer<Instance> instances;
        Buffer<BVHNode> nodes;
        Buffer<int> lightInstances;
        Buffer<int> lightObjects;
        Buffer<Float> lightPowers;
        Buffer<Material> materials;
        Buffer<int> materialProperties;
        Buffer<ScalarTexture> scalarTextures;
        Buffer<SpectrumTexture> spectrumTextures;
        Buffer<Float> images;
        Buffer<Float> output;
};