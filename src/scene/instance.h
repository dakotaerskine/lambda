#pragma once

#include "core/platform.h"
#include "core/utils.h"
#include "math/matrix.h"
#include "math/random.h"
#include "math/ray.h"
#include "math/vector.h"
#include "scene/object.h"

class Instance {
    public:
        HOST_DEVICE Instance(int o, int c, int m, const Vector<Float> & translation, const Vector<Float> & rotation, const Vector<Float> & scale) : object(o), count(c), material(m) {
            Matrix4<Float> translateMatrix = Matrix4<Float>(Float(1), Float(0), Float(0), translation[0], Float(0), Float(1), Float(0), translation[1], Float(0), Float(0), Float(1), translation[2], Float(0), Float(0), Float(0), Float(1));

            Float sinX = sin(rotation[0]), cosX = cos(rotation[0]);
            Float sinY = sin(rotation[1]), cosY = cos(rotation[1]);
            Float sinZ = sin(rotation[2]), cosZ = cos(rotation[2]);

            Matrix4<Float> rotateMatrixX = Matrix4<Float>(Float(1), Float(0), Float(0), Float(0), Float(0), cosX, -sinX, Float(0), Float(0), sinX, cosX, Float(0), Float(0), Float(0), Float(0), Float(1));
            Matrix4<Float> rotateMatrixY = Matrix4<Float>(cosY, Float(0), sinY, Float(0), Float(0), 1, Float(0), Float(0), -sinY, Float(0), cosY, Float(0), Float(0), Float(0), Float(0), Float(1));
            Matrix4<Float> rotateMatrixZ = Matrix4<Float>(cosZ, -sinZ, Float(0), Float(0), sinZ, cosZ, Float(0), Float(0), Float(0), Float(0), 1, Float(0), Float(0), Float(0), Float(0), Float(1));

            Matrix4<Float> scaleMatrix = Matrix4<Float>(scale[0], Float(0), Float(0), Float(0), Float(0), scale[1], Float(0), Float(0), Float(0), Float(0), scale[2], Float(0), Float(0), Float(0), Float(0), Float(1));

            transformMatrix = translateMatrix * rotateMatrixZ * rotateMatrixY * rotateMatrixX * scaleMatrix;
            inverseTransformMatrix = inverse(transformMatrix);
            normalMatrix = transpose(inverseTransformMatrix);

            normalMatrix.get(0, 3) = Float(0);
            normalMatrix.get(1, 3) = Float(0);
            normalMatrix.get(2, 3) = Float(0);
            normalMatrix.get(3, 0) = Float(0);
            normalMatrix.get(3, 1) = Float(0);
            normalMatrix.get(3, 2) = Float(0);
            normalMatrix.get(3, 3) = Float(1);
        }

        HOST_DEVICE void setNode(int n) { node = n; }

        HOST_DEVICE int getObject() const { return object; }

        HOST_DEVICE int getCount() const { return count; }

        HOST_DEVICE int getNode() const { return node; }

        HOST_DEVICE int getMaterial(const Object * const objects, int i) const { return material >= 0 ? material : objects[object + i].getMaterial(); }

        HOST_DEVICE Vector<Float> transformPointToLocal(const Vector<Float> & p) const { return transform(inverseTransformMatrix, p, Float(1)); }

        HOST_DEVICE Vector<Float> transformPointToWorld(const Vector<Float> & p) const { return transform(transformMatrix, p, Float(1)); }

        HOST_DEVICE Vector<Float> transformVectorToLocal(const Vector<Float> & n) const { return transform(inverseTransformMatrix, n, Float(0)); }

        HOST_DEVICE Vector<Float> transformVectorToWorld(const Vector<Float> & n) const { return transform(transformMatrix, n, Float(0)); }

        HOST_DEVICE Vector<Float> transformNormalToWorld(const Vector<Float> & n) const { return normalize(transform(normalMatrix, n, Float(0))); }

        HOST_DEVICE Ray transformRayToLocal(const Ray & r) const {
            return Ray(transformPointToLocal(r.getOrigin()), transformVectorToLocal(r.getDirection()), r.getLambdas());
        }

        HOST_DEVICE Vector<Float> min(const Object * const objects) const {
            Vector<Float> min(MAX, MAX, MAX);

            for (int i = 0; i < count; i++) {
                Vector<Float> corner1 = objects[object + i].min();
                Vector<Float> corner2 = objects[object + i].max();

                min = minV(min, transformPointToWorld(corner1));
                min = minV(min, transformPointToWorld(corner2));
                min = minV(min, transformPointToWorld(Vector<Float>(corner1[0], corner1[1], corner2[2])));
                min = minV(min, transformPointToWorld(Vector<Float>(corner1[0], corner2[1], corner1[2])));
                min = minV(min, transformPointToWorld(Vector<Float>(corner1[0], corner2[1], corner2[2])));
                min = minV(min, transformPointToWorld(Vector<Float>(corner2[0], corner1[1], corner1[2])));
                min = minV(min, transformPointToWorld(Vector<Float>(corner2[0], corner1[1], corner2[2])));
                min = minV(min, transformPointToWorld(Vector<Float>(corner2[0], corner2[1], corner1[2])));
            }

            return min;
        }

        HOST_DEVICE Vector<Float> max(const Object * const objects) const {
            Vector<Float> max(-MAX, -MAX, -MAX);

            for (int i = 0; i < count; i++) {
                Vector<Float> corner1 = objects[object + i].min();
                Vector<Float> corner2 = objects[object + i].max();

                max = maxV(max, transformPointToWorld(corner1));
                max = maxV(max, transformPointToWorld(corner2));
                max = maxV(max, transformPointToWorld(Vector<Float>(corner1[0], corner1[1], corner2[2])));
                max = maxV(max, transformPointToWorld(Vector<Float>(corner1[0], corner2[1], corner1[2])));
                max = maxV(max, transformPointToWorld(Vector<Float>(corner1[0], corner2[1], corner2[2])));
                max = maxV(max, transformPointToWorld(Vector<Float>(corner2[0], corner1[1], corner1[2])));
                max = maxV(max, transformPointToWorld(Vector<Float>(corner2[0], corner1[1], corner2[2])));
                max = maxV(max, transformPointToWorld(Vector<Float>(corner2[0], corner2[1], corner1[2])));
            }

            return max;
        }

        HOST_DEVICE Vector<Float> center(const Object * const objects) const {
            Vector<Float> center(0, 0, 0);

            for (int i = 0; i < count; i++)
                center += transformPointToWorld(objects[object + i].center());

            return center / Float(count);
        }

        HOST_DEVICE Vector<Float> center(const Object * const objects, int i) const { return transformPointToWorld(objects[object + i].center()); }

        HOST_DEVICE Float radius(const Object * const objects, int i) const { return objects[object + i].radius() * Vector<Float>(transformMatrix.get(0, 0), transformMatrix.get(1, 0), transformMatrix.get(2, 0)).length(); }

        HOST_DEVICE Float area(const Object * const objects, int i) const { return objects[object + i].area() * Vector<Float>(transformMatrix.get(0, 0), transformMatrix.get(1, 0), transformMatrix.get(2, 0)).lengthSquared(); }

        HOST_DEVICE Float pdf(const Object * const objects, int i, const Vector<Float> & point, const Vector<Float> & direction) const { return objects[object + i].pdf(transformPointToLocal(point), normalize(transformVectorToLocal(direction))); }

        HOST_DEVICE Vector<Float> sample(const Object * const objects, int i, const Vector<Float> & point, Random & state) const { return normalize(transformVectorToWorld(objects[object + i].sample(transformPointToLocal(point), state))); }

    private:
        int object, count, node, material;
        Matrix4<Float> transformMatrix, inverseTransformMatrix, normalMatrix;
};