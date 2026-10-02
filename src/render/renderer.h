#pragma once

#include <atomic>
#include <cmath>

#include <omp.h>

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
#include "scene/medium.h"
#include "scene/object.h"
#include "scene/scene.h"

namespace lambda {
    class Renderer {
        public:
            Renderer() : space(ColorSpace::SRGB), width(0), height(0), totalPixels(0), samples(0), sqrtSamples(0), depth(0), lambdaMin(0), lambdaMax(0), lambdaRange(0), seed(0), camera(nullptr), scene(nullptr), buffer(nullptr) {}

            void setRender(ColorSpace _space, int _width, int _height, int _samples, int _depth, Float _lambdaMin, Float _lambdaMax, uint64_t _seed) {
                space = _space;
                width = _width;
                height = _height;
                totalPixels = width * height;
                samples = _samples;
                sqrtSamples = int(std::lround(std::sqrt(_samples)));
                depth = _depth;
                lambdaMin = _lambdaMin;
                lambdaMax = _lambdaMax;
                lambdaRange = _lambdaMax - _lambdaMin;
                seed = _seed;
            }

            void setCamera(Camera * c) { camera = c; }

            void setScene(Scene * s) { scene = s; }

            void setBuffer(Float * _buffer) { buffer = _buffer; }

            LAMBDA_HOST_DEVICE ColorSpace getSpace() const { return space; }

            LAMBDA_HOST_DEVICE int getWidth() const { return width; }

            LAMBDA_HOST_DEVICE int getHeight() const { return height; }

            LAMBDA_HOST_DEVICE int getTotalPixels() const { return totalPixels; }

            LAMBDA_HOST_DEVICE int getSamples() const { return samples; }

            LAMBDA_HOST_DEVICE Float getLambdaMin() const { return lambdaMin; }

            LAMBDA_HOST_DEVICE Float getLambdaMax() const { return lambdaMax; }

            LAMBDA_HOST_DEVICE const Camera * getCamera() const { return camera; }

            LAMBDA_HOST_DEVICE Float getChannel(int i, int j) const { return buffer[i * 3 + j]; }

            void renderImage(std::atomic<int> & completed) {
                #pragma omp parallel for schedule(guided)
                for (int py = 0; py < height; py++)
                    for (int px = 0; px < width; px++) {
                        Random state(seed, py * width + px);

                        renderPixel(px, py, state);

                        completed.fetch_add(1, std::memory_order_relaxed);
                    }
            }

        private:
            ColorSpace space;
            int width, height, totalPixels, samples, sqrtSamples, depth;
            Float lambdaMin, lambdaMax, lambdaRange;
            uint64_t seed;
            Camera * camera;
            Scene * scene;
            Float * buffer;

            LAMBDA_HOST_DEVICE void renderPixel(int px, int py, Random & state) {
                int index = py * width + px;

                Vector<Float, 3> color;

                for (int i = 0; i < sqrtSamples; i++)
                    for (int j = 0; j < sqrtSamples; j++) {
                        Float u = (Float(px) + (Float(i) + utils::randomFloat(state)) / Float(sqrtSamples)) / Float(width);
                        Float v = (Float(py) + (Float(j) + utils::randomFloat(state)) / Float(sqrtSamples)) / Float(height);

                        Float lambda = lambdaMin + utils::randomFloat(state) * (lambdaMax - lambdaMin);

                        SampledSpectrum lambdas;

                        for (int k = 0; k < constants::HERO_COUNT; k++) lambdas[k] = lambdaMin + std::fmod(lambda - lambdaMin + Float(k) * lambdaRange / constants::HERO_COUNT, lambdaRange);

                        Ray ray = camera->getRay(u, v, lambdas);

                        SampledSpectrum spectrum = trace(ray, state);

                        color += utils::spectrumToXYZ(spectrum, lambdas, lambdaRange);
                    }

                color /= Float(sqrtSamples * sqrtSamples);

                buffer[index * 3 + 0] = color[0];
                buffer[index * 3 + 1] = color[1];
                buffer[index * 3 + 2] = color[2];
            }

            LAMBDA_HOST_DEVICE SampledSpectrum trace(Ray r, Random & state) const {
                SampledSpectrum radiance;
                SampledSpectrum throughput(1);

                Float previousScatterProbability = 1;

                bool specular = true;
                bool collapsed = false;
                bool canBeSampled = true;

                Vector<Float, 3> scatterPoint;

                Float totalPower = scene->getTotalPower();

                const Background * background = scene->getBackground();

                for (int i = 0; i <= depth; i++) {
                    Intersection intersection;

                    bool hitSurface = scene->hit(r, intersection);

                    if (r.getMedium()) throughput *= r.getMedium()->intersect(r, intersection, state);

                    if (!hitSurface && intersection.isSurface) {
                        Float weight = 1;

                        if (!specular && canBeSampled && totalPower > 0) weight = utils::powerHeuristic(previousScatterProbability, background->pdf(r.getDirection()) * background->getPower() / totalPower);

                        radiance += throughput * background->evaluate(r) * weight;

                        break;
                    }

                    const Material * material = nullptr;
                    const Medium * medium0 = nullptr;
                    const Medium * medium1 = nullptr;

                    if (intersection.isSurface) {
                        const Instance * instance = intersection.instance;
                        int objectIndex = int(intersection.object - instance->getObject(0));
                        material = instance->getMaterial(objectIndex);
                        medium0 = instance->getMedium0(objectIndex);
                        medium1 = instance->getMedium1(objectIndex);

                        if (material && material->isEmissive()) {
                            Float weight = 1;

                            if (!specular && canBeSampled) weight = utils::powerHeuristic(previousScatterProbability, instance->pdf(objectIndex, scatterPoint, r.getDirection()) * instance->area(objectIndex) * material->averageEmission() / totalPower);

                            radiance += throughput * material->emission(intersection, r.getLambdas()) * weight;

                            break;
                        }
                    }

                    if ((!intersection.isSurface || (material && !material->isSpecular())) && totalPower > 0) {
                        Float target = totalPower * utils::randomFloat(state);

                        const Object * lightObject = nullptr;
                        const Instance * lightInstance = nullptr;
                        int lightObjectIndex;

                        Float lightPower = 0;

                        scene->findLight(target, lightObject, lightInstance, lightPower);

                        Float lightProbability;
                        Vector<Float, 3> lightDirection;

                        if (lightInstance == nullptr) {
                            lightObjectIndex = -1;

                            lightPower = background->getPower();
                            lightDirection = background->sample(state);
                            lightProbability = background->pdf(lightDirection);
                        }
                        else {
                            lightObjectIndex = int(lightObject - lightInstance->getObject(0));

                            lightDirection = lightInstance->sample(lightObjectIndex, intersection.transformedPoint, state);
                            lightProbability = lightInstance->pdf(lightObjectIndex, intersection.transformedPoint, lightDirection);
                        }

                        lightProbability *= lightPower / totalPower;

                        Float cosTheta = intersection.isSurface ? dot(intersection.transformedNormal, lightDirection) : 1;

                        if (lightProbability > 0 && cosTheta > 0) {
                            Vector<Float, 3> offset = intersection.isSurface ? intersection.transformedNormal * constants::EPSILON : Vector<Float, 3>();

                            Ray shadowRay(intersection.transformedPoint + offset, lightDirection, r.getLambdas(), r.getMedium());

                            Intersection shadowIntersection;

                            if (!scene->hit(shadowRay, shadowIntersection, true, lightObject, lightInstance)) {
                                SampledSpectrum emission = lightInstance ? lightInstance->getMaterial(lightObjectIndex)->emission(shadowIntersection, shadowRay.getLambdas()) : background->evaluate(shadowRay);
                                SampledSpectrum attenuation = !intersection.isSurface ? r.getMedium()->evaluate(intersection, r, shadowRay) : material->evaluate(intersection, r, shadowRay);

                                SampledSpectrum scatterProbability = !intersection.isSurface ? r.getMedium()->pdf(intersection, r, shadowRay) : material->pdf(intersection, r, shadowRay);

                                Float scatterToLightProbability = collapsed ? scatterProbability[0] : scatterProbability.average();

                                SampledSpectrum transmittance = SampledSpectrum(1);

                                const Instance * instance;
                                int objectIndex;

                                do {
                                    shadowIntersection = Intersection();

                                    bool hit = scene->hit(shadowRay, shadowIntersection);

                                    if (!hit) shadowIntersection.t = constants::MAX;

                                    if (shadowRay.getMedium()) transmittance *= shadowRay.getMedium()->transmittance(shadowRay, shadowIntersection);

                                    instance = shadowIntersection.instance;

                                    if (!instance) break;

                                    objectIndex = int(shadowIntersection.object - instance->getObject(0));

                                    shadowRay = Ray(shadowIntersection.transformedPoint, lightDirection, r.getLambdas(), shadowIntersection.frontFacing ? instance->getMedium1(objectIndex) : instance->getMedium0(objectIndex));
                                } while (!instance->getMaterial(objectIndex));

                                Float weight = i == depth ? 1 : utils::powerHeuristic(lightProbability, scatterToLightProbability);

                                radiance += throughput * attenuation * transmittance * cosTheta * emission * weight / lightProbability;
                            }
                        }
                    }

                    if (i == depth) break;

                    Ray previousRay = r;

                    SampledSpectrum attenuation;
                    SampledSpectrum scatterProbability;
                    Float cosTheta;

                    if (intersection.isSurface) {
                        if (material) attenuation = material->scatter(intersection, r, state, collapsed);
                        else r = Ray(intersection.transformedPoint, r.getDirection(), r.getLambdas(), r.getMedium());

                        r.setMedium(dot(intersection.transformedNormal, r.getDirection()) > constants::EPSILON ? (intersection.frontFacing ? medium0 : medium1) : (intersection.frontFacing ? medium1 : medium0));

                        if (!material) {
                            i--;
                            continue;
                        }

                        canBeSampled = dot(intersection.transformedNormal, r.getDirection()) > constants::EPSILON;
                        scatterProbability = material->pdf(intersection, previousRay, r);
                        specular = material->isSpecular();
                        cosTheta = std::fabs(dot(intersection.transformedNormal, r.getDirection()));
                    }
                    else {
                        attenuation = r.getMedium()->scatter(intersection, r, state);
                        canBeSampled = true;
                        scatterProbability = r.getMedium()->pdf(intersection, previousRay, r);
                        specular = false;
                        cosTheta = 1;
                    }

                    scatterPoint = intersection.transformedPoint;

                    previousScatterProbability = collapsed ? scatterProbability[0] : scatterProbability.average();

                    if (specular) throughput *= attenuation;
                    else if (previousScatterProbability < constants::EPSILON) break;
                    else throughput *= attenuation * cosTheta / previousScatterProbability;

                    if (r.getDirection().lengthSquared() < constants::EPSILON) break;

                    if (i >= constants::RR_START_DEPTH) {
                        Float q = 1 - utils::clamp(throughput.max(), Float(0.05), Float(0.95));

                        if (utils::randomFloat(state) < q) break;

                        throughput /= 1 - q;
                    }
                }

                return radiance;
            }

            friend LAMBDA_GLOBAL void renderKernel(Renderer * renderer, std::atomic<int> & completed);
    };
}