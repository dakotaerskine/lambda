#pragma once

#include "core/constants.h"
#include "core/platform.h"
#include "math/intersection.h"
#include "math/ray.h"
#include "math/spectrum.h"
#include "scene/background.h"
#include "scene/bvh.h"
#include "scene/instance.h"
#include "scene/object.h"

class Material;
class ScalarTexture;
class SpectrumTexture;

class Scene {
    public:
        HOST_DEVICE Scene() : spectra(nullptr), complexSpectra(nullptr), background(), objects(nullptr), instances(nullptr), nodes(nullptr), lightInstances(nullptr), lightObjects(nullptr), numLights(0), lightPowers(nullptr), totalLightPower(0), materials(nullptr), materialProperties(nullptr), scalarTextures(nullptr), spectrumTextures(nullptr) {}

        HOST_DEVICE Scene(const DenseSpectrum<Float> * _spectra, const DenseSpectrum<Complex> * _complexSpectra, const Background & _background, const Object * _objects, const Instance * _instances, const BVHNode * _nodes, const int * _lightInstances, const int * _lightObjects, int _numLights, const Float * _lightPowers, Float _totalLightPower, const Material * _materials, const int * _materialProperties, const ScalarTexture * _scalarTextures, const SpectrumTexture * _spectrumTextures, const Float * _images) : spectra(_spectra), complexSpectra(_complexSpectra), background(_background), objects(_objects), instances(_instances), nodes(_nodes), lightInstances(_lightInstances), lightObjects(_lightObjects), numLights(_numLights), lightPowers(_lightPowers), totalLightPower(_totalLightPower), materials(_materials), materialProperties(_materialProperties), scalarTextures(_scalarTextures), spectrumTextures(_spectrumTextures), images(_images) {}

        HOST_DEVICE const DenseSpectrum<Float> * getSpectra() const { return spectra; }

        HOST_DEVICE const DenseSpectrum<Complex> * getComplexSpectra() const { return complexSpectra; }

        HOST_DEVICE const Background & getBackground() const { return background; }

        HOST_DEVICE const Object * getObjects() const { return objects; }

        HOST_DEVICE const Instance * getInstances() const { return instances; }

        HOST_DEVICE const int * getLightInstances() const { return lightInstances; }

        HOST_DEVICE const int * getLightObjects() const { return lightObjects; }

        HOST_DEVICE int getNumLights() const { return numLights; }

        HOST_DEVICE const Float * getLightPowers() const { return lightPowers; }

        HOST_DEVICE Float getTotalLightPower() const { return totalLightPower; }

        HOST_DEVICE const Material * getMaterials() const { return materials; }

        HOST_DEVICE const int * getMaterialProperties() const { return materialProperties; }

        HOST_DEVICE const ScalarTexture * getScalarTextures() const { return scalarTextures; }

        HOST_DEVICE const SpectrumTexture * getSpectrumTextures() const { return spectrumTextures; }

        HOST_DEVICE const Float * getImages() const { return images; }

        HOST_DEVICE bool hit(const Ray & r, Intersection & intersection, int occludedInstance = -1, int occludedObject = -2) const {
            int stack[BVH_MAX_DEPTH];
            int stackSize = 0;
            stack[stackSize++] = 0;

            int subStack[BVH_MAX_DEPTH];
            int subStackSize = 0;

            bool hitObject = false;

            if (occludedInstance != -1) {
                Ray transformedRay = instances[occludedInstance].transformRayToLocal(r);

                if (objects[occludedObject].intersect(occludedObject, transformedRay, intersection)) {
                    intersection.instance = occludedInstance;
                    intersection.localPoint = intersection.point;
                    intersection.localNormal = intersection.normal;
                    intersection.point = instances[occludedInstance].transformPointToWorld(intersection.point);
                    intersection.normal = instances[occludedInstance].transformNormalToWorld(intersection.normal);
                }
            }

            while (stackSize > 0) {
                int current = stack[--stackSize];

                Intersection nodeIntersection;

                if (!nodes[current].intersect(r, nodeIntersection) || nodeIntersection.t > intersection.t) continue;

                if (nodes[current].isLeaf()) {
                    int instanceIndex = nodes[current].getIndex();

                    for (int i = 0; i < nodes[current].getCount(); i++) {
                        const Instance & instance = instances[instanceIndex + i];

                        subStack[subStackSize++] = instance.getNode();

                        Ray transformedRay = instance.transformRayToLocal(r);

                        while (subStackSize > 0) {
                            int subCurrent = subStack[--subStackSize];

                            Intersection subNodeIntersection;

                            if (!nodes[subCurrent].intersect(transformedRay, subNodeIntersection) || subNodeIntersection.t > intersection.t) continue;

                            if (nodes[subCurrent].isLeaf()) {
                                int objectIndex = nodes[subCurrent].getIndex();

                                for (int j = 0; j < nodes[subCurrent].getCount(); j++) {
                                    if (occludedInstance != -1 && instanceIndex + i == occludedInstance && objectIndex + j == occludedObject) continue;

                                    if (objects[objectIndex + j].intersect(objectIndex + j, transformedRay, intersection)) {
                                        intersection.instance = instanceIndex + i;
                                        intersection.localPoint = intersection.point;
                                        intersection.localNormal = intersection.normal;
                                        intersection.point = instance.transformPointToWorld(intersection.point);
                                        intersection.normal = instance.transformNormalToWorld(intersection.normal);

                                        if (occludedObject != -2) return true;
                                        else hitObject = true;
                                    }
                                }
                            }
                            else {
                                subStack[subStackSize++] = subCurrent + 1;
                                subStack[subStackSize++] = nodes[subCurrent].getRight();
                            }
                        }
                    }
                }
                else {
                    stack[stackSize++] = current + 1;
                    stack[stackSize++] = nodes[current].getRight();
                }
            }

            return hitObject;
        }

    private:
        const DenseSpectrum<Float> * spectra;
        const DenseSpectrum<Complex> * complexSpectra;
        Background background;
        const Object * objects;
        const Instance * instances;
        const BVHNode * nodes;
        const int * lightInstances;
        const int * lightObjects;
        int numLights;
        const Float * lightPowers;
        Float totalLightPower;
        const Material * materials;
        const int * materialProperties;
        const ScalarTexture * scalarTextures;
        const SpectrumTexture * spectrumTextures;
        const Float * images;
};