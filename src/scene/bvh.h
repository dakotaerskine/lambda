#pragma once

#include <algorithm>
#include <map>
#include <numeric>
#include <vector>

#include "core/constants.h"
#include "core/platform.h"
#include "math/vector.h"

namespace lambda {
    template <typename T>
    class BVHNode {
        public:
            BVHNode() : min(Vector<Float, 3>(constants::MAX, constants::MAX, constants::MAX)), max(Vector<Float, 3>(-constants::MAX, -constants::MAX, -constants::MAX)), right(nullptr), elements(nullptr), count(0), power(0) {}

            BVHNode(const Vector<Float, 3> & _min, const Vector<Float, 3> & _max) : min(_min), max(_max), right(nullptr), elements(nullptr), count(0), power(0) {}

            BVHNode(const Vector<Float, 3> & _min, const Vector<Float, 3> & _max, T ** _elements, int _count) : min(_min), max(_max), right(nullptr), elements(_elements), count(_count), power(0) {}

            void setRight(BVHNode<T> * _right) { right = _right; }

            void setPower(Float p) { power = p; }

            LAMBDA_HOST_DEVICE const Vector<Float, 3> & getMin() const { return min; }
            LAMBDA_HOST_DEVICE const Vector<Float, 3> & getMax() const { return max; }

            LAMBDA_HOST_DEVICE bool isLeaf() const { return elements != nullptr; }

            LAMBDA_HOST_DEVICE int getCount() const { return count; }

            LAMBDA_HOST_DEVICE const BVHNode<T> * getRight() const { return right; }

            LAMBDA_HOST_DEVICE const T * getElement(int i) const { return isLeaf() ? elements[i] : nullptr; }

            LAMBDA_HOST_DEVICE Float getPower() const { return power; }

            LAMBDA_HOST_DEVICE bool intersect(const Ray & r, const Intersection & intersection) const {
                Float t1 = 0, t2 = constants::MAX;

                for (int i = 0; i < 3; i++) {
                    Float inverseDirection = 1 / r.getDirection()[i];

                    Float tMin = (min[i] - r.getOrigin()[i]) * inverseDirection;
                    Float tMax = (max[i] - r.getOrigin()[i]) * inverseDirection;

                    if (inverseDirection < 0) {
                        Float temp = tMin;
                        tMin = tMax;
                        tMax = temp;
                    }

                    t1 = std::fmax(tMin, t1);
                    t2 = std::fmin(tMax, t2);
                }

                if (t1 <= t2 && t2 >= 0 && t1 < intersection.t) return true;

                return false;
            }

        private:
            Vector<Float, 3> min, max;
            BVHNode<T> * right;
            T ** elements;
            int count;
            Float power;
    };
}