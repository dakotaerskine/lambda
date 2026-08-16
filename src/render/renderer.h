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
#include "scene/bvh.h"
#include "scene/instance.h"
#include "scene/material.h"
#include "scene/object.h"
#include "scene/scene.h"

class Renderer {
    public:
        HOST_DEVICE Renderer() : width(0), height(0), totalPixels(0), depth(0), samples(0), sqrtSamples(0), lambdaMin(0), lambdaMax(0), lambdaRange(0), buffer(nullptr) {}

        HOST_DEVICE Renderer(int _width, int _height, int _depth, int _samples, int _sqrtSamples, Float _lambdaMin, Float _lambdaMax) : width(_width), height(_height), totalPixels(width * height), depth(_depth), samples(_samples), sqrtSamples(_sqrtSamples), lambdaMin(_lambdaMin), lambdaMax(_lambdaMax), lambdaRange(_lambdaMax - _lambdaMin), buffer(nullptr) {}

        HOST_DEVICE int getWidth() const { return width; }

        HOST_DEVICE int getHeight() const { return height; }

        HOST_DEVICE int getTotalPixels() const { return totalPixels; }

        void setCamera(const Vector & position, const Vector & corner, const Vector & horizontal, const Vector & vertical) { camera = Camera(position, corner, horizontal, vertical); }

        void setScene(DenseSpectrum<Float> * const spectra, DenseSpectrum<Complex> * const complexSpectra, const Background & background, Object * const objects, Instance * const instances, BVHNode * const nodes, int * const lightInstances, int * const lightObjects, int numLights, Float * const lightPowers, Float totalLightPower, Material * const materials, int * const materialProperties, ScalarTexture * const scalarTextures, SpectrumTexture * const spectrumTextures) { scene = Scene(spectra, complexSpectra, background, objects, instances, nodes, lightInstances, lightObjects, numLights, lightPowers, totalLightPower, materials, materialProperties, scalarTextures, spectrumTextures); }

        void setBuffer(Float * _buffer) { buffer = _buffer; }

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
        int width, height, totalPixels, depth, samples, sqrtSamples;
        Float lambdaMin, lambdaMax, lambdaRange;
        Camera camera;
        Scene scene;
        Float * buffer;

        HOST_DEVICE void renderPixel(int px, int py, Random & state) const {
            int index = py * width + px;

            Vector color;

            for (int i = 0; i < sqrtSamples; i++)
                for (int j = 0; j < sqrtSamples; j++) {
                    Float u = (Float(px) + (Float(i) + randomFloat(state)) / Float(sqrtSamples)) / Float(width);
                    Float v = (Float(py) + (Float(j) + randomFloat(state)) / Float(sqrtSamples)) / Float(height);

                    Float lambda = lambdaMin + randomFloat(state) * (lambdaMax - lambdaMin);
                    SampledSpectrum lambdas;

                    for (int k = 0; k < HERO_COUNT; k++)
                        lambdas[k] = lambdaMin + fmodF(lambda - lambdaMin + Float(k) * lambdaRange / HERO_COUNT, lambdaRange);

                    Ray ray = camera.getRay(u, v, lambdas);

                    SampledSpectrum spectrum = trace(ray, state);

                    color += spectrumToRGB(spectrum, lambdas, lambdaRange);
                }

            color /= Float(samples);

            buffer[index * 3 + 0] = color[0];
            buffer[index * 3 + 1] = color[1];
            buffer[index * 3 + 2] = color[2];
        }

        HOST_DEVICE SampledSpectrum trace(Ray r, Random & state) const {
            SampledSpectrum radiance = 0;
            SampledSpectrum throughput = 1;
            Float previousScatterProbability = 1;
            bool specular = true;

            const Background & background = scene.getBackground();

            for (int i = 0; i <= depth; i++) {
                Intersection intersection;

                if (!scene.hit(r, intersection)) {
                    Float weight = 1;

                    if (!specular && scene.getTotalLightPower() > 0) weight = powerHeuristic(previousScatterProbability, background.pdf() * scene.getLightPowers()[scene.getNumLights() - 1] / scene.getTotalLightPower());

                    radiance += throughput * background.evaluate(scene.getSpectra(), scene.getScalarTextures(), scene.getSpectrumTextures(), r) * weight;

                    break;
                }

                const Instance & instance = scene.getInstances()[intersection.instance];
                int relativeObjectIndex = intersection.object - instance.getObject();
                const Material & material = scene.getMaterials()[instance.getMaterial(scene.getObjects(), relativeObjectIndex)];

                if (material.isEmissive()) {
                    Float weight = 1;

                    if (!specular && scene.getTotalLightPower() > 0) {
                        Float lightPower = instance.area(scene.getObjects(), relativeObjectIndex) * material.averageEmission(scene.getSpectra(), scene.getScalarTextures(), scene.getSpectrumTextures());

                        weight = powerHeuristic(previousScatterProbability, instance.pdf(scene.getObjects(), relativeObjectIndex, r.getOrigin(), r.getDirection()) * lightPower / scene.getTotalLightPower());
                    }

                    radiance += throughput * material.emission(scene.getSpectra(), scene.getScalarTextures(), scene.getSpectrumTextures(), intersection, r.getLambdas()) * weight;

                    break;
                }

                if (!material.isSpecular() && scene.getTotalLightPower() > 0) {
                    Float target = scene.getTotalLightPower() * randomFloat(state);
                    Float cumulative = 0;
                    int lightInstanceIndex = scene.getLightInstances()[0];
                    int lightObjectIndex = scene.getLightObjects()[0];
                    Float lightPower = scene.getLightPowers()[0];

                    for (int j = 0; j < scene.getNumLights(); j++) {
                        cumulative += scene.getLightPowers()[j];

                        if (cumulative >= target) {
                            lightInstanceIndex = scene.getLightInstances()[j];
                            lightObjectIndex = scene.getLightObjects()[j];
                            lightPower = scene.getLightPowers()[j];

                            break;
                        }
                    }

                    int relativeLightObjectIndex = lightInstanceIndex != -1 ? lightObjectIndex - scene.getInstances()[lightInstanceIndex].getObject() : -1;

                    Vector lightDirection = lightInstanceIndex != -1 ? scene.getInstances()[lightInstanceIndex].sample(scene.getObjects(), relativeLightObjectIndex, intersection.point, state) : background.sample(state);
                    Float lightProbability = lightInstanceIndex != -1 ? scene.getInstances()[lightInstanceIndex].pdf(scene.getObjects(), relativeLightObjectIndex, intersection.point, lightDirection) : background.pdf();

                    Float cosTheta = dot(intersection.normal, lightDirection);

                    if (lightProbability > 0 && cosTheta > 0) {
                        lightProbability *= lightPower / scene.getTotalLightPower();

                        Ray shadowRay(intersection.point, lightDirection, r.getLambdas());

                        Intersection shadowIntersection;

                        if (!scene.hit(shadowRay, shadowIntersection, lightInstanceIndex, lightObjectIndex)) {
                            SampledSpectrum emission = lightInstanceIndex != -1 ? scene.getMaterials()[scene.getInstances()[lightInstanceIndex].getMaterial(scene.getObjects(), relativeLightObjectIndex)].emission(scene.getSpectra(), scene.getScalarTextures(), scene.getSpectrumTextures(), shadowIntersection, shadowRay.getLambdas()) : background.evaluate(scene.getSpectra(), scene.getScalarTextures(), scene.getSpectrumTextures(), shadowRay);
                            SampledSpectrum attenuation = material.evaluate(scene.getSpectra(), scene.getScalarTextures(), scene.getSpectrumTextures(), intersection, shadowRay);
                            Float scatterToLightProbability = material.pdf(intersection, shadowRay);
                            Float weight = powerHeuristic(lightProbability, scatterToLightProbability);

                            radiance += throughput * attenuation * cosTheta * emission * weight / lightProbability;
                        }
                    }
                }

                SampledSpectrum attenuation = material.scatter(scene.getSpectra(), scene.getComplexSpectra(), scene.getMaterialProperties(), scene.getScalarTextures(), scene.getSpectrumTextures(), intersection, r, state);

                previousScatterProbability = material.pdf(intersection, r);
                specular = material.isSpecular();

                if (specular) throughput *= attenuation;
                else throughput *= attenuation * dot(intersection.normal, r.getDirection()) / previousScatterProbability;

                if (i >= RR_START_DEPTH) {
                    Float q = 1 - clamp(throughput.max(), Float(0.05), Float(0.95));

                    if (randomFloat(state) < q) break;

                    throughput /= 1 - q;
                }
            }

            return radiance;
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