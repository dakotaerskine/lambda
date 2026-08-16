#pragma once

#include "core/constants.h"
#include "core/platform.h"
#include "math/intersection.h"
#include "math/ray.h"
#include "math/spectrum.h"
#include "scene/bvh.h"
#include "scene/instance.h"
#include "scene/object.h"

class Background;
class Material;
class ScalarTexture;
class SpectrumTexture;

class Scene {
    public:
        HOST_DEVICE Scene() : spectra(nullptr), complexSpectra(nullptr), background(), objects(nullptr), instances(nullptr), nodes(nullptr), lightInstances(nullptr), lightObjects(nullptr), numLights(0), lightPowers(nullptr), totalLightPower(0), materials(nullptr), materialProperties(nullptr), scalarTextures(nullptr), spectrumTextures(nullptr) {}

        HOST_DEVICE Scene(DenseSpectrum<Float> * const _spectra, DenseSpectrum<Complex> * const _complexSpectra, const Background & _background, Object * const _objects, Instance * const _instances, BVHNode * const _nodes, int * const _lightInstances, int * const _lightObjects, int _numLights, Float * const _lightPowers, Float _totalLightPower, Material * const _materials, int * const _materialProperties, ScalarTexture * const _scalarTextures, SpectrumTexture * const _spectrumTextures) : spectra(_spectra), complexSpectra(_complexSpectra), background(_background), objects(_objects), instances(_instances), nodes(_nodes), lightInstances(_lightInstances), lightObjects(_lightObjects), numLights(_numLights), lightPowers(_lightPowers), totalLightPower(_totalLightPower), materials(_materials), materialProperties(_materialProperties), scalarTextures(_scalarTextures), spectrumTextures(_spectrumTextures) {}

        HOST_DEVICE DenseSpectrum<Float> * getSpectra() const { return spectra; }

        HOST_DEVICE DenseSpectrum<Complex> * getComplexSpectra() const { return complexSpectra; }

        HOST_DEVICE const Background & getBackground() const { return background; }

        HOST_DEVICE Object * getObjects() const { return objects; }

        HOST_DEVICE Instance * getInstances() const { return instances; }

        HOST_DEVICE int * getLightInstances() const { return lightInstances; }

        HOST_DEVICE int * getLightObjects() const { return lightObjects; }

        HOST_DEVICE int getNumLights() const { return numLights; }

        HOST_DEVICE Float * getLightPowers() const { return lightPowers; }

        HOST_DEVICE Float getTotalLightPower() const { return totalLightPower; }

        HOST_DEVICE Material * getMaterials() const { return materials; }

        HOST_DEVICE int * getMaterialProperties() const { return materialProperties; }

        HOST_DEVICE ScalarTexture * getScalarTextures() const { return scalarTextures; }

        HOST_DEVICE SpectrumTexture * getSpectrumTextures() const { return spectrumTextures; }

        HOST_DEVICE bool hit(const Ray & r, Intersection & intersection, int occludedInstance = -1, int occludedObject = -2) const {
            int stack[BVH_MAX_DEPTH];
            int stackSize = 0;
            stack[stackSize++] = 0;

            int subStack[BVH_MAX_DEPTH];
            int subStackSize = 0;

            bool hitObject = false;

            if (occludedInstance != -1) {
                Ray transformedRay = instances[occludedInstance].transformRayToLocal(r);

                if (!objects[occludedObject].intersect(occludedObject, transformedRay, intersection)) return true;
                else {
                    intersection.instance = occludedInstance;
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

                    const Instance & instance = instances[instanceIndex];

                    subStack[subStackSize++] = instance.getNode();

                    Ray transformedRay = instance.transformRayToLocal(r);

                    while (subStackSize > 0) {
                        int subCurrent = subStack[--subStackSize];

                        Intersection subNodeIntersection;

                        if (!nodes[subCurrent].intersect(transformedRay, subNodeIntersection) || subNodeIntersection.t > intersection.t) continue;

                        if (nodes[subCurrent].isLeaf()) {
                            int objectIndex = nodes[subCurrent].getIndex();

                            for (int i = 0; i < nodes[subCurrent].getCount(); i++) {
                                if (occludedInstance != -1 && instanceIndex == occludedInstance && objectIndex + i == occludedObject) continue;

                                if (objects[objectIndex + i].intersect(objectIndex + i, transformedRay, intersection)) {
                                    intersection.instance = instanceIndex;
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
                else {
                    stack[stackSize++] = current + 1;
                    stack[stackSize++] = nodes[current].getRight();
                }
            }
            
            return hitObject;
        }

    private:
        DenseSpectrum<Float> * spectra;
        DenseSpectrum<Complex> * complexSpectra;
        Background background;
        Object * objects;
        Instance * instances;
        BVHNode * nodes;
        int * lightInstances;
        int * lightObjects;
        int numLights;
        Float * lightPowers;
        Float totalLightPower;
        Material * materials;
        int * materialProperties;
        ScalarTexture * scalarTextures;
        SpectrumTexture * spectrumTextures;
};