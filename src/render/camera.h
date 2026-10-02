#pragma once

#include "core/platform.h"
#include "core/utils.h"
#include "math/matrix.h"
#include "math/ray.h"
#include "math/spectrum.h"
#include "math/vector.h"

namespace lambda {
    class Medium;

    class Camera {
        public:
            Camera() : type(Type::PERSPECTIVE), medium(nullptr), position(0, 0, 0), perspective{Vector<Float, 3>(-1, 1, 1), Vector<Float, 3>(2, 0, 0), Vector<Float, 3>(0, -2, 0)} {}

            static Camera makePerspective(Medium * medium, const Vector<Float, 3> & position, const Vector<Float, 3> & lookAt, const Vector<Float, 3> & up, Float fov, Float aspect) {
                Camera camera;

                camera.type = Type::PERSPECTIVE;
                camera.position = position;
                camera.medium = medium;

                Vector<Float, 3> forward = normalize(lookAt - position);
                Vector<Float, 3> right = normalize(cross(forward, up));
                Vector<Float, 3> trueUp = cross(right, forward);

                Float halfHeight = std::tan(fov * Float(0.5));
                Float halfWidth = halfHeight * aspect;

                camera.perspective.corner = forward - right * halfWidth + trueUp * halfHeight;
                camera.perspective.horizontal = right * halfWidth * 2;
                camera.perspective.vertical = trueUp * halfHeight * -2;

                return camera;
            }

            static Camera makeOrthographic(Medium * medium, const Vector<Float, 3> & position, const Vector<Float, 3> & lookAt, const Vector<Float, 3> & up, Float height, Float aspect) {
                Camera camera;

                camera.type = Type::ORTHOGRAPHIC;
                camera.position = position;
                camera.medium = medium;

                camera.orthographic.forward = normalize(lookAt - position);
                Vector<Float, 3> right = normalize(cross(camera.orthographic.forward, up));
                Vector<Float, 3> trueUp = cross(right, camera.orthographic.forward);

                Float halfHeight = height * Float(0.5);
                Float halfWidth = halfHeight * aspect;

                camera.orthographic.corner = -right * halfWidth + trueUp * halfHeight;
                camera.orthographic.horizontal = right * halfWidth * 2;
                camera.orthographic.vertical = trueUp * halfHeight * -2;

                return camera;
            }

            static Camera makeSpherical(Medium * medium, const Vector<Float, 3> & position, const Vector<Float, 3> & lookAt, const Vector<Float, 3> & up) {
                Camera camera;

                camera.type = Type::SPHERICAL;
                camera.position = position;
                camera.medium = medium;

                camera.spherical.forward = normalize(lookAt - position);
                camera.spherical.right = normalize(cross(camera.spherical.forward, up));
                camera.spherical.up = cross(camera.spherical.right, camera.spherical.forward);

                return camera;
            }

            LAMBDA_HOST_DEVICE Ray getRay(Float u, Float v, const SampledSpectrum & lambdas) const {
                switch (type) {
                    case Type::PERSPECTIVE: return getRayPerspective(u, v, lambdas);
                    case Type::ORTHOGRAPHIC: return getRayOrthographic(u, v, lambdas);
                    case Type::SPHERICAL: return getRaySpherical(u, v, lambdas);
                }

                return Ray();
            }

            LAMBDA_HOST_DEVICE bool isLinear() const {
                switch (type) {
                    case Type::PERSPECTIVE: return true;
                    case Type::ORTHOGRAPHIC: return true;
                    case Type::SPHERICAL: return false;
                }

                return false;
            }

            LAMBDA_HOST_DEVICE Matrix<Float, 4> getWorldToCamera() const {
                switch (type) {
                    case Type::PERSPECTIVE: return getWorldToCameraLinear(perspective.horizontal, perspective.vertical);
                    case Type::ORTHOGRAPHIC: return getWorldToCameraLinear(orthographic.horizontal, orthographic.vertical);
                    case Type::SPHERICAL: return getWorldToCameraSpherical();
                }

                return Matrix<Float, 4>();
            }

            LAMBDA_HOST_DEVICE Matrix<Float, 4> getWorldToNDC() const {
                switch (type) {
                    case Type::PERSPECTIVE: return getWorldToNDCPerspective();
                    case Type::ORTHOGRAPHIC: return getWorldToNDCOrthographic();
                    case Type::SPHERICAL: return Matrix<Float, 4>();
                }

                return Matrix<Float, 4>();
            }

        private:
            enum class Type { PERSPECTIVE, ORTHOGRAPHIC, SPHERICAL };

            Type type;
            Medium * medium;
            Vector<Float, 3> position;

            union {
                struct { Vector<Float, 3> corner, horizontal, vertical; } perspective;
                struct { Vector<Float, 3> corner, horizontal, vertical, forward; } orthographic;
                struct { Vector<Float, 3> forward, right, up; } spherical;
            };

            LAMBDA_HOST_DEVICE Ray getRayPerspective(Float u, Float v, const SampledSpectrum & lambdas) const {
                Vector<Float, 3> direction = normalize(perspective.corner + perspective.horizontal * u + perspective.vertical * v);

                return Ray(position, direction, lambdas, medium);
            }

            LAMBDA_HOST_DEVICE Ray getRayOrthographic(Float u, Float v, const SampledSpectrum & lambdas) const {
                Vector<Float, 3> origin = position + orthographic.corner + orthographic.horizontal * u + orthographic.vertical * v;

                return Ray(origin, orthographic.forward, lambdas, medium);
            }

            LAMBDA_HOST_DEVICE Ray getRaySpherical(Float u, Float v, const SampledSpectrum & lambdas) const {
                Float phi = u * 2 * constants::PI;
                Float theta = v * constants::PI;

                Vector<Float, 3> direction = normalize(-spherical.forward * std::sin(theta) * std::cos(phi) - spherical.right * std::sin(theta) * std::sin(phi) + spherical.up * std::cos(theta));

                return Ray(position, direction, lambdas, medium);
            }

            LAMBDA_HOST_DEVICE Matrix<Float, 4> getWorldToCameraLinear(const Vector<Float, 3> & horizontal, const Vector<Float, 3> & vertical) const {
                Vector<Float, 3> r = normalize(horizontal);
                Vector<Float, 3> u = normalize(-vertical);
                Vector<Float, 3> b = normalize(cross(horizontal, vertical));

                return Matrix<Float, 4>(r[0], r[1], r[2], -dot(r, position), u[0], u[1], u[2], -dot(u, position), b[0], b[1], b[2], -dot(b, position), 0, 0, 0, 1);
            }

            LAMBDA_HOST_DEVICE Matrix<Float, 4> getWorldToCameraSpherical() const { return Matrix<Float, 4>(spherical.right[0], spherical.right[1], spherical.right[2], -dot(spherical.right, position), spherical.up[0], spherical.up[1], spherical.up[2], -dot(spherical.up, position), spherical.forward[0], spherical.forward[1], spherical.forward[2], -dot(spherical.forward, position), 0, 0, 0, 1); }

            LAMBDA_HOST_DEVICE Matrix<Float, 4> getWorldToNDCPerspective() const {
                Matrix<Float, 3> m = inverse(Matrix<Float, 3>(perspective.horizontal[0], perspective.vertical[0], perspective.corner[0], perspective.horizontal[1], perspective.vertical[1], perspective.corner[1], perspective.horizontal[2], perspective.vertical[2], perspective.corner[2]));

                Vector<Float, 3> t = -(m * position);

                return Matrix<Float, 4>(m.get(0, 0), m.get(0, 1), m.get(0, 2), t[0], m.get(1, 0), m.get(1, 1), m.get(1, 2), t[1], m.get(2, 0), m.get(2, 1), m.get(2, 2), t[2], 0, 0, 0, 1);
            }

            LAMBDA_HOST_DEVICE Matrix<Float, 4> getWorldToNDCOrthographic() const {
                Matrix<Float, 3> m = inverse(Matrix<Float, 3>(orthographic.horizontal[0], orthographic.vertical[0], orthographic.forward[0], orthographic.horizontal[1], orthographic.vertical[1], orthographic.forward[1], orthographic.horizontal[2], orthographic.vertical[2], orthographic.forward[2]));

                Vector<Float, 3> t = -(m * (position + orthographic.corner));

                return Matrix<Float, 4>(m.get(0, 0), m.get(0, 1), m.get(0, 2), t[0], m.get(1, 0), m.get(1, 1), m.get(1, 2), t[1], m.get(2, 0), m.get(2, 1), m.get(2, 2), t[2], 0, 0, 0, 1);
            }
    };
}