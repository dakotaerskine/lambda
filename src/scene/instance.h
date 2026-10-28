#pragma once

#include "core/platform.h"
#include "core/utils.h"
#include "math/matrix.h"
#include "math/random.h"
#include "math/ray.h"
#include "math/transform.h"
#include "math/vector.h"
#include "scene/bvh.h"
#include "scene/material.h"
#include "scene/object.h"

namespace lambda {
    class Medium;

    class Instance {
        public:
            Instance() : objects(nullptr), count(0), node(nullptr), lightNode(nullptr), material(nullptr), medium0(nullptr), medium1(nullptr) {}

            Instance(Object * o, int c, BVHNode<Object> * n, BVHNode<Object> * ln, Material * m, Medium * m0, Medium * m1, const Vector<Float, 3> & translation, const Vector<Float, 3> & rotation, const Vector<Float, 3> & scale) : objects(o), count(c), node(n), lightNode(ln), material(m), medium0(m0), medium1(m1), transform(translation, rotation, scale) {}

            LAMBDA_HOST_DEVICE const Object * getObject(int i) const { return objects + i; }

            LAMBDA_HOST_DEVICE int getCount() const { return count; }

            LAMBDA_HOST_DEVICE const Material * getMaterial(int i) const { return material ? material : objects[i].getMaterial(); }

            LAMBDA_HOST_DEVICE bool hasMaterial() const { return material != nullptr; }

            LAMBDA_HOST_DEVICE const Medium * getMedium0(int i) const { return medium0 ? medium0 : objects[i].getMedium0(); }

            LAMBDA_HOST_DEVICE const Medium * getMedium1(int i) const { return medium1 ? medium1 : objects[i].getMedium1(); }

            LAMBDA_HOST_DEVICE Vector<Float, 3> getMin() const {
                Vector<Float, 3> bounds(constants::MAX, constants::MAX, constants::MAX);

                Vector<Float, 3> corner1 = node->getMin();
                Vector<Float, 3> corner2 = node->getMax();

                bounds = min(bounds, transform.pointToWorld(corner1));
                bounds = min(bounds, transform.pointToWorld(corner2));
                bounds = min(bounds, transform.pointToWorld(Vector<Float, 3>(corner1[0], corner1[1], corner2[2])));
                bounds = min(bounds, transform.pointToWorld(Vector<Float, 3>(corner1[0], corner2[1], corner1[2])));
                bounds = min(bounds, transform.pointToWorld(Vector<Float, 3>(corner1[0], corner2[1], corner2[2])));
                bounds = min(bounds, transform.pointToWorld(Vector<Float, 3>(corner2[0], corner1[1], corner1[2])));
                bounds = min(bounds, transform.pointToWorld(Vector<Float, 3>(corner2[0], corner1[1], corner2[2])));
                bounds = min(bounds, transform.pointToWorld(Vector<Float, 3>(corner2[0], corner2[1], corner1[2])));

                return bounds;
            }

            LAMBDA_HOST_DEVICE Vector<Float, 3> getMax() const {
                Vector<Float, 3> bounds(-constants::MAX, -constants::MAX, -constants::MAX);

                Vector<Float, 3> corner1 = node->getMin();
                Vector<Float, 3> corner2 = node->getMax();

                bounds = max(bounds, transform.pointToWorld(corner1));
                bounds = max(bounds, transform.pointToWorld(corner2));
                bounds = max(bounds, transform.pointToWorld(Vector<Float, 3>(corner1[0], corner1[1], corner2[2])));
                bounds = max(bounds, transform.pointToWorld(Vector<Float, 3>(corner1[0], corner2[1], corner1[2])));
                bounds = max(bounds, transform.pointToWorld(Vector<Float, 3>(corner1[0], corner2[1], corner2[2])));
                bounds = max(bounds, transform.pointToWorld(Vector<Float, 3>(corner2[0], corner1[1], corner1[2])));
                bounds = max(bounds, transform.pointToWorld(Vector<Float, 3>(corner2[0], corner1[1], corner2[2])));
                bounds = max(bounds, transform.pointToWorld(Vector<Float, 3>(corner2[0], corner2[1], corner1[2])));

                return bounds;
            }

            LAMBDA_HOST_DEVICE Vector<Float, 3> center() const { return transform.pointToWorld(Float(0.5) * (node->getMin() + node->getMax())); }

            LAMBDA_HOST_DEVICE Vector<Float, 3> center(int i) const { return transform.pointToWorld(objects[i].center()); }

            LAMBDA_HOST_DEVICE Float radius(int i) const { return objects[i].radius() * std::fmax(transform.scale(0), std::fmax(transform.scale(1), transform.scale(2))); }

            LAMBDA_HOST_DEVICE Float area(int i) const { return objects[i].getArea() * transform.scaleSquared(0); }

            LAMBDA_HOST_DEVICE Float getPower() const { return lightNode->getPower() * transform.scaleSquared(0) * (material ? material->averageEmission() : 1); }

            LAMBDA_HOST_DEVICE Float getPower(int i) const {
                const Material * currentMaterial = getMaterial(i);

                if (!currentMaterial) return 0;

                return area(i) * currentMaterial->averageEmission();
            }

            LAMBDA_HOST_DEVICE Float probabilityDensity(int i, const Vector<Float, 3> & point, const Vector<Float, 3> & direction) const { return objects[i].probabilityDensity(transform.pointToLocal(point), normalize(transform.vectorToLocal(direction))); }

            LAMBDA_HOST_DEVICE bool intersect(int i, Intersection & intersection, const Ray & r) const {
                if (objects[i].intersect(intersection, transform.rayToLocal(r))) {
                    toWorld(intersection, r, i);

                    return true;
                }

                return false;
            }

            LAMBDA_HOST_DEVICE bool hit(Intersection & intersection, const Ray & r) const { return hit<false>(intersection, r, nullptr, -1); }

            LAMBDA_HOST_DEVICE bool hit(Intersection & intersection, const Ray & r, const Instance * occludedInstance, int occludedObjectIndex) const { return hit<true>(intersection, r, occludedInstance, occludedObjectIndex); }

            LAMBDA_HOST_DEVICE Vector<Float, 3> sampleLight(const Vector<Float, 3> & point, Float & probabilityDensity, int & objectIndex, Random & state) const {
                const BVHNode<Object> * current = lightNode;

                Float totalPower = current->getPower();

                Float targetPower = totalPower * utils::randomFloat(state);

                while (!current->isLeaf()) {
                    const BVHNode<Object> * left = current + 1;
                    const BVHNode<Object> * right = current + current->getCount();

                    Float leftPower = left->getPower();

                    if (targetPower < leftPower) current = left;
                    else {
                        targetPower -= leftPower;

                        current = right;
                    }
                }

                for (int i = 0; i < current->getCount(); i++) {
                    objectIndex = int(current->getElement(i) - objects);

                    Float objectPower = objects[objectIndex].getArea() * (material ? 1 : objects[objectIndex].getMaterial()->averageEmission());

                    if (targetPower < objectPower || i == current->getCount() - 1) break;

                    targetPower -= objectPower;
                }

                return normalize(transform.vectorToWorld(objects[objectIndex].sample(transform.pointToLocal(point), probabilityDensity, state)));
            }

        private:
            Object * objects;
            int count;
            BVHNode<Object> * node, * lightNode;
            Material * material;
            Medium * medium0, * medium1;
            Transform transform;

            LAMBDA_HOST_DEVICE void toWorld(Intersection & intersection, const Ray & r, int i) const {
                intersection.instance = this;
                intersection.i = i;
                intersection.transformedPoint = transform.pointToWorld(intersection.point);
                intersection.transformedNormal = transform.normalToWorld(intersection.normal);
                intersection.transformedTangent = normalize(transform.vectorToWorld(intersection.tangent));
                intersection.geometricNormal = transform.normalToWorld(intersection.geometricNormal);
                intersection.pointError = utils::pointError(r.getOrigin(), intersection.point, intersection.transformedPoint, transform.scaleSquared());
                intersection.useTransformed = material != nullptr;
                intersection.frontFacing = dot(r.getDirection(), intersection.transformedNormal) < 0;
                if (!intersection.frontFacing) intersection.transformedNormal *= -1;
                if (dot(r.getDirection(), intersection.geometricNormal) >= 0) intersection.geometricNormal *= -1;
            }

            template <bool occluded>
            LAMBDA_HOST_DEVICE bool hit(Intersection & intersection, const Ray & r, const Instance * occludedInstance, int occludedObjectIndex) const {
                const BVHNode<Object> * stack[constants::BVH_MAX_DEPTH + 1];
                int stackSize = 0;

                stack[stackSize++] = node;

                Ray transformedRay = transform.rayToLocal(r);

                bool hitObject = false;

                while (stackSize > 0) {
                    const BVHNode<Object> * current = stack[--stackSize];

                    if (!current->intersect(intersection, transformedRay)) continue;

                    if (current->isLeaf())
                        for (int j = 0; j < current->getCount(); j++) {
                            const Object * object = current->getElement(j);

                            if constexpr (occluded) {
                                if ((this == occludedInstance && int(object - objects) == occludedObjectIndex) || (!material && !object->getMaterial())) continue;

                                if (object->intersect(intersection, transformedRay)) return true;
                            }
                            else if (object->intersect(intersection, transformedRay)) hitObject = true;
                        }
                    else {
                        stack[stackSize++] = current + 1;
                        stack[stackSize++] = current + current->getCount();
                    }
                }

                if (hitObject) toWorld(intersection, r, int(intersection.object - objects));

                return hitObject;
            }
    };
}