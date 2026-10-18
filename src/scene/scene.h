#pragma once

#include "core/constants.h"
#include "core/platform.h"
#include "math/ray.h"
#include "math/spectrum.h"
#include "scene/background.h"
#include "scene/bvh.h"
#include "scene/instance.h"
#include "scene/intersection.h"
#include "scene/object.h"

namespace lambda {
    class Material;
    class ScalarTexture;
    class SpectrumTexture;

    class Scene {
        public:
            Scene(BVHNode<Instance> * _node = nullptr, BVHNode<Instance> * _lightNode = nullptr, Background * _backgrounds = nullptr, int _numBackgrounds = 0) : node(_node), lightNode(_lightNode), backgrounds(_backgrounds), numBackgrounds(_numBackgrounds) {}

            LAMBDA_HOST_DEVICE const Background * getBackground(int i) const { return backgrounds + i; }

            LAMBDA_HOST_DEVICE int getNumBackgrounds() const { return numBackgrounds; }

            LAMBDA_HOST_DEVICE Float getTotalPower() const {
                Float totalPower = 0;

                for (int i = 0; i < numBackgrounds; i++) totalPower += backgrounds[i].getPower();

                if (lightNode) totalPower += lightNode->getPower();

                return totalPower;
            }

            LAMBDA_HOST_DEVICE bool hit(Intersection & intersection, const Ray & r) const { return hit<false>(intersection, r, nullptr, -1); }

            LAMBDA_HOST_DEVICE bool hit(Intersection & intersection, const Ray & r, const Instance * occludedInstance, int occludedObjectIndex) const { return hit<true>(intersection, r, occludedInstance, occludedObjectIndex); }

            LAMBDA_HOST_DEVICE Float lightProbabilityDensity(const Instance * instance, int objectIndex, const Vector<Float, 3> & point, const Vector<Float, 3> & direction) const {
                Float totalPower = getTotalPower();

                if (totalPower <= constants::EPSILON) return 0;

                if (!instance) {
                    Float density = 0;

                    for (int i = 0; i < numBackgrounds; i++) density += backgrounds[i].probabilityDensity(direction) * backgrounds[i].getPower() / totalPower;

                    return density;
                }

                return instance->probabilityDensity(objectIndex, point, direction) * instance->getPower(objectIndex) / totalPower;
            }

            LAMBDA_HOST_DEVICE Vector<Float, 3> sampleLight(const Vector<Float, 3> & point, Float & probabilityDensity, const Instance * & instance, int & objectIndex, Random & state) const {
                instance = nullptr;
                objectIndex = -1;

                Float totalPower = getTotalPower();

                if (totalPower <= constants::EPSILON) {
                    probabilityDensity = 0;

                    return Vector<Float, 3>(0, 1, 0);
                }

                Float targetPower = totalPower * utils::randomFloat(state);

                Vector<Float, 3> direction;

                for (int i = 0; i < numBackgrounds; i++) {
                    Float backgroundPower = backgrounds[i].getPower();

                    if (targetPower < backgroundPower || (!lightNode && i == numBackgrounds - 1)) {
                        direction = backgrounds[i].sample(state);

                        probabilityDensity = lightProbabilityDensity(instance, objectIndex, point, direction);

                        return direction;
                    }

                    targetPower -= backgroundPower;
                }

                const BVHNode<Instance> * current = lightNode;

                while (!current->isLeaf()) {
                    const BVHNode<Instance> * left = current + 1;
                    const BVHNode<Instance> * right = current + current->getCount();

                    Float leftPower = left->getPower();

                    if (targetPower < leftPower) current = left;
                    else {
                        targetPower -= leftPower;

                        current = right;
                    }
                }

                for (int i = 0; i < current->getCount(); i++) {
                    instance = current->getElement(i);

                    Float instancePower = instance->getPower();

                    if (targetPower < instancePower || i == current->getCount() - 1) break;

                    targetPower -= instancePower;
                }

                direction = instance->sampleLight(point, probabilityDensity, objectIndex, state);

                probabilityDensity *= instance->getPower(objectIndex) / totalPower;

                return direction;
            }

        private:
            BVHNode<Instance> * node;
            BVHNode<Instance> * lightNode;
            Background * backgrounds;
            int numBackgrounds;

            template <bool occluded>
            LAMBDA_HOST_DEVICE bool hit(Intersection & intersection, const Ray & r, const Instance * occludedInstance, int occludedObjectIndex) const {
                if (!node) return false;

                const BVHNode<Instance> * stack[constants::BVH_MAX_DEPTH + 1];
                int stackSize = 0;
                stack[stackSize++] = node;

                bool hitObject = false;

                if constexpr (occluded)
                    if (occludedInstance) occludedInstance->intersect(occludedObjectIndex, intersection, r);

                while (stackSize > 0) {
                    const BVHNode<Instance> * current = stack[--stackSize];

                    if (!current->intersect(intersection, r)) continue;

                    if (current->isLeaf())
                        for (int i = 0; i < current->getCount(); i++) {
                            const Instance * instance = current->getElement(i);

                            if constexpr (occluded) {
                                if (instance->hit(intersection, r, occludedInstance, occludedObjectIndex)) return true;
                            }
                            else if (instance->hit(intersection, r)) hitObject = true;
                        }
                    else {
                        stack[stackSize++] = current + 1;
                        stack[stackSize++] = current + current->getCount();
                    }
                }

                return hitObject;
            }
    };
}