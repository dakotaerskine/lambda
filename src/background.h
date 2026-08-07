#pragma once

#include "constants.h"
#include "intersection.h"
#include "platform.h"
#include "scene.h"
#include "texture.h"
#include "vector.h"

enum class BackgroundType { EQUIRECTANGULAR };

class Background {
    public:
        HOST_DEVICE Background() : type(BackgroundType::EQUIRECTANGULAR) {}

        HOST_DEVICE Background(const Background & b) {
            type = b.type;

            switch (type) {
                case BackgroundType::EQUIRECTANGULAR:
                    equirectangular.texture = b.equirectangular.texture;
                    break;
            }
        }

        HOST_DEVICE Background & operator=(const Background & b) {
            type = b.type;

            switch (type) {
                case BackgroundType::EQUIRECTANGULAR:
                    equirectangular.texture = b.equirectangular.texture;
                    break;
            }

            return *this;
        }

        HOST_DEVICE static Background makeEquirectangular(int texture) {
            Background background;

            background.type = BackgroundType::EQUIRECTANGULAR;
            background.equirectangular.texture = texture;

            return background;
        }

        HOST_DEVICE Float evaluate(const Scene & s, const Vector & direction, Float lambda) const {
            switch (type) {
                case BackgroundType::EQUIRECTANGULAR: return evaluateEquirectangular(s, direction, lambda);
            }

            return 0;
        }
    
    private:
        BackgroundType type;

        union {
            struct { int texture; } equirectangular;
        };

        HOST_DEVICE Float evaluateEquirectangular(const Scene & s, const Vector & direction, Float lambda) const {
            Intersection intersection;

            intersection.point = direction;

            intersection.u = (atan2(direction[2], direction[0]) + PI) / (2 * PI);
            intersection.v = acos(direction[1]) / PI;

            return s.getSpectrumTextures()[equirectangular.texture].evaluate(s, intersection, lambda);
        }
};