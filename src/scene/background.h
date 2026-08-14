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

        HOST_DEVICE Float area(Float radius) const {
            switch (type) {
                case BackgroundType::EQUIRECTANGULAR: return areaEquirectangular(radius);
            }

            return 0;
        }

        HOST_DEVICE Float average(DenseSpectrum<Float> * const spectra, SpectrumTexture * const spectrumTextures) const {
            switch (type) {
                case BackgroundType::EQUIRECTANGULAR: return averageEquirectangular(spectra, spectrumTextures);
            }

            return 0;
        }

        HOST_DEVICE Float pdf() const {
            switch (type) {
                case BackgroundType::EQUIRECTANGULAR: return pdfEquirectangular();
            }

            return 0;
        }

        HOST_DEVICE Vector sample(Random & state) const {
            switch (type) {
                case BackgroundType::EQUIRECTANGULAR: return sampleEquirectangular(state);
            }

            return Vector();
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

        HOST_DEVICE Float areaEquirectangular(Float radius) const { return 4 * PI * radius * radius; }

        HOST_DEVICE Float averageEquirectangular(DenseSpectrum<Float> * const spectra, SpectrumTexture * const spectrumTextures) const { return spectrumTextures[equirectangular.texture].average(spectra); }

        HOST_DEVICE Float pdfEquirectangular() const { return 1 / (4 * PI); }

        HOST_DEVICE Vector sampleEquirectangular(Random & state) const { return randomUnitVector(state); }

        HOST_DEVICE SampledSpectrum evaluateEquirectangular(DenseSpectrum<Float> * const spectra, SpectrumTexture * const spectrumTextures, const Ray & r) const {
            Intersection intersection;

            intersection.point = r.getDirection();

            intersection.u = (atan2F(r.getDirection()[2], r.getDirection()[0]) + PI) / (2 * PI);
            intersection.v = acosF(r.getDirection()[1]) / PI;

            SampledSpectrum attenuation;

            for (int i = 0; i < HERO_COUNT; i++)
                attenuation[i] = spectrumTextures[equirectangular.texture].evaluate(spectra, intersection, r.getLambdas()[i]);

            return attenuation;
        }
};