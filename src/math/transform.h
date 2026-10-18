#pragma once

#include "core/platform.h"
#include "core/utils.h"
#include "math/matrix.h"
#include "math/ray.h"
#include "math/vector.h"

namespace lambda {
    class Transform {
        public:
            Transform() {}

            Transform(const Vector<Float, 3> & translation, const Vector<Float, 3> & rotation, const Vector<Float, 3> & scale) {
                Matrix<Float, 4> rotationMatrix = utils::rotationMatrix(rotation);

                transformMatrix = utils::translationMatrix(translation) * rotationMatrix * utils::scaleMatrix(scale);
                inverseTransformMatrix = utils::scaleMatrix(Vector<Float, 3>(1 / scale[0], 1 / scale[1], 1 / scale[2])) * transpose(rotationMatrix) * utils::translationMatrix(Vector<Float, 3>(-translation));
                normalMatrix = Matrix<Float, 3>(transpose(inverseTransformMatrix));
            }

            LAMBDA_HOST_DEVICE Float scale(int i) const { return Vector<Float, 3>(transformMatrix.get(0, i), transformMatrix.get(1, i), transformMatrix.get(2, i)).length(); }

            LAMBDA_HOST_DEVICE Float scaleSquared(int i) const { return Vector<Float, 3>(transformMatrix.get(0, i), transformMatrix.get(1, i), transformMatrix.get(2, i)).lengthSquared();}

            LAMBDA_HOST_DEVICE Vector<Float, 3> pointToLocal(const Vector<Float, 3> & p) const { return Vector<Float, 3>(inverseTransformMatrix * Vector<Float, 4>(p, 1)); }

            LAMBDA_HOST_DEVICE Vector<Float, 3> pointToWorld(const Vector<Float, 3> & p) const { return Vector<Float, 3>(transformMatrix * Vector<Float, 4>(p, 1)); }

            LAMBDA_HOST_DEVICE Vector<Float, 3> vectorToLocal(const Vector<Float, 3> & v) const { return Vector<Float, 3>(inverseTransformMatrix * Vector<Float, 4>(v, 0)); }

            LAMBDA_HOST_DEVICE Vector<Float, 3> vectorToWorld(const Vector<Float, 3> & v) const { return Vector<Float, 3>(transformMatrix * Vector<Float, 4>(v, 0)); }

            LAMBDA_HOST_DEVICE Vector<Float, 3> normalToWorld(const Vector<Float, 3> & n) const { return normalize(normalMatrix * n); }

            LAMBDA_HOST_DEVICE Ray rayToLocal(const Ray & r) const { return Ray(pointToLocal(r.getOrigin()), vectorToLocal(r.getDirection()), r.getLambdas(), r.getMedium()); }

        private:
            Matrix<Float, 4> transformMatrix, inverseTransformMatrix;
            Matrix<Float, 3> normalMatrix;
    };
}