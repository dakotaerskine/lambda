#pragma once

#include <cmath>
#include <unordered_map>

#include "core/buffer.h"
#include "core/context.h"
#include "core/platform.h"
#include "math/vector.h"
#include "render/renderer.h"
#include "scene/bvh.h"
#include "scene/background.h"
#include "scene/instance.h"
#include "scene/material.h"
#include "scene/object.h"
#include "scene/scene.h"
#include "scene/texture.h"

namespace lambda {
    class Builder {
        public:
            static void buildContext(Context & context) {
                Background * background = context.getBackground();

                background->setRadius(computeSceneRadius(context));

                std::vector<Float> conditionalCDF, marginalCDF;

                buildCDF(background, conditionalCDF, marginalCDF);


                background->setConditionalCDF(context.addConditionalCDF(conditionalCDF.data(), int(std::ssize(conditionalCDF))));
                background->setMarginalCDF(context.addMarginalCDF(marginalCDF.data(), int(std::ssize(marginalCDF))));

                Scene * scene = context.getScene();

                scene->setBackground(background);
                scene->setNode(buildGeometryBVH(context));
                scene->setLightNode(buildLightBVH(context));

                Renderer * renderer = context.getRenderer();

                renderer->setScene(scene);
                renderer->setCamera(context.getCamera());
                renderer->setBuffer(context.addOutput(renderer->getTotalPixels() * 3));
            }

        private:
            static Float computeSceneRadius(Context & context) {
                Vector<Float, 3> sceneCenter;
                Float sceneRadius = 0;

                for (int i = 0; i < context.getInstanceCount(); i++) {
                    Instance * instance = context.getInstance(i);

                    for (int j = 0; j < instance->getCount(); j++) {
                        Vector<Float, 3> objectCenter = instance->center(j);
                        Float objectRadius = instance->radius(j);

                        Vector<Float, 3> sceneToObject = objectCenter - sceneCenter;
                        Float distance = sceneToObject.length();

                        if (distance + sceneRadius <= objectRadius) {
                            sceneCenter = objectCenter;
                            sceneRadius = objectRadius;
                        }
                        else if (distance + objectRadius > sceneRadius) {
                            Float newRadius = Float(0.5) * (distance + sceneRadius + objectRadius);

                            sceneCenter = sceneCenter + (newRadius - sceneRadius) * sceneToObject / distance;
                            sceneRadius = newRadius;
                        }
                    }
                }

                return sceneRadius;
            }

            static void buildCDF(const Background * background, std::vector<Float> & conditionalCDF, std::vector<Float> & marginalCDF) {
                const SpectrumTexture * backgroundEmission = background->getEmission();

                int width = backgroundEmission->getWidth();
                int height = backgroundEmission->getHeight();

                conditionalCDF.resize(width * height);
                marginalCDF.resize(height);

                Float totalWeight = 0;

                for (int y = 0; y < height; y++) {
                    Float rowWeight = 0;

                    for (int x = 0; x < width; x++) {
                        rowWeight += (width == 1 && height == 1) ? backgroundEmission->getAverage() : backgroundEmission->getAverage(x, y);

                        conditionalCDF[y * width + x] = rowWeight;
                    }

                    if (rowWeight > constants::EPSILON)
                        for (int x = 0; x < width; x++) conditionalCDF[y * width + x] /= rowWeight;
                    else
                        for (int x = 0; x < width; x++) conditionalCDF[y * width + x] = (Float(x) + Float(1)) / Float(width);

                    rowWeight *= background->jacobian((Float(y) + Float(0.5)) / Float(height));

                    totalWeight += rowWeight;
                    marginalCDF[y] = totalWeight;
                }

                if (totalWeight > constants::EPSILON) {
                    for (int y = 0; y < height; y++) marginalCDF[y] /= totalWeight;
                }
                else {
                    for (int y = 0; y < height; y++) marginalCDF[y] = (Float(y) + Float(1)) / Float(height);
                }
            }

            static BVHNode<Instance> * buildGeometryBVH(Context & context) {
                std::vector<Object *> orderedObjects;
                std::vector<Instance *> orderedInstances;

                for (int i = 0; i < context.getObjectCount(); i++)
                    orderedObjects.push_back(context.getObject(i));

                for (int i = 0; i < context.getInstanceCount(); i++)
                    orderedInstances.push_back(context.getInstance(i));

                context.addOrderedObjects(orderedObjects.data(), int(std::ssize(orderedObjects)));
                context.addOrderedInstances(orderedInstances.data(), int(std::ssize(orderedInstances)));

                BVHNode<Instance> * root = nullptr;

                if (context.hasObjects() && context.hasInstances()) {
                    context.reserveNodes<Object>(2 * context.getObjectCount() - 1);
                    context.reserveNodes<Instance>(2 * context.getInstanceCount() - 1);

                    root = buildBVH<Instance>(context, 0, context.getInstanceCount(), 1);

                    std::unordered_map<const Object *, BVHNode<Object> *> objectsToNodes;

                    for (int i = 0; i < context.getObjectIndex(); i++) {
                        int start = context.getOffset(i);
                        int end = context.getOffset(i + 1);

                        if (start == end) continue;

                        objectsToNodes[context.getObject(start)] = buildBVH<Object>(context, start, end, 1);
                    }

                    for (int i = 0; i < context.getInstanceCount(); i++) {
                        Instance * instance = context.getInstance(i);

                        instance->setNode(objectsToNodes.at(instance->getObject(0)));
                    }
                }

                return root;
            }


            static BVHNode<Instance> * buildLightBVH(Context & context) {
                BVHNode<Instance> * root = nullptr;

                std::vector<Instance *> lightInstances;

                std::unordered_map<const Object *, Object *> objectsToLightObjects;

                for (int i = 0; i < context.getObjectIndex(); i++) {
                    int start = context.getOffset(i);
                    int end = context.getOffset(i + 1);

                    bool hasEmissive = false;

                    for (int j = start; j < end; j++) {
                        Object * object = context.getObject(j);
                        const Material * material = object->getMaterial();

                        if (material && material->isEmissive()) {
                            context.addLightObject(object);

                            if (!hasEmissive) objectsToLightObjects[context.getObject(start)] = object;

                            hasEmissive = true;
                        }
                    }

                    context.flushLightObjects();
                }

                int lightObjectOffset = context.getLightObjectIndex();

                std::vector<const Object *> lightInstanceObjects;

                for (int i = 0; i < context.getInstanceCount(); i++) {
                    Instance * instance = context.getInstance(i);

                    if (instance->hasMaterial()) {
                        const Material * material = instance->getMaterial(0);

                        if (material && material->isEmissive()) {
                            lightInstances.push_back(instance);

                            if (std::find(lightInstanceObjects.begin(), lightInstanceObjects.end(), instance->getObject(0)) == lightInstanceObjects.end()) {
                                lightInstanceObjects.push_back(instance->getObject(0));

                                for (int j = 0; j < instance->getCount(); j++)
                                    context.addLightObject(const_cast<Object *>(instance->getObject(j)));

                                context.flushLightObjects();
                            }
                        }
                    }
                    else {
                        for (int j = 0; j < instance->getCount(); j++) {
                            const Material * material = instance->getObject(j)->getMaterial();

                            if (material && material->isEmissive()) {
                                lightInstances.push_back(instance);

                                break;
                            }
                        }
                    }
                }

                context.addLightInstances(lightInstances.data(), int(std::ssize(lightInstances)));

                if (context.hasLightObjects() && context.hasLightInstances()) {
                    context.reserveNodes<Object>(2 * context.getLightObjectCount() - 1, true);
                    context.reserveNodes<Instance>(2 * context.getLightInstanceCount() - 1, true);

                    root = buildBVH<Instance>(context, 0, context.getLightInstanceCount(), 1, true);

                    std::unordered_map<Object *, BVHNode<Object> *> objectsToLightNodes;
                    std::unordered_map<const Object *, BVHNode<Object> *> instancedObjectsToLightNodes;

                    for (int i = 0; i < context.getLightObjectIndex(); i++) {
                        int start = context.getLightOffset(i);
                        int end = context.getLightOffset(i + 1);

                        if (start == end) continue;

                        Object * object = context.getOrdered<Object>(start, true);

                        bool materialBlind = i >= lightObjectOffset;

                        BVHNode<Object> * node = buildBVH<Object>(context, start, end, 1, true, materialBlind);

                        if (materialBlind) instancedObjectsToLightNodes[object] = node;
                        else objectsToLightNodes[object] = node;
                    }

                    for (int i = 0; i < context.getLightInstanceCount(); i++) {
                        Instance * instance = context.getOrdered<Instance>(i, true);

                        BVHNode<Object> * node = instance->hasMaterial() ? instancedObjectsToLightNodes.at(instance->getObject(0)) : objectsToLightNodes.at(objectsToLightObjects.at(instance->getObject(0)));

                        instance->setLightNode(node);
                    }
                }

                return root;
            }

            template <typename T>
            static BVHNode<T> * buildBVH(Context & context, int start, int end, int depth, bool light = false, bool materialBlind = false) {
                Vector<Float, 3> minBounds(constants::MAX, constants::MAX, constants::MAX);
                Vector<Float, 3> maxBounds(-constants::MAX, -constants::MAX, -constants::MAX);
                Vector<Float, 3> minCenter(constants::MAX, constants::MAX, constants::MAX);
                Vector<Float, 3> maxCenter(-constants::MAX, -constants::MAX, -constants::MAX);

                Float totalPower = 0;

                for (int i = start; i < end; i++) {
                    minBounds = min(minBounds, context.getOrdered<T>(i, light)->minBounds());
                    maxBounds = max(maxBounds, context.getOrdered<T>(i, light)->maxBounds());
                    minCenter = min(minCenter, context.getOrdered<T>(i, light)->center());
                    maxCenter = max(maxCenter, context.getOrdered<T>(i, light)->center());

                    if (light) {
                        if constexpr (std::is_same_v<T, Instance>) {
                            Instance * instance = context.getOrdered<Instance>(i, light);

                            for (int j = 0; j < instance->getCount(); j++)
                                totalPower += instance->area(j) * instance->getMaterial(j)->averageEmission();
                        }
                        else {
                            Object * object = context.getOrdered<Object>(i, light);

                            Float averageEmission = 1;

                            if (!materialBlind) averageEmission = object->getMaterial()->averageEmission();

                            totalPower += object->getArea() * averageEmission;
                        }
                    }
                }

                Float bestCost = constants::MAX;
                int bestAxis = 0;
                int bestSplit = -1;

                for (int i = 0; i < 3; i++)
                    if (maxCenter[i] - minCenter[i] >= constants::EPSILON) {
                        Vector<Float, 3> binMin[constants::BVH_BIN_COUNT];
                        Vector<Float, 3> binMax[constants::BVH_BIN_COUNT];
                        Float binCount[constants::BVH_BIN_COUNT];

                        for (int j = 0; j < constants::BVH_BIN_COUNT; j++) {
                            binMin[j] = Vector<Float, 3>(constants::MAX, constants::MAX, constants::MAX);
                            binMax[j] = Vector<Float, 3>(-constants::MAX, -constants::MAX, -constants::MAX);
                            binCount[j] = 0;
                        }

                        for (int j = start; j < end; j++) {
                            Vector<Float, 3> center = context.getOrdered<T>(j, light)->center();
                            int bin = std::clamp(int(constants::BVH_BIN_COUNT * (center[i] - minCenter[i]) / (maxCenter[i] - minCenter[i])), 0, constants::BVH_BIN_COUNT - 1);

                            binMin[bin] = min(binMin[bin], context.getOrdered<T>(j, light)->minBounds());
                            binMax[bin] = max(binMax[bin], context.getOrdered<T>(j, light)->maxBounds());

                            if (light) {
                                if constexpr (std::is_same_v<T, Instance>) {
                                    Instance * instance = context.getOrdered<Instance>(j, light);

                                    for (int k = 0; k < instance->getCount(); k++)
                                        binCount[bin] += instance->area(k) * instance->getMaterial(k)->averageEmission();
                                }
                                else {
                                    Object * object = context.getOrdered<Object>(j, light);

                                    Float averageEmission = 1;

                                    if (!materialBlind) averageEmission = object->getMaterial()->averageEmission();

                                    binCount[bin] += object->getArea() * averageEmission;
                                }
                            }
                            else binCount[bin]++;
                        }

                        Vector<Float, 3> leftMin[constants::BVH_BIN_COUNT];
                        Vector<Float, 3> leftMax[constants::BVH_BIN_COUNT];
                        Float leftCount[constants::BVH_BIN_COUNT];

                        Vector<Float, 3> runningMin(constants::MAX, constants::MAX, constants::MAX);
                        Vector<Float, 3> runningMax(-constants::MAX, -constants::MAX, -constants::MAX);
                        Float runningCount = 0;

                        for (int j = 0; j < constants::BVH_BIN_COUNT; j++) {
                            runningMin = min(runningMin, binMin[j]);
                            runningMax = max(runningMax, binMax[j]);
                            runningCount += binCount[j];

                            leftMin[j] = runningMin;
                            leftMax[j] = runningMax;
                            leftCount[j] = runningCount;
                        }

                        Vector<Float, 3> rightMin[constants::BVH_BIN_COUNT];
                        Vector<Float, 3> rightMax[constants::BVH_BIN_COUNT];
                        Float rightCount[constants::BVH_BIN_COUNT];

                        runningMin = Vector<Float, 3>(constants::MAX, constants::MAX, constants::MAX);
                        runningMax = Vector<Float, 3>(-constants::MAX, -constants::MAX, -constants::MAX);
                        runningCount = 0;

                        for (int j = constants::BVH_BIN_COUNT - 1; j >= 0; j--) {
                            runningMin = min(runningMin, binMin[j]);
                            runningMax = max(runningMax, binMax[j]);
                            runningCount += binCount[j];

                            rightMin[j] = runningMin;
                            rightMax[j] = runningMax;
                            rightCount[j] = runningCount;
                        }

                        for (int j = 0; j < constants::BVH_BIN_COUNT - 1; j++) {
                            if (leftCount[j] < constants::EPSILON || rightCount[j + 1] < constants::EPSILON) continue;

                            Vector<Float, 3> leftExtents = leftMax[j] - leftMin[j];
                            Vector<Float, 3> rightExtents = rightMax[j + 1] - rightMin[j + 1];
                            Vector<Float, 3> totalExtents = maxBounds - minBounds;

                            Float leftArea = 2 * (leftExtents[0] * leftExtents[1] + leftExtents[0] * leftExtents[2] + leftExtents[1] * leftExtents[2]);
                            Float rightArea = 2 * (rightExtents[0] * rightExtents[1] + rightExtents[0] * rightExtents[2] + rightExtents[1] * rightExtents[2]);
                            Float totalArea = 2 * (totalExtents[0] * totalExtents[1] + totalExtents[0] * totalExtents[2] + totalExtents[1] * totalExtents[2]);

                            Float cost = light ? std::fabs(leftCount[j] - rightCount[j + 1]) : leftArea / totalArea * Float(leftCount[j]) + rightArea / totalArea * Float(rightCount[j + 1]);

                            if (cost < bestCost) {
                                bestCost = cost;
                                bestAxis = i;
                                bestSplit = j;
                            }
                        }
                }

                if (end - start == 1 || depth >= constants::BVH_MAX_DEPTH || (!light && bestCost >= Float(end - start)) || bestSplit == -1) {
                    BVHNode<T> * node = context.addNode(BVHNode<T>(minBounds, maxBounds, context.getOrderedBase<T>(start, light), end - start), light);

                    if (light) node->setPower(totalPower);

                    return node;
                }

                Float splitPlane = minCenter[bestAxis] + Float(bestSplit + 1) * (maxCenter[bestAxis] - minCenter[bestAxis]) / Float(constants::BVH_BIN_COUNT);

                int mid = start;

                for (int i = start; i < end; i++) {
                    T * element = context.getOrdered<T>(i, light);

                    if (element->center()[bestAxis] < splitPlane) {
                        if (i != mid) {
                            T * temp = context.getOrdered<T>(mid, light);
                            context.setOrdered<T>(mid, element, light);
                            context.setOrdered<T>(i, temp, light);
                        }

                        mid++;
                    }
                }

                if (mid == start || mid == end) mid = start + (end - start) / 2;

                BVHNode<T> * node = context.addNode(BVHNode<T>(minBounds, maxBounds), light);

                buildBVH<T>(context, start, mid, depth + 1, light, materialBlind);

                BVHNode<T> * right = buildBVH<T>(context, mid, end, depth + 1, light, materialBlind);

                node->setRight(right);
                node->setPower(totalPower);

                return node;
            }
    };
}