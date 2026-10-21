#pragma once

#include <cmath>

#include "core/platform.h"
#include "math/vector.h"
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
            static Float computeSceneRadius(const std::vector<Instance *> & instances) {
                Vector<Float, 3> sceneCenter = instances.empty() ? Vector<Float, 3>() : instances[0]->center(0);
                Float sceneRadius = instances.empty() ? 1 : instances[0]->radius(0);

                for (int i = 0; i < std::ssize(instances); i++) {
                    for (int j = 0; j < instances[i]->getCount(); j++) {
                        Vector<Float, 3> objectCenter = instances[i]->center(j);
                        Float objectRadius = instances[i]->radius(j);

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

            static void buildCDF(const Background & background, std::vector<Float> & conditionalCDF, std::vector<Float> & marginalCDF) {
                const SpectrumTexture * backgroundEmission = background.getEmission();

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

                    if (rowWeight > 0)
                        for (int x = 0; x < width; x++) conditionalCDF[y * width + x] /= rowWeight;
                    else
                        for (int x = 0; x < width; x++) conditionalCDF[y * width + x] = (Float(x) + Float(1)) / Float(width);

                    rowWeight *= background.jacobian((Float(y) + Float(0.5)) / Float(height));

                    totalWeight += rowWeight;
                    marginalCDF[y] = totalWeight;
                }

                if (totalWeight > 0) {
                    for (int y = 0; y < height; y++) marginalCDF[y] /= totalWeight;
                }
                else {
                    for (int y = 0; y < height; y++) marginalCDF[y] = (Float(y) + Float(1)) / Float(height);
                }
            }

            template <typename T, bool light>
            requires (std::is_same_v<T, Object> || std::is_same_v<T, Instance>)
            static std::vector<BVHNode<T>> buildBVH(T ** elements, int n, bool materialBlind = false) {
                if (n <= 0) return std::vector<BVHNode<T>>();

                return buildBVH<T, light>(elements, n, 1, materialBlind);
            }

        private:
            template <typename T, bool light>
            requires (std::is_same_v<T, Object> || std::is_same_v<T, Instance>)
            static std::vector<BVHNode<T>> buildBVH(T ** elements, int n, int depth, bool materialBlind) {
                std::vector<BVHNode<T>> nodes;

                Vector<Float, 3> minBounds(constants::MAX, constants::MAX, constants::MAX);
                Vector<Float, 3> maxBounds(-constants::MAX, -constants::MAX, -constants::MAX);
                Vector<Float, 3> minCenter(constants::MAX, constants::MAX, constants::MAX);
                Vector<Float, 3> maxCenter(-constants::MAX, -constants::MAX, -constants::MAX);

                Float totalPower = 0;

                for (int i = 0; i < n; i++) {
                    minBounds = min(minBounds, elements[i]->minBounds());
                    maxBounds = max(maxBounds, elements[i]->maxBounds());
                    minCenter = min(minCenter, elements[i]->center());
                    maxCenter = max(maxCenter, elements[i]->center());

                    if constexpr (light) totalPower += elementPower(elements[i], materialBlind);
                }

                Float bestCost = constants::MAX;
                int bestAxis = 0;
                int bestSplit = -1;

                for (int i = 0; i < 3; i++)
                    if (maxCenter[i] - minCenter[i] > 0) {
                        Vector<Float, 3> binMin[constants::BVH_BIN_COUNT];
                        Vector<Float, 3> binMax[constants::BVH_BIN_COUNT];
                        Float binCount[constants::BVH_BIN_COUNT];

                        for (int j = 0; j < constants::BVH_BIN_COUNT; j++) {
                            binMin[j] = Vector<Float, 3>(constants::MAX, constants::MAX, constants::MAX);
                            binMax[j] = Vector<Float, 3>(-constants::MAX, -constants::MAX, -constants::MAX);
                            binCount[j] = 0;
                        }

                        for (int j = 0; j < n; j++) {
                            Vector<Float, 3> center = elements[j]->center();
                            int bin = std::clamp(int(constants::BVH_BIN_COUNT * (center[i] - minCenter[i]) / (maxCenter[i] - minCenter[i])), 0, constants::BVH_BIN_COUNT - 1);

                            binMin[bin] = min(binMin[bin], elements[j]->minBounds());
                            binMax[bin] = max(binMax[bin], elements[j]->maxBounds());

                            if constexpr (light) binCount[bin] += elementPower(elements[j], materialBlind);
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
                            if (leftCount[j] <= 0 || rightCount[j + 1] <= 0) continue;

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

                if (n == 1 || depth >= constants::BVH_MAX_DEPTH || (!light && bestCost >= Float(n)) || bestSplit == -1) {
                    nodes.push_back(BVHNode<T>(minBounds, maxBounds, elements, n, totalPower));

                    return nodes;
                }

                Float splitPlane = minCenter[bestAxis] + Float(bestSplit + 1) * (maxCenter[bestAxis] - minCenter[bestAxis]) / Float(constants::BVH_BIN_COUNT);

                int mid = 0;

                for (int i = 0; i < n; i++) {
                    T * element = elements[i];

                    if (element->center()[bestAxis] < splitPlane) {
                        if (i != mid) {
                            elements[i] = elements[mid];
                            elements[mid] = element;
                        }

                        mid++;
                    }
                }

                if (mid == 0 || mid == n) mid = n / 2;

                std::vector<BVHNode<T>> leftNodes = buildBVH<T, light>(elements, mid, depth + 1, materialBlind);
                std::vector<BVHNode<T>> rightNodes = buildBVH<T, light>(elements + mid, n - mid, depth + 1, materialBlind);

                nodes.push_back(BVHNode<T>(minBounds, maxBounds, int(std::ssize(leftNodes)) + 1, totalPower));

                nodes.insert(nodes.end(), leftNodes.begin(), leftNodes.end());
                nodes.insert(nodes.end(), rightNodes.begin(), rightNodes.end());

                return nodes;
            }

            template <typename T>
            static Float elementPower(const T * element, bool materialBlind) {
                if constexpr (std::is_same_v<T, Instance>) return element->getPower();
                else {
                    Float averageEmission = 1;

                    if (!materialBlind) averageEmission = element->getMaterial()->averageEmission();

                    return element->getArea() * averageEmission;
                }
            }
    };
}