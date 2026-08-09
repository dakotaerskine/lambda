#pragma once

#include <atomic>

#ifdef USE_OPENMP
    #include <omp.h>
#endif

#include "core/constants.h"
#include "core/platform.h"
#include "math/intersection.h"
#include "math/random.h"
#include "math/ray.h"
#include "math/spectrum.h"
#include "math/vector.h"
#include "render/camera.h"
#include "scene/background.h"
#include "scene/material.h"
#include "scene/object.h"
#include "scene/scene.h"

class Renderer {
    public:
        HOST_DEVICE Renderer() : width(0), height(0), totalPixels(0), depth(0), samples(0), sqrtSamples(0), wavelengthSamples(0), lambdaMin(0), lambdaMax(0), lambdaStep(0), outputBuffer(nullptr) {}

        HOST_DEVICE Renderer(int _width, int _height, int _depth, int _samples, int _sqrtSamples, int _wavelengthSamples, Float _lambdaMin, Float _lambdaMax) : width(_width), height(_height), totalPixels(width * height), depth(_depth), samples(_samples), sqrtSamples(_sqrtSamples), wavelengthSamples(_wavelengthSamples), lambdaMin(_lambdaMin), lambdaMax(_lambdaMax), lambdaStep((lambdaMax - lambdaMin) / (wavelengthSamples - 1)), outputBuffer(nullptr) {}

        HOST_DEVICE int getWidth() const { return width; }

        HOST_DEVICE int getHeight() const { return height; }

        HOST_DEVICE int getTotalPixels() const { return totalPixels; }

        HOST_DEVICE void setCamera(const Vector & position, const Vector & corner, const Vector & horizontal, const Vector & vertical) { camera = Camera(position, corner, horizontal, vertical); }

        HOST_DEVICE void setScene(DenseSpectrum<Float> * const spectra, DenseSpectrum<Complex> * const complexSpectra, const Background & background, Object * const objects, int numObjects, Material * const materials, int * const materialProperties, ScalarTexture * const scalarTextures, SpectrumTexture * const spectrumTextures) { scene = Scene(spectra, complexSpectra, background, objects, numObjects, materials, materialProperties, scalarTextures, spectrumTextures); }

        HOST_DEVICE void setOutputBuffer(Float * _outputBuffer) { outputBuffer = _outputBuffer; }

        void renderImage(int * completed, uint64_t seed) const {
            #ifdef USE_OPENMP
                #pragma omp parallel for schedule(guided)
            #endif

            for (int py = 0; py < height; py++)
                for (int px = 0; px < width; px++) {
                    Random state(seed, py * width + px);

                    renderPixel(px, py, state);

                    std::atomic_ref<int>(*completed).fetch_add(1, std::memory_order_relaxed);
                }
        }

    private:
        int width, height, totalPixels, depth, samples, sqrtSamples, wavelengthSamples;
        Float lambdaMin, lambdaMax, lambdaStep;
        Camera camera;
        Scene scene;
        Float * outputBuffer;

        HOST_DEVICE void renderPixel(int px, int py, Random & state) const {
            int index = py * width + px;

            SampledSpectrum spectrum(wavelengthSamples);

            for (int k = 0; k < sqrtSamples; k++)
                for (int l = 0; l < sqrtSamples; l++)
                    for (int m = 0; m < wavelengthSamples; m++) {
                        Float u = Float(px + (k + randomDouble(state)) / sqrtSamples) / width;
                        Float v = Float(py + (l + randomDouble(state)) / sqrtSamples) / height;
                        Float lambda = lambdaMin + m * lambdaStep;
                        Ray ray = camera.getRay(u, v, lambda);
                        spectrum[m] += trace(ray, state);
                    }

            spectrum /= sqrtSamples * sqrtSamples;

            Vector color = spectrumToRGB(spectrum, lambdaMin, lambdaMax);
            color = Vector(sRGB(fmin(fmax(color[0], 0.0), 1.0)), sRGB(fmin(fmax(color[1], 0.0), 1.0)), sRGB(fmin(fmax(color[2], 0.0), 1.0)));

            outputBuffer[index * 3 + 0] = color[0];
            outputBuffer[index * 3 + 1] = color[1];
            outputBuffer[index * 3 + 2] = color[2];
        }

        HOST_DEVICE Float trace (Ray r, Random & state) const {
            Float throughput = 1;

            for (int i = 0; i <= depth; i++) {
                Intersection intersection;

                if (!scene.hit(r, intersection)) return throughput * scene.getBackground().evaluate(scene.getSpectra(), scene.getSpectrumTextures(), r.getDirection(), r.getLambda());

                Ray scattered;
                Float attenuation = 1;

                if (!scene.getMaterials()[scene.getObjects()[intersection.i].getMaterial()].scatter(scene.getSpectra(), scene.getComplexSpectra(), scene.getMaterialProperties(), scene.getScalarTextures(), scene.getSpectrumTextures(), r, intersection, scattered, attenuation, state)) return throughput * attenuation;

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

        friend GLOBAL void renderKernel(int * d_completed, Renderer renderer, uint64_t seed);
};

#ifdef __CUDACC__
    GLOBAL void renderKernel(int * d_completed, Renderer renderer, uint64_t seed) {
        int px = blockIdx.x * blockDim.x + threadIdx.x;
        int py = blockIdx.y * blockDim.y + threadIdx.y;

        if (px >= renderer.width || py >= renderer.height) return;

        Random state(seed, py * renderer.width + px);

        renderer.renderPixel(px, py, state);

        atomicAdd(d_completed, 1);
    }
#endif