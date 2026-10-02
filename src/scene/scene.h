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

namespace lambda {
    class Material;
    class ScalarTexture;
    class SpectrumTexture;

    class Scene {
        public:
            Scene() : node(nullptr), lightNode(nullptr), background(nullptr) {}

            void setNode(BVHNode<Instance> * n) { node = n; }

            void setLightNode(BVHNode<Instance> * n) { lightNode = n; }

            void setBackground(Background * b) { background = b; }

            LAMBDA_HOST_DEVICE const Background * getBackground() const { return background; }

            LAMBDA_HOST_DEVICE Float getTotalPower() const {
                Float totalPower = background->getPower();

                if (lightNode) totalPower += lightNode->getPower();

                if (totalPower < constants::EPSILON) totalPower = 1;

                return totalPower;
            }

            LAMBDA_HOST_DEVICE bool hit(const Ray & r, Intersection & intersection, bool occluded = false, const Object * occludedObject = nullptr, const Instance * occludedInstance = nullptr) const {
                if (!node) return false;

                const BVHNode<Instance> * stack[constants::BVH_MAX_DEPTH + 1];
                int stackSize = 0;
                stack[stackSize++] = node;

                const BVHNode<Object> * objectStack[constants::BVH_MAX_DEPTH + 1];
                int objectStackSize = 0;

                bool hitObject = false;

                if (occluded && occludedInstance && occludedObject) {
                    Ray transformedRay = occludedInstance->transformRayToLocal(r);

                    if (occludedObject->intersect(transformedRay, intersection)) {
                        intersection.instance = occludedInstance;
                        intersection.object = occludedObject;
                        intersection.transformedPoint = occludedInstance->transformPointToWorld(intersection.point);
                        intersection.transformedNormal = occludedInstance->transformNormalToWorld(intersection.normal);
                        intersection.transformedTangent = normalize(occludedInstance->transformVectorToWorld(intersection.tangent));
                        intersection.useTransformed = occludedInstance->hasMaterial();
                    }
                }

                while (stackSize > 0) {
                    const BVHNode<Instance> * instanceNode = stack[--stackSize];

                    if (!instanceNode->intersect(r, intersection)) continue;

                    if (instanceNode->isLeaf())
                        for (int i = 0; i < instanceNode->getCount(); i++) {
                            const Instance * instance = instanceNode->getElement(i);

                            objectStack[objectStackSize++] = instance->getNode();

                            Ray transformedRay = instance->transformRayToLocal(r);

                            while (objectStackSize > 0) {
                                const BVHNode<Object> * objectNode = objectStack[--objectStackSize];

                                if (!objectNode->intersect(transformedRay, intersection)) continue;

                                if (objectNode->isLeaf())
                                    for (int j = 0; j < objectNode->getCount(); j++) {
                                        const Object * object = objectNode->getElement(j);
                                        const Material * material = instance->getMaterial(int(object - instance->getObject(0)));

                                        if (occluded && instance == occludedInstance && object == occludedObject) continue;

                                        if ((!occluded || (occluded && material)) && object->intersect(transformedRay, intersection)) {
                                            intersection.instance = instance;
                                            intersection.object = object;
                                            intersection.transformedPoint = instance->transformPointToWorld(intersection.point);
                                            intersection.transformedNormal = instance->transformNormalToWorld(intersection.normal);
                                            intersection.transformedTangent = normalize(instance->transformVectorToWorld(intersection.tangent));
                                            intersection.useTransformed = instance->hasMaterial();
                                            intersection.frontFacing = dot(r.getDirection(), intersection.transformedNormal) < 0;
                                            if (!intersection.frontFacing) intersection.transformedNormal *= -1;

                                            if (occluded) return true;
                                            else hitObject = true;
                                        }
                                    }
                                else {
                                    objectStack[objectStackSize++] = objectNode + 1;
                                    objectStack[objectStackSize++] = objectNode->getRight();
                                }
                            }
                        }
                    else {
                        stack[stackSize++] = instanceNode + 1;
                        stack[stackSize++] = instanceNode->getRight();
                    }
                }

                return hitObject;
            }

            LAMBDA_HOST_DEVICE void findLight(Float targetPower, const Object * & object, const Instance * & instance, Float & lightPower) const {
                if (!lightNode || targetPower < background->getPower()) {
                    object = nullptr;
                    instance = nullptr;

                    lightPower = background->getPower();

                    return;
                }
                else targetPower -= background->getPower();

                const BVHNode<Instance> * instanceNode = lightNode;

                while (!instanceNode->isLeaf()) {
                    const BVHNode<Instance> * left = instanceNode + 1;
                    const BVHNode<Instance> * right = instanceNode->getRight();

                    Float leftPower = left->getPower();

                    if (targetPower < leftPower) instanceNode = left;
                    else {
                        targetPower -= leftPower;

                        instanceNode = right;
                    }
                }

                const BVHNode<Object> * objectNode = nullptr;
                bool hasMaterial = false;

                Float instanceScale = 1, averageEmission = 1;

                for (int i = 0; i < instanceNode->getCount(); i++) {
                    instance = instanceNode->getElement(i);

                    objectNode = instance->getLightNode();

                    instanceScale = instance->getScaleSquared(0);
                    Float instancePower = objectNode->getPower() * instanceScale;

                    averageEmission = 1;

                    hasMaterial = instance->hasMaterial();

                    if (hasMaterial) averageEmission = instance->getMaterial(0)->averageEmission();

                    instancePower *= averageEmission;

                    if (targetPower < instancePower) {
                        targetPower /= instanceScale * averageEmission;

                        break;
                    }
                    else targetPower -= instancePower;
                }

                while (!objectNode->isLeaf()) {
                    const BVHNode<Object> * left = objectNode + 1;
                    const BVHNode<Object> * right = objectNode->getRight();

                    Float leftPower = left->getPower();

                    if (targetPower < leftPower) objectNode = left;
                    else {
                        targetPower -= leftPower;

                        objectNode = right;
                    }
                }

                for (int i = 0; i < objectNode->getCount(); i++) {
                    object = objectNode->getElement(i);

                    Float averageObjectEmission = 1;

                    if (!hasMaterial) averageObjectEmission = object->getMaterial()->averageEmission();

                    Float objectPower = object->getArea() * averageObjectEmission;

                    if (targetPower < objectPower || i == objectNode->getCount() - 1) {
                        if (!hasMaterial) averageEmission = averageObjectEmission;

                        lightPower = objectPower * instanceScale * (hasMaterial ? averageEmission : 1);

                        break;
                    }
                    else targetPower -= objectPower;
                }
            }

        private:
            BVHNode<Instance> * node;
            BVHNode<Instance> * lightNode;
            Background * background;
    };
}