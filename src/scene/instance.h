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
        HOST_DEVICE Instance(int o, int c, int m, const Vector & translation, const Vector & rotation, const Vector & scale) : object(o), count(c), material(m) {
            Matrix4<Float> translateMatrix = Matrix4<Float>(Float(1), Float(0), Float(0), translation[0], Float(0), Float(1), Float(0), translation[1], Float(0), Float(0), Float(1), translation[2], Float(0), Float(0), Float(0), Float(1));

            Float sinX = sinF(rotation[0]), cosX = cosF(rotation[0]);
            Float sinY = sinF(rotation[1]), cosY = cosF(rotation[1]);
            Float sinZ = sinF(rotation[2]), cosZ = cosF(rotation[2]);

            Matrix4<Float> rotateMatrixX = Matrix4<Float>(Float(1), Float(0), Float(0), Float(0), Float(0), cosX, -sinX, Float(0), Float(0), sinX, cosX, Float(0), Float(0), Float(0), Float(0), Float(1));
            Matrix4<Float> rotateMatrixY = Matrix4<Float>(cosY, Float(0), sinY, Float(0), Float(0), 1, Float(0), Float(0), -sinY, Float(0), cosY, Float(0), Float(0), Float(0), Float(0), Float(1));
            Matrix4<Float> rotateMatrixZ = Matrix4<Float>(cosZ, -sinZ, Float(0), Float(0), sinZ, cosZ, Float(0), Float(0), Float(0), Float(0), 1, Float(0), Float(0), Float(0), Float(0), Float(1));

            Matrix4<Float> scaleMatrix = Matrix4<Float>(scale[0], Float(0), Float(0), Float(0), Float(0), scale[1], Float(0), Float(0), Float(0), Float(0), scale[2], Float(0), Float(0), Float(0), Float(0), Float(1));

            transformMatrix = translateMatrix * rotateMatrixZ * rotateMatrixY * rotateMatrixX * scaleMatrix;
            inverseTransformMatrix = inverse(transformMatrix);
        }

        HOST_DEVICE void setNode(int n) { node = n; }

        HOST_DEVICE int getObject() const { return object; }

        HOST_DEVICE int getCount() const { return count; }

        HOST_DEVICE int getNode() const { return node; }

        HOST_DEVICE int getMaterial(const Object * const objects, int i) const { return material >= 0 ? material : objects[object + i].getMaterial(); }

        HOST_DEVICE Vector transformPointToLocal(const Vector & p) const { return transform(inverseTransformMatrix, p, 1); }

        HOST_DEVICE Vector transformPointToWorld(const Vector & p) const { return transform(transformMatrix, p, 1); }

        HOST_DEVICE Vector transformVectorToLocal(const Vector & n) const { return transform(inverseTransformMatrix, n, 0); }
        
        HOST_DEVICE Vector transformVectorToWorld(const Vector & n) const { return transform(transformMatrix, n, 0); }

        HOST_DEVICE Vector transformNormalToWorld(const Vector & n) const { return normalize(transform(transpose(inverseTransformMatrix), n, 0)); }

        HOST_DEVICE Ray transformRayToLocal(const Ray & r) const {
            return Ray(transformPointToLocal(r.getOrigin()), transformVectorToLocal(r.getDirection()), r.getLambdas());
        }

        HOST_DEVICE Vector min(const Object * const objects) const {
            Vector min(MAX, MAX, MAX);

            for (int i = 0; i < count; i++) {
                Vector corner1 = objects[object + i].min();
                Vector corner2 = objects[object + i].max();

                min = minV(min, transformPointToWorld(corner1));
                min = minV(min, transformPointToWorld(corner2));
                min = minV(min, transformPointToWorld(Vector(corner1[0], corner1[1], corner2[2])));
                min = minV(min, transformPointToWorld(Vector(corner1[0], corner2[1], corner1[2])));
                min = minV(min, transformPointToWorld(Vector(corner1[0], corner2[1], corner2[2])));
                min = minV(min, transformPointToWorld(Vector(corner2[0], corner1[1], corner1[2])));
                min = minV(min, transformPointToWorld(Vector(corner2[0], corner1[1], corner2[2])));
                min = minV(min, transformPointToWorld(Vector(corner2[0], corner2[1], corner1[2])));
            }

            return min;
        }

        HOST_DEVICE Vector max(const Object * const objects) const {
            Vector max(-MAX, -MAX, -MAX);

            for (int i = 0; i < count; i++) {
                Vector corner1 = objects[object + i].min();
                Vector corner2 = objects[object + i].max();

                max = maxV(max, transformPointToWorld(corner1));
                max = maxV(max, transformPointToWorld(corner2));
                max = maxV(max, transformPointToWorld(Vector(corner1[0], corner1[1], corner2[2])));
                max = maxV(max, transformPointToWorld(Vector(corner1[0], corner2[1], corner1[2])));
                max = maxV(max, transformPointToWorld(Vector(corner1[0], corner2[1], corner2[2])));
                max = maxV(max, transformPointToWorld(Vector(corner2[0], corner1[1], corner1[2])));
                max = maxV(max, transformPointToWorld(Vector(corner2[0], corner1[1], corner2[2])));
                max = maxV(max, transformPointToWorld(Vector(corner2[0], corner2[1], corner1[2])));
            }

            return max;
        }

        HOST_DEVICE Vector center(const Object * const objects) const {
            Vector center(0, 0, 0);

            for (int i = 0; i < count; i++)
                center += transformPointToWorld(objects[object + i].center());

            return center / Float(count);
        }

        HOST_DEVICE Vector center(const Object * const objects, int i) const { return transformPointToWorld(objects[object + i].center()); }

        HOST_DEVICE Float radius(const Object * const objects, int i) const { return objects[object + i].radius() * Vector(transformMatrix.get(0, 0), transformMatrix.get(1, 0), transformMatrix.get(2, 0)).length(); }

        HOST_DEVICE Float area(const Object * const objects, int i) const { return objects[object + i].area() * Vector(transformMatrix.get(0, 0), transformMatrix.get(1, 0), transformMatrix.get(2, 0)).lengthSquared(); }

        HOST_DEVICE Float pdf(const Object * const objects, int i, const Vector & point, const Vector & direction) const { return objects[object + i].pdf(transformPointToLocal(point), normalize(transformVectorToLocal(direction))); }

        HOST_DEVICE Vector sample(const Object * const objects, int i, const Vector & point, Random & state) const { return normalize(transformVectorToWorld(objects[object + i].sample(transformPointToLocal(point), state))); }

    private:
        int object, count, node, material;
        Matrix4<Float> transformMatrix, inverseTransformMatrix;
};