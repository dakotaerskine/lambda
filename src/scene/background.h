#pragma once

#include "core/constants.h"
#include "core/platform.h"
#include "math/intersection.h"
#include "math/ray.h"
#include "math/spectrum.h"
#include "math/vector.h"
#include "scene/texture.h"

enum class BackgroundType { EQUIRECTANGULAR };

class Background {
    public:
        HOST_DEVICE Background() : type(BackgroundType::EQUIRECTANGULAR) { equirectangular.texture = -1; }

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

        HOST_DEVICE SampledSpectrum evaluate(DenseSpectrum<Float> * const spectra, SpectrumTexture * const spectrumTextures, const Ray & r) const {
            switch (type) {
                case BackgroundType::EQUIRECTANGULAR: return evaluateEquirectangular(spectra, spectrumTextures, r);
            }

            return 0;
        }

    private:
        BackgroundType type;

        union {
            struct { int texture; } equirectangular;
        };

        HOST_DEVICE SampledSpectrum evaluateEquirectangular(DenseSpectrum<Float> * const spectra, SpectrumTexture * const spectrumTextures, const Ray & r) const {
            Intersection intersection;

            intersection.point = r.getDirection();


            intersection.u = (atan2F(r.getDirection()[2], r.getDirection()[0]) + PI) / (2 * PI);
            intersection.v = acosF(r.getDirection()[1]) / PI;

            SampledSpectrum attenuation;

            for (int i = 0; i < HERO_COUNT; i++)
                attenuation[i] = spectrumTextures[equirectangular.texture].evaluate(spectra, intersection, r.getLambda(i));

            return attenuation;
        }
};