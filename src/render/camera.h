#pragma once

#include "core/platform.h"
#include "core/utils.h"
#include "math/matrix.h"
#include "math/ray.h"
#include "math/spectrum.h"
#include "math/vector.h"

class Camera {
    public:
        HOST_DEVICE Camera() : position(0, 0, 0), corner(-1, 1, 1), horizontal(2, 0, 0), vertical(0, -2, 0) {}
        HOST_DEVICE Camera(const Vector<Float> & p, const Vector<Float> & c, const Vector<Float> & h, const Vector<Float> & v) : position(p), corner(c), horizontal(h), vertical(v) {}

        HOST_DEVICE Ray getRay(Float u, Float v, const SampledSpectrum & lambdas) const {
            Vector<Float> direction = normalize(corner + horizontal * u + vertical * v - position);
            return Ray(position, direction, lambdas);
        }

        HOST_DEVICE Matrix<Float, 4> getWorldToCamera() const {
            Vector<Float> r = normalize(horizontal);
            Vector<Float> u = normalize(-vertical);
            Vector<Float> b = normalize(cross(horizontal, vertical));

            return Matrix<Float, 4>(r[0], r[1], r[2], -dot(r, position), u[0], u[1], u[2], -dot(u, position), b[0], b[1], b[2], -dot(b, position), Float(0), Float(0), Float(0), Float(1));
        }

        HOST_DEVICE Matrix<Float, 4> getWorldToNDC() const {
            Vector<Float> e = corner - position;

            Matrix<Float, 3> m = inverse(Matrix<Float, 3>(horizontal[0], vertical[0], e[0], horizontal[1], vertical[1], e[1], horizontal[2], vertical[2], e[2]));

            Vector<Float> t = -(transform(m, position));

            return Matrix<Float, 4>(m.get(0, 0), m.get(0, 1), m.get(0, 2), t[0], m.get(1, 0), m.get(1, 1), m.get(1, 2), t[1], m.get(2, 0), m.get(2, 1), m.get(2, 2), t[2], Float(0), Float(0), Float(0), Float(1));
        }

    private:
        Vector<Float> position, corner, horizontal, vertical;
};