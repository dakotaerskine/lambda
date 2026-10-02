#pragma once

#include "core/platform.h"
#include "core/utils.h"
#include "math/matrix.h"
#include "math/random.h"
#include "math/ray.h"
#include "math/vector.h"
#include "scene/bvh.h"
#include "scene/object.h"

namespace lambda {
    class Material;
    class Medium;

    class Instance {
        public:
            Instance() : objects(nullptr), count(0), node(nullptr), lightNode(nullptr), material(nullptr), medium0(nullptr), medium1(nullptr) {}

            Instance(Object * o, int c, Material * m, Medium * m0, Medium * m1, const Vector<Float, 3> & translation, const Vector<Float, 3> & rotation, const Vector<Float, 3> & scale) : objects(o), count(c), node(nullptr), lightNode(nullptr), material(m), medium0(m0), medium1(m1) {
                Matrix<Float, 4> rotationMatrix = utils::rotationMatrix(rotation);

                transformMatrix = utils::translationMatrix(translation) * rotationMatrix * utils::scaleMatrix(scale);
                inverseTransformMatrix = utils::scaleMatrix(Vector<Float, 3>(1 / scale[0], 1 / scale[1], 1 / scale[2])) * transpose(rotationMatrix) * utils::translationMatrix(Vector<Float, 3>(-translation));
                normalMatrix = Matrix<Float, 3>(transpose(inverseTransformMatrix));
            }

            void setNode(BVHNode<Object> * n) { node = n; }

            void setLightNode(BVHNode<Object> * n) { lightNode = n; }

            LAMBDA_HOST_DEVICE const Object * getObject(int i) const { return objects + i; }

            LAMBDA_HOST_DEVICE int getCount() const { return count; }

            LAMBDA_HOST_DEVICE const BVHNode<Object> * getNode() const { return node; }

            LAMBDA_HOST_DEVICE const BVHNode<Object> * getLightNode() const { return lightNode; }

            LAMBDA_HOST_DEVICE const Material * getMaterial(int i) const { return material != nullptr ? material : objects[i].getMaterial(); }

            LAMBDA_HOST_DEVICE const Medium * getMedium0(int i) const { return medium0 != nullptr ? medium0 : objects[i].getMedium0(); }

            LAMBDA_HOST_DEVICE const Medium * getMedium1(int i) const { return medium1 != nullptr ? medium1 : objects[i].getMedium1(); }

            LAMBDA_HOST_DEVICE bool hasMaterial() const { return material != nullptr; }

            LAMBDA_HOST_DEVICE Float getScale(int i) const { return Vector<Float, 3>(transformMatrix.get(0, i), transformMatrix.get(1, i), transformMatrix.get(2, i)).length(); }

            LAMBDA_HOST_DEVICE Float getScaleSquared(int i) const { return Vector<Float, 3>(transformMatrix.get(0, i), transformMatrix.get(1, i), transformMatrix.get(2, i)).lengthSquared();}

            LAMBDA_HOST_DEVICE Vector<Float, 3> transformPointToLocal(const Vector<Float, 3> & p) const { return Vector<Float, 3>(inverseTransformMatrix * Vector<Float, 4>(p, 1)); }

            LAMBDA_HOST_DEVICE Vector<Float, 3> transformPointToWorld(const Vector<Float, 3> & p) const { return Vector<Float, 3>(transformMatrix * Vector<Float, 4>(p, 1)); }

            LAMBDA_HOST_DEVICE Vector<Float, 3> transformVectorToLocal(const Vector<Float, 3> & v) const { return Vector<Float, 3>(inverseTransformMatrix * Vector<Float, 4>(v, 0)); }

            LAMBDA_HOST_DEVICE Vector<Float, 3> transformVectorToWorld(const Vector<Float, 3> & v) const { return Vector<Float, 3>(transformMatrix * Vector<Float, 4>(v, 0)); }

            LAMBDA_HOST_DEVICE Vector<Float, 3> transformNormalToWorld(const Vector<Float, 3> & n) const { return normalize(normalMatrix * n); }

            LAMBDA_HOST_DEVICE Ray transformRayToLocal(const Ray & r) const { return Ray(transformPointToLocal(r.getOrigin()), transformVectorToLocal(r.getDirection()), r.getLambdas(), r.getMedium()); }

            LAMBDA_HOST_DEVICE Vector<Float, 3> minBounds() const {
                Vector<Float, 3> bounds(constants::MAX, constants::MAX, constants::MAX);

                for (int i = 0; i < count; i++) {
                    Vector<Float, 3> corner1 = objects[i].minBounds();
                    Vector<Float, 3> corner2 = objects[i].maxBounds();

                    bounds = min(bounds, transformPointToWorld(corner1));
                    bounds = min(bounds, transformPointToWorld(corner2));
                    bounds = min(bounds, transformPointToWorld(Vector<Float, 3>(corner1[0], corner1[1], corner2[2])));
                    bounds = min(bounds, transformPointToWorld(Vector<Float, 3>(corner1[0], corner2[1], corner1[2])));
                    bounds = min(bounds, transformPointToWorld(Vector<Float, 3>(corner1[0], corner2[1], corner2[2])));
                    bounds = min(bounds, transformPointToWorld(Vector<Float, 3>(corner2[0], corner1[1], corner1[2])));
                    bounds = min(bounds, transformPointToWorld(Vector<Float, 3>(corner2[0], corner1[1], corner2[2])));
                    bounds = min(bounds, transformPointToWorld(Vector<Float, 3>(corner2[0], corner2[1], corner1[2])));
                }

                return bounds;
            }

            LAMBDA_HOST_DEVICE Vector<Float, 3> maxBounds() const {
                Vector<Float, 3> bounds(-constants::MAX, -constants::MAX, -constants::MAX);

                for (int i = 0; i < count; i++) {
                    Vector<Float, 3> corner1 = objects[i].minBounds();
                    Vector<Float, 3> corner2 = objects[i].maxBounds();

                    bounds = max(bounds, transformPointToWorld(corner1));
                    bounds = max(bounds, transformPointToWorld(corner2));
                    bounds = max(bounds, transformPointToWorld(Vector<Float, 3>(corner1[0], corner1[1], corner2[2])));
                    bounds = max(bounds, transformPointToWorld(Vector<Float, 3>(corner1[0], corner2[1], corner1[2])));
                    bounds = max(bounds, transformPointToWorld(Vector<Float, 3>(corner1[0], corner2[1], corner2[2])));
                    bounds = max(bounds, transformPointToWorld(Vector<Float, 3>(corner2[0], corner1[1], corner1[2])));
                    bounds = max(bounds, transformPointToWorld(Vector<Float, 3>(corner2[0], corner1[1], corner2[2])));
                    bounds = max(bounds, transformPointToWorld(Vector<Float, 3>(corner2[0], corner2[1], corner1[2])));
                }

                return bounds;
            }

            LAMBDA_HOST_DEVICE Vector<Float, 3> center() const {
                Vector<Float, 3> center(0, 0, 0);

                for (int i = 0; i < count; i++) center += transformPointToWorld(objects[i].center());

                return center / Float(count);
            }

            LAMBDA_HOST_DEVICE Vector<Float, 3> center(int i) const { return transformPointToWorld(objects[i].center()); }

            LAMBDA_HOST_DEVICE Float radius(int i) const { return objects[i].radius() * std::fmax(getScale(0), std::fmax(getScale(1), getScale(2))); }

            LAMBDA_HOST_DEVICE Float area(int i) const { return objects[i].getArea() * getScaleSquared(0); }

            LAMBDA_HOST_DEVICE Float pdf(int i, const Vector<Float, 3> & point, const Vector<Float, 3> & direction) const { return objects[i].pdf(transformPointToLocal(point), normalize(transformVectorToLocal(direction))); }

            LAMBDA_HOST_DEVICE Vector<Float, 3> sample(int i, const Vector<Float, 3> & point, Random & state) const { return normalize(transformVectorToWorld(objects[i].sample(transformPointToLocal(point), state))); }

        private:
            Object * objects;
            int count;
            BVHNode<Object> * node, * lightNode;
            Material * material;
            Medium * medium0, * medium1;
            Matrix<Float, 4> transformMatrix, inverseTransformMatrix;
            Matrix<Float, 3> normalMatrix;
    };
}