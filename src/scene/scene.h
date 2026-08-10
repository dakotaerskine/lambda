#pragma once

#include "core/platform.h"
#include "math/intersection.h"
#include "math/ray.h"
#include "math/spectrum.h"
#include "scene/object.h"

class Background;
class Material;
class ScalarTexture;
class SpectrumTexture;

class Scene {
    public:
        HOST_DEVICE Scene() : spectra(nullptr), complexSpectra(nullptr), background(), objects(nullptr), numObjects(0), lights(nullptr), numLights(0), lightPowers(nullptr), totalLightPower(0), materials(nullptr), materialProperties(nullptr), scalarTextures(nullptr), spectrumTextures(nullptr) {}

        HOST_DEVICE Scene(DenseSpectrum<Float> * const _spectra, DenseSpectrum<Complex> * const _complexSpectra, const Background & _background, Object * const _objects, int _numObjects, int * const _lights, int _numLights, Float * const _lightPowers, Float _totalLightPower, Material * const _materials, int * const _materialProperties, ScalarTexture * const _scalarTextures, SpectrumTexture * const _spectrumTextures) : spectra(_spectra), complexSpectra(_complexSpectra), background(_background), objects(_objects), numObjects(_numObjects), lights(_lights), numLights(_numLights), lightPowers(_lightPowers), totalLightPower(_totalLightPower), materials(_materials), materialProperties(_materialProperties), scalarTextures(_scalarTextures), spectrumTextures(_spectrumTextures) {}

        HOST_DEVICE DenseSpectrum<Float> * getSpectra() const { return spectra; }

        HOST_DEVICE DenseSpectrum<Complex> * getComplexSpectra() const { return complexSpectra; }

        HOST_DEVICE const Background & getBackground() const { return background; }

        HOST_DEVICE Object * getObjects() const { return objects; }

        HOST_DEVICE int * getLights() const { return lights; }

        HOST_DEVICE int getNumLights() const { return numLights; }

        HOST_DEVICE Float * getLightPowers() const { return lightPowers; }

        HOST_DEVICE Float getTotalLightPower() const { return totalLightPower; }

        HOST_DEVICE Material * getMaterials() const { return materials; }

        HOST_DEVICE int * getMaterialProperties() const { return materialProperties; }

        HOST_DEVICE ScalarTexture * getScalarTextures() const { return scalarTextures; }

        HOST_DEVICE SpectrumTexture * getSpectrumTextures() const { return spectrumTextures; }

        HOST_DEVICE bool hit(const Ray & r, Intersection & intersection) const {
            bool hitObject = false;

            for (int i = 0; i < numObjects; i++)
                if (objects[i].intersect(i, r, intersection)) hitObject = true;

            return hitObject;
        }

    private:
        DenseSpectrum<Float> * spectra;
        DenseSpectrum<Complex> * complexSpectra;
        Background background;
        Object * objects;
        int numObjects;
        int * lights;
        int numLights;
        Float * lightPowers;
        Float totalLightPower;
        Material * materials;
        int * materialProperties;
        ScalarTexture * scalarTextures;
        SpectrumTexture * spectrumTextures;
};