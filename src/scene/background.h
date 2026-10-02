#pragma once

#include "core/constants.h"
#include "core/platform.h"
#include "math/intersection.h"
#include "math/ray.h"
#include "math/spectrum.h"
#include "math/vector.h"
#include "scene/texture.h"

namespace lambda {
    class Background {
        public:
            Background() : type(Type::EQUIRECTANGULAR), radius(1), emission(nullptr), conditionalCDF(nullptr), marginalCDF(nullptr) {}

            static Background makeEquirectangular(SpectrumTexture * emission, const Vector<Float, 3> & rotation) {
                Background background;

                background.type = Type::EQUIRECTANGULAR;
                background.radius = 1;
                background.emission = emission;
                background.rotation = utils::rotationMatrix(rotation);
                background.inverseRotation = transpose(background.rotation);

                return background;
            }

            static Background makeOctahedral(SpectrumTexture * emission, const Vector<Float, 3> & rotation) {
                Background background;

                background.type = Type::OCTAHEDRAL;
                background.radius = 1;
                background.emission = emission;
                background.rotation =  utils::rotationMatrix(rotation);
                background.inverseRotation = transpose(background.rotation);

                return background;
            }

            void setRadius(Float r) { radius = r; }

            void setConditionalCDF(Float * c) { conditionalCDF = c; }

            void setMarginalCDF(Float * m) { marginalCDF = m; }

            LAMBDA_HOST_DEVICE const SpectrumTexture * getEmission() const { return emission; }

            LAMBDA_HOST_DEVICE Float area() const { return 4 * constants::PI * radius * radius; }

            LAMBDA_HOST_DEVICE Float getPower() const { return area() * emission->getAverage(); }

            LAMBDA_HOST_DEVICE Float jacobian(Float y) const {
                switch (type) {
                    case Type::EQUIRECTANGULAR: return 2 * constants::PI * constants::PI * std::sin(constants::PI * y);
                    case Type::OCTAHEDRAL: return 4 * constants::PI;
                }

                return 0;
            }

            LAMBDA_HOST_DEVICE void getTextureCoordinate(const Vector<Float, 3> & direction, Vector<Float, 2> & textureCoordinate) const {
                switch (type) {
                    case Type::EQUIRECTANGULAR: textureCoordinateEquirectangular(direction, textureCoordinate); break;
                    case Type::OCTAHEDRAL: textureCoordinateOctahedral(direction, textureCoordinate); break;
                }
            }

            LAMBDA_HOST_DEVICE Float pdf(const Vector<Float, 3> & direction) const {
                Vector<Float, 3> transformedDirection(rotation * Vector<Float, 4>(direction, 0));

                Vector<Float, 2> textureCoordinate;

                getTextureCoordinate(transformedDirection, textureCoordinate);

                int width = emission->getWidth();
                int height = emission->getHeight();

                int xIndex = int(utils::clamp(textureCoordinate[0] * Float(width), 0, Float(width - 1)));
                int yIndex = int(utils::clamp(textureCoordinate[1] * Float(height), 0, Float(height - 1)));

                Float marginal = marginalCDF[yIndex];
                Float conditional = conditionalCDF[yIndex * width + xIndex];

                if (yIndex > 0) marginal -= marginalCDF[yIndex - 1];
                if (xIndex > 0) conditional -= conditionalCDF[yIndex * width + xIndex - 1];

                return marginal * conditional * Float(width * height) / std::fmax(jacobian(textureCoordinate[1]), constants::EPSILON);
            }

            LAMBDA_HOST_DEVICE Vector<Float, 3> sample(Random & state) const {
                switch (type) {
                    case Type::EQUIRECTANGULAR: return sampleEquirectangular(state);
                    case Type::OCTAHEDRAL: return sampleOctahedral(state);
                }

                return Vector<Float, 3>();
            }

            LAMBDA_HOST_DEVICE SampledSpectrum evaluate(const Ray & r) const {
                Intersection intersection;

                Vector<Float, 3> direction(rotation * Vector<Float, 4>(r.getDirection(), 0));

                intersection.point = direction;
                intersection.normal = direction;
                intersection.tangent = normalize(Vector<Float, 3>(-direction[2], 0, direction[0]));

                getTextureCoordinate(direction, intersection.textureCoordinate);

                SampledSpectrum attenuation;

                for (int i = 0; i < constants::HERO_COUNT; i++) attenuation[i] = emission->evaluate(intersection, r.getLambdas()[i]);

                return attenuation;
            }

        private:
            enum class Type { EQUIRECTANGULAR, OCTAHEDRAL };

            Type type;
            Float radius;
            SpectrumTexture * emission;
            Float * conditionalCDF, * marginalCDF;
            Matrix<Float, 4> rotation;
            Matrix<Float, 4> inverseRotation;

            LAMBDA_HOST_DEVICE void textureCoordinateEquirectangular(const Vector<Float, 3> & direction, Vector<Float, 2> & textureCoordinate) const {
                textureCoordinate[0] = (std::atan2(direction[2], direction[0]) + constants::PI) / (2 * constants::PI);
                textureCoordinate[1] = std::acos(utils::clamp(direction[1], Float(-1), Float(1))) / constants::PI;
            }

            LAMBDA_HOST_DEVICE void textureCoordinateOctahedral(const Vector<Float, 3> & direction, Vector<Float, 2> & textureCoordinate) const {
                Float r = std::sqrt(std::fmax(1 - std::fabs(direction[1]), Float(0)));
                Float denominator = r * std::sqrt(std::fmax(2 - r * r, Float(0)));

                Float cosPhi = r > constants::EPSILON_SQUARED ? direction[0] / denominator : 1;
                Float sinPhi = r > constants::EPSILON_SQUARED ? direction[2] / denominator : 0;

                Float phi = std::atan2(std::fabs(sinPhi), std::fabs(cosPhi));

                Float delta = r * (4 * phi / constants::PI - 1);
                Float sum = (direction[1] > 0) ? r : 2 - r;

                textureCoordinate[0] = std::copysign((sum - delta) / 2, cosPhi);
                textureCoordinate[1] = std::copysign((sum + delta) / 2, sinPhi);

                textureCoordinate[0] = Float(0.5) * textureCoordinate[0] + Float(0.5);
                textureCoordinate[1] = Float(0.5) * textureCoordinate[1] + Float(0.5);
            }

            LAMBDA_HOST_DEVICE int findIndex(const Float * cdf, int size, Float value) const {
                int left = 0;
                int right = size - 1;

                while (left < right) {
                    int mid = left + (right - left) / 2;

                    if (cdf[mid] < value)
                        left = mid + 1;
                    else
                        right = mid;
                }

                return left;
            }

            LAMBDA_HOST_DEVICE Vector<Float, 3> sampleEquirectangular(Random & state) const {
                int width = emission->getWidth();
                int height = emission->getHeight();

                int y = findIndex(marginalCDF, height, utils::randomFloat(state));
                int x = findIndex(conditionalCDF + y * width, width, utils::randomFloat(state));

                Float u = (Float(x) + utils::randomFloat(state)) / Float(width);
                Float v = (Float(y) + utils::randomFloat(state)) / Float(height);

                Float phi = u * 2 * constants::PI - constants::PI;
                Float theta = v * constants::PI;

                Float sinTheta = std::sin(theta);

                Vector<Float, 3> direction(sinTheta * std::cos(phi), std::cos(theta), sinTheta * std::sin(phi));

                return Vector<Float, 3>(inverseRotation * Vector<Float, 4>(direction, 0));
            }

            LAMBDA_HOST_DEVICE Vector<Float, 3> sampleOctahedral(Random & state) const {
                int width = emission->getWidth();
                int height = emission->getHeight();

                int y = findIndex(marginalCDF, height, utils::randomFloat(state));
                int x = findIndex(conditionalCDF + y * width, width, utils::randomFloat(state));

                Float u = (Float(x) + utils::randomFloat(state)) / Float(width);
                Float v = (Float(y) + utils::randomFloat(state)) / Float(height);

                u = u * 2 - 1;
                v = v * 2 - 1;

                Float sum = std::fabs(u) + std::fabs(v);

                Float r = (sum < 1) ? sum : 2 - sum;

                Vector<Float, 3> direction(0, 1 - r * r, 0);

                if (sum > 1) direction[1] *= -1;

                if (r > constants::EPSILON) {
                    Float delta = std::fabs(v) - std::fabs(u);

                    Float phi = (constants::PI / 4) * (delta / r + 1);

                    Float denominator = r * std::sqrt(std::fmax(2 - r * r, Float(0)));

                    direction[0] = std::copysign(denominator * std::cos(phi), u);
                    direction[2] = std::copysign(denominator * std::sin(phi), v);
                }

                return Vector<Float, 3>(inverseRotation * Vector<Float, 4>(direction, 0));
            }
    };
}