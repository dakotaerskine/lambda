#pragma once

#include <algorithm>
#include <numeric>
#include <vector>

#include "core/constants.h"
#include "core/platform.h"
#include "math/vector.h"
#include "scene/object.h"

class BVHNode {
    public:
        HOST_DEVICE BVHNode(const Vector & _min, const Vector & _max, int _index, int _count = 0) : min(_min), max(_max), index(_index), count(_count) {}

        HOST_DEVICE const Vector & getMin() const { return min; }
        HOST_DEVICE const Vector & getMax() const { return max; }

        HOST_DEVICE bool isLeaf() const { return count > 0; }

        HOST_DEVICE int getCount() const { return count; }

        HOST_DEVICE int getRight() const { return isLeaf() ? -1 : index; }
        HOST_DEVICE int getObject() const { return isLeaf() ? index : -1; }

        HOST_DEVICE void setRight(int _right) { index = _right; }

        HOST_DEVICE bool intersect(const Ray & r, Intersection & intersection) const {
            Float t1 = 0, t2 = MAX;

            for (int i = 0; i < 3; i++) {
                Float inverseDirection = 1 / r.getDirection()[i];

                Float tMin = (min[i] - r.getOrigin()[i]) * inverseDirection;
                Float tMax = (max[i] - r.getOrigin()[i]) * inverseDirection;

                if (inverseDirection < 0) {
                    Float temp = tMin;
                    tMin = tMax;
                    tMax = temp;
                }

                t1 = fmaxF(tMin, t1);
                t2 = fminF(tMax, t2);
            }

            intersection.t = t1;

            return t1 <= t2 && t2 >= 0;
        }

    private:
        Vector min, max;
        int index, count;
};

#ifndef __CUDA_ARCH__
    class BVH {
        public:
            static std::vector<BVHNode> makeBVH(std::vector<Object> & objects) {
                std::vector<BVHNode> nodes;

                nodes.reserve(2 * objects.size() - 1);

                std::vector<int> indices(objects.size());

                std::iota(indices.begin(), indices.end(), 0);

                makeBVH(objects, indices, 0, int(objects.size()), nodes, 1);

                std::vector<Object> orderedObjects;

                orderedObjects.reserve(objects.size());

                for (int i = 0; i < int(indices.size()); i++)
                    orderedObjects.push_back(objects[indices[i]]);

                objects = std::move(orderedObjects);

                return nodes;
            }

        private:
            static int makeBVH(const std::vector<Object> & objects, std::vector<int> & indices, int start, int end, std::vector<BVHNode> & nodes, int depth) {
                Vector min = objects[indices[start]].min();
                Vector max = objects[indices[start]].max();
                Vector minCenter = objects[indices[start]].center();
                Vector maxCenter = objects[indices[start]].center();

                for (int i = start + 1; i < end; i++) {
                    min = minV(min, objects[indices[i]].min());
                    max = maxV(max, objects[indices[i]].max());
                    minCenter = minV(minCenter, objects[indices[i]].center());
                    maxCenter = maxV(maxCenter, objects[indices[i]].center());
                }

                Float bestCost = MAX;
                int bestAxis = 0;
                int bestSplit = -1;

                for (int i = 0; i < 3; i++)
                    if (maxCenter[i] - minCenter[i] >= EPSILON) {
                        Vector binMin[BVH_BIN_COUNT];
                        Vector binMax[BVH_BIN_COUNT];
                        int binCount[BVH_BIN_COUNT];

                        for (int j = 0; j < BVH_BIN_COUNT; j++) {
                            binMin[j] = Vector(MAX, MAX, MAX);
                            binMax[j] = Vector(-MAX, -MAX, -MAX);
                            binCount[j] = 0;
                        }

                        for (int j = start; j < end; j++) {
                            Vector center = objects[indices[j]].center();
                            int bin = std::clamp(int(BVH_BIN_COUNT * (center[i] - minCenter[i]) / (maxCenter[i] - minCenter[i])), 0, BVH_BIN_COUNT - 1);

                            binMin[bin] = minV(binMin[bin], objects[indices[j]].min());
                            binMax[bin] = maxV(binMax[bin], objects[indices[j]].max());
                            binCount[bin]++;
                        }

                        Vector leftMin[BVH_BIN_COUNT];
                        Vector leftMax[BVH_BIN_COUNT];
                        int leftCount[BVH_BIN_COUNT];

                        Vector runningMin(MAX, MAX, MAX);
                        Vector runningMax(-MAX, -MAX, -MAX);
                        int runningCount = 0;

                        for (int j = 0; j < BVH_BIN_COUNT; j++) {
                            runningMin = minV(runningMin, binMin[j]);
                            runningMax = maxV(runningMax, binMax[j]);
                            runningCount += binCount[j];

                            leftMin[j] = runningMin;
                            leftMax[j] = runningMax;
                            leftCount[j] = runningCount;
                        }

                        Vector rightMin[BVH_BIN_COUNT];
                        Vector rightMax[BVH_BIN_COUNT];
                        int rightCount[BVH_BIN_COUNT];

                        runningMin = Vector(MAX, MAX, MAX);
                        runningMax = Vector(-MAX, -MAX, -MAX);
                        runningCount = 0;

                        for (int j = BVH_BIN_COUNT - 1; j >= 0; j--) {
                            runningMin = minV(runningMin, binMin[j]);
                            runningMax = maxV(runningMax, binMax[j]);
                            runningCount += binCount[j];

                            rightMin[j] = runningMin;
                            rightMax[j] = runningMax;
                            rightCount[j] = runningCount;
                        }

                        for (int j = 0; j < BVH_BIN_COUNT - 1; j++) {
                            if (leftCount[j] == 0 || rightCount[j + 1] == 0) continue;

                            Vector leftExtents = leftMax[j] - leftMin[j];
                            Vector rightExtents = rightMax[j + 1] - rightMin[j + 1];
                            Vector totalExtents = max - min;

                            Float leftArea = 2 * (leftExtents[0] * leftExtents[1] + leftExtents[0] * leftExtents[2] + leftExtents[1] * leftExtents[2]);
                            Float rightArea = 2 * (rightExtents[0] * rightExtents[1] + rightExtents[0] * rightExtents[2] + rightExtents[1] * rightExtents[2]);
                            Float totalArea = 2 * (totalExtents[0] * totalExtents[1] + totalExtents[0] * totalExtents[2] + totalExtents[1] * totalExtents[2]);

                            Float cost = leftArea / totalArea * Float(leftCount[j]) + rightArea / totalArea * Float(rightCount[j + 1]);

                            if (cost < bestCost) {
                                bestCost = cost;
                                bestAxis = i;
                                bestSplit = j;
                            }
                        }
                }

                if (end - start == 1 || depth >= BVH_MAX_DEPTH || bestCost >= Float(end - start) || bestSplit == -1) {
                    nodes.push_back(BVHNode(min, max, start, end - start));

                    return int(nodes.size()) - 1;
                }

                Float splitPlane = minCenter[bestAxis] + Float(bestSplit + 1) * (maxCenter[bestAxis] - minCenter[bestAxis]) / Float(BVH_BIN_COUNT);

                typename std::vector<int>::iterator iterator = std::partition(indices.begin() + start, indices.begin() + end, [&](int i) { return objects[i].center()[bestAxis] < splitPlane; });

                int mid = int(std::distance(indices.begin(), iterator));

                if (mid == start || mid == end) mid = start + (end - start) / 2;

                int current = int(nodes.size());

                nodes.push_back(BVHNode(min, max, -1));

                makeBVH(objects, indices, start, mid, nodes, depth + 1);
                
                int right = makeBVH(objects, indices, mid, end, nodes, depth + 1);

                nodes[current].setRight(right);

                return current;
            }
    };
#endif