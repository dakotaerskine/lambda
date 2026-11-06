#pragma once

#include "core/platform.h"
#include "core/utils.h"
#include "math/matrix.h"
#include "math/ray.h"
#include "math/vector.h"

namespace lambda {
    class Transform {
        public:
            LAMBDA_HOST_DEVICE Transform() {}

            LAMBDA_HOST_DEVICE Transform(const Vector<Float, 3> & translation, const Vector<Float, 3> & rotation, const Vector<Float, 3> & scale) {
                transformMatrix = translationMatrix(translation) * rotationMatrix(rotation) * scaleMatrix(scale);
                inverseTransformMatrix = scaleMatrix(Vector<Float, 3>(1 / scale[0], 1 / scale[1], 1 / scale[2])) * transpose(rotationMatrix(rotation)) * translationMatrix(Vector<Float, 3>(-translation));
                normalMatrix = Matrix<Float, 3>(transpose(inverseTransformMatrix));
            }

            LAMBDA_HOST_DEVICE Float scale(int i) const { return Vector<Float, 3>(transformMatrix.get(0, i), transformMatrix.get(1, i), transformMatrix.get(2, i)).length(); }

            LAMBDA_HOST_DEVICE Float scaleSquared(int i) const { return Vector<Float, 3>(transformMatrix.get(0, i), transformMatrix.get(1, i), transformMatrix.get(2, i)).lengthSquared();}

            LAMBDA_HOST_DEVICE Vector<Float, 3> scale() const { return Vector<Float, 3>(scale(0), scale(1), scale(2)); }

            LAMBDA_HOST_DEVICE Vector<Float, 3> scaleSquared() const { return Vector<Float, 3>(scaleSquared(0), scaleSquared(1), scaleSquared(2)); }

            LAMBDA_HOST_DEVICE Vector<Float, 3> pointToLocal(const Vector<Float, 3> & p) const { return Vector<Float, 3>(inverseTransformMatrix * Vector<Float, 4>(p, 1)); }

            LAMBDA_HOST_DEVICE Vector<Float, 3> pointToWorld(const Vector<Float, 3> & p) const { return Vector<Float, 3>(transformMatrix * Vector<Float, 4>(p, 1)); }

            LAMBDA_HOST_DEVICE Vector<Float, 3> vectorToLocal(const Vector<Float, 3> & v) const { return Vector<Float, 3>(inverseTransformMatrix * Vector<Float, 4>(v, 0)); }

            LAMBDA_HOST_DEVICE Vector<Float, 3> vectorToWorld(const Vector<Float, 3> & v) const { return Vector<Float, 3>(transformMatrix * Vector<Float, 4>(v, 0)); }

            LAMBDA_HOST_DEVICE Vector<Float, 3> normalToWorld(const Vector<Float, 3> & n) const { return normalize(normalMatrix * n); }

            LAMBDA_HOST_DEVICE Ray rayToLocal(const Ray & r) const { return Ray(pointToLocal(r.getOrigin()), vectorToLocal(r.getDirection()), r.getLambdas(), r.getMedium()); }

        private:
            Matrix<Float, 4> transformMatrix, inverseTransformMatrix;
            Matrix<Float, 3> normalMatrix;

            LAMBDA_HOST_DEVICE static Matrix<Float, 4> translationMatrix(const Vector<Float, 3> & translation) { return Matrix<Float, 4>(1, 0, 0, translation[0], 0, 1, 0, translation[1], 0, 0, 1, translation[2], 0, 0, 0, 1); }

            LAMBDA_HOST_DEVICE static Matrix<Float, 4> rotationMatrix(const Vector<Float, 3> & rotation) {
                Float sinX = std::sin(rotation[0]), cosX = std::cos(rotation[0]);
                Float sinY = std::sin(rotation[1]), cosY = std::cos(rotation[1]);
                Float sinZ = std::sin(rotation[2]), cosZ = std::cos(rotation[2]);

                Matrix<Float, 4> rotationMatrixX = Matrix<Float, 4>(1, 0, 0, 0, 0, cosX, -sinX, 0, 0, sinX, cosX, 0, 0, 0, 0, 1);
                Matrix<Float, 4> rotationMatrixY = Matrix<Float, 4>(cosY, 0, sinY, 0, 0, 1, 0, 0, -sinY, 0, cosY, 0, 0, 0, 0, 1);
                Matrix<Float, 4> rotationMatrixZ = Matrix<Float, 4>(cosZ, -sinZ, 0, 0, sinZ, cosZ, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);

                return rotationMatrixZ * rotationMatrixY * rotationMatrixX;
            }

            LAMBDA_HOST_DEVICE static Matrix<Float, 4> scaleMatrix(const Vector<Float, 3> & scale) { return Matrix<Float, 4>(scale[0], 0, 0, 0, 0, scale[1], 0, 0, 0, 0, scale[2], 0, 0, 0, 0, 1); }
    };
}