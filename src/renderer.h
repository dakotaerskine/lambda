#pragma once

#include "camera.h"
#include "config.h"
#include "constants.h"
#include "intersection.h"
#include "platform.h"
#include "random.h"
#include "ray.h"
#include "scene.h"
#include "spectrum.h"
#include "vector.h"

class Renderer {
    public:
        HOST_DEVICE static void renderPixel(int px, int py, Float * outputBuffer, const Config & config, const Camera & camera, const Scene & scene, Random & state) {
            int index = py * config.width + px;

            Spectrum spectrum(config.wavelengthSamples);

            for (int k = 0; k < config.sqrtSamples; k++)
                for (int l = 0; l < config.sqrtSamples; l++)
                    for (int m = 0; m < config.wavelengthSamples; m++) {
                        Float u = Float(px + (k + randomDouble(state)) / config.sqrtSamples) / config.width;
                        Float v = Float(py + (l + randomDouble(state)) / config.sqrtSamples) / config.height;
                        Float lambda = config.lambdaMin + m * config.lambdaStep;
                        Ray ray = camera.getRay(u, v, lambda, m);
                        spectrum[m] += trace(ray, scene, config.depth, state);
                    }

            spectrum /= config.sqrtSamples * config.sqrtSamples;

            Vector color = spectrumToRGB(spectrum, config.lambdaMin, config.lambdaMax);
            color = Vector(sRGB(fmin(fmax(color[0], 0.0), 1.0)), sRGB(fmin(fmax(color[1], 0.0), 1.0)), sRGB(fmin(fmax(color[2], 0.0), 1.0)));

            outputBuffer[index * 3 + 0] = color[0];
            outputBuffer[index * 3 + 1] = color[1];
            outputBuffer[index * 3 + 2] = color[2];
        }

    private:
        HOST_DEVICE static Float trace(Ray r, const Scene & s, int d, Random & state) {
            Float throughput = 1;

            for (int i = 0; i <= d; i++) {
                Intersection intersection;

                if (!s.hit(r, intersection)) return throughput * s.getBackground()[r.getLambdaIndex()];

                Ray scattered;
                Float attenuation = 1;

                if (!s.getObject(intersection.i).getMaterial().scatter(r, intersection, scattered, attenuation, state)) return throughput * attenuation;

                throughput *= attenuation;

                if (i >= RR_START_DEPTH) {
                    Float q = 1 - fmin(fmax(throughput, 0.05), 0.95);

                    if (randomDouble(state) < q) return 0;

                    throughput /= 1 - q;
                }

                r = scattered;
            }

            return 0;
        }
};