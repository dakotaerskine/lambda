#pragma once

#include <atomic>
#include <cmath>

#include <omp.h>

#include "core/constants.h"
#include "core/platform.h"
#include "math/random.h"
#include "math/ray.h"
#include "math/spectrum.h"
#include "math/vector.h"
#include "render/camera.h"
#include "render/film.h"
#include "scene/background.h"
#include "scene/bvh.h"
#include "scene/instance.h"
#include "scene/intersection.h"
#include "scene/material.h"
#include "scene/medium.h"
#include "scene/object.h"
#include "scene/scene.h"

namespace lambda {
    class Integrator {
        public:
            Integrator() : samples(0), sqrtSamples(0), depth(0), seed(0), scene(nullptr), camera(nullptr), film(nullptr) {}

            Integrator(int _samples, int _depth, uint64_t _seed, Film * _film) : samples(_samples), sqrtSamples(int(std::lround(std::sqrt(_samples)))), depth(_depth), seed(_seed), scene(nullptr), camera(nullptr), film(_film) {}

            void setScene(Scene * s) { scene = s; }

            void setCamera(Camera * c) { camera = c; }

            LAMBDA_HOST_DEVICE int getSamples() const { return samples; }

            LAMBDA_HOST_DEVICE const Camera * getCamera() const { return camera; }

            LAMBDA_HOST_DEVICE const Film * getFilm() const { return film; }

            void renderImage(int * completed) {
                #pragma omp parallel for schedule(guided)
                for (int py = 0; py < film->getHeight(); py++)
                    for (int px = 0; px < film->getWidth(); px++) {
                        Random state(seed, py * film->getWidth() + px);

                        renderPixel(px, py, state);

                        std::atomic_ref<int>(*completed).fetch_add(1, std::memory_order_relaxed);
                    }
            }

        private:
            int samples, sqrtSamples, depth;
            uint64_t seed;
            Scene * scene;
            Camera * camera;
            Film * film;

            LAMBDA_HOST_DEVICE void renderPixel(int px, int py, Random & state) {
                Vector<Float, 3> color;

                Float invSqrtSamples = 1 / Float(sqrtSamples);
                Float invWidth = 1 / Float(film->getWidth());
                Float invHeight = 1 / Float(film->getHeight());

                for (int i = 0; i < sqrtSamples; i++)
                    for (int j = 0; j < sqrtSamples; j++) {
                        Float u = (Float(px) + (Float(i) + utils::randomFloat(state)) * invSqrtSamples) * invWidth;
                        Float v = (Float(py) + (Float(j) + utils::randomFloat(state)) * invSqrtSamples) * invHeight;

                        Ray ray = camera->getRay(u, v, film->sampleLambdas(state));

                        SampledSpectrum spectrum = trace(ray, state);

                        color += film->toXYZ(spectrum, ray.getLambdas());
                    }

                film->setPixel(px, py, color / Float(sqrtSamples * sqrtSamples));
            }

            LAMBDA_HOST_DEVICE SampledSpectrum trace(Ray r, Random & state) const {
                SampledSpectrum radiance;
                SampledSpectrum throughput(1);
                SampledSpectrum unidirectionalDensity(1);
                SampledSpectrum lightSamplingDensity;

                bool collapsed = false;

                Vector<Float, 3> scatterPoint;

                Float totalPower = scene->getTotalPower();

                for (int i = 0; i <= depth; i++) {
                    Intersection intersection;

                    const Instance * instance = nullptr;
                    int objectIndex = -1;

                    const Material * material = nullptr;
                    const Medium * medium0 = nullptr;
                    const Medium * medium1 = nullptr;

                    bool hitSurface = false;

                    while (!material && intersection.isSurface) {
                        intersection = Intersection();

                        hitSurface = scene->hit(intersection, r);

                        if (r.getMedium()) {
                            SampledSpectrum distanceDensity, attenuation = r.getMedium()->sampleDistance(intersection, r, distanceDensity, collapsed, state);

                            if (attenuation.max() <= 0 || !(distanceDensity[0] > 0)) return radiance;

                            throughput *= attenuation;

                            distanceDensity /= distanceDensity[0];

                            unidirectionalDensity *= distanceDensity;
                            lightSamplingDensity *= distanceDensity;
                        }

                        instance = intersection.instance;
                        objectIndex = intersection.i;

                        material = nullptr;
                        medium0 = nullptr;
                        medium1 = nullptr;

                        if (instance) {
                            material = instance->getMaterial(objectIndex);
                            medium0 = intersection.frontFacing ? instance->getMedium0(objectIndex) : instance->getMedium1(objectIndex);
                            medium1 = intersection.frontFacing ? instance->getMedium1(objectIndex) : instance->getMedium0(objectIndex);
                        }

                        if (intersection.isSurface) {
                            if (!hitSurface) break;

                            if (!material) r = Ray(intersection.transformedPoint, r.getDirection(), r.getLambdas(), dot(intersection.transformedNormal, r.getDirection()) > constants::EPSILON ? medium0 : medium1);
                        }
                    }

                    if ((!hitSurface && intersection.isSurface) || (material && material->isEmissive())) {
                        SampledSpectrum combinedDensity = unidirectionalDensity;

                        if (totalPower > constants::EPSILON) {
                            lightSamplingDensity *= scene->lightProbabilityDensity(instance, objectIndex, scatterPoint, r.getDirection());

                            combinedDensity += lightSamplingDensity;
                        }

                        Float denominator = combinedDensity.average();

                        if (denominator <= 0) break;

                        SampledSpectrum emission;

                        if (material) emission = material->emission(intersection, r.getLambdas());
                        else
                            for (int j = 0; j < scene->getNumBackgrounds(); j++) emission += scene->getBackground(j)->evaluate(r);

                        radiance += throughput * emission / denominator;

                        break;
                    }

                    radiance += throughput * directLighting(intersection, r, unidirectionalDensity, collapsed, i == depth, state);

                    if (i == depth) break;

                    bool previousCollapsed = collapsed;

                    SampledSpectrum scatterDensity, attenuation = intersection.isSurface ? material->sample(intersection, r, medium1, scatterDensity, collapsed, state) : r.getMedium()->sample(intersection, r, scatterDensity, collapsed, state);

                    if (attenuation.max() <= 0 || !(scatterDensity[0] > 0)) break;

                    if (!previousCollapsed && collapsed) {
                        unidirectionalDensity = SampledSpectrum(0);
                        unidirectionalDensity[0] = 1;
                    }

                    lightSamplingDensity = (!intersection.isSurface || (dot(intersection.transformedNormal, r.getDirection()) > constants::EPSILON && !material->isSpecular(intersection))) ? unidirectionalDensity / scatterDensity[0] : SampledSpectrum(0);
                    unidirectionalDensity *= scatterDensity / scatterDensity[0];

                    scatterPoint = intersection.transformedPoint;

                    throughput *= attenuation;

                    if (r.getDirection().lengthSquared() <= constants::EPSILON) break;

                    if (i >= constants::RR_START_DEPTH) {
                        Float q = 1 - utils::clamp((throughput / unidirectionalDensity.average()).max(), Float(0.05), Float(0.95));

                        if (utils::randomFloat(state) < q) break;

                        throughput /= 1 - q;
                    }
                }

                return radiance;
            }

            LAMBDA_HOST_DEVICE SampledSpectrum directLighting(const Intersection & intersection, const Ray & r, const SampledSpectrum & unidirectionalDensity, bool collapsed, bool last, Random & state) const {
                const Material * material = intersection.isSurface ? intersection.instance->getMaterial(intersection.i) : nullptr;

                if ((!intersection.isSurface || (material && !material->isSpecular(intersection))) && scene->getTotalPower() > 0) {
                    const Instance * lightInstance = nullptr;
                    int lightObjectIndex;

                    Float lightDensity;

                    Vector<Float, 3> lightDirection = scene->sampleLight(intersection.transformedPoint, lightDensity, lightInstance, lightObjectIndex, state);

                    Ray shadowRay(intersection.transformedPoint, lightDirection, r.getLambdas(), r.getMedium());

                    Intersection shadowIntersection;

                    Float cosTheta = intersection.isSurface ? dot(intersection.transformedNormal, lightDirection) : 1;

                    if (lightDensity > 0 && cosTheta > 0 && !scene->hit(shadowIntersection, shadowRay, lightInstance, lightObjectIndex)) {
                        SampledSpectrum emission;

                        if (lightInstance) emission = lightInstance->getMaterial(lightObjectIndex)->emission(shadowIntersection, shadowRay.getLambdas());
                        else
                            for (int i = 0; i < scene->getNumBackgrounds(); i++) emission += scene->getBackground(i)->evaluate(shadowRay);

                        SampledSpectrum scatterToLightDensity, attenuation = !intersection.isSurface ? r.getMedium()->evaluate(r, shadowRay, scatterToLightDensity, collapsed) : material->evaluate(intersection, r, shadowRay, scatterToLightDensity, collapsed);

                        SampledSpectrum transmittance = SampledSpectrum(1);

                        const Instance * interfaceInstance = nullptr;
                        int interfaceObjectIndex;

                        do {
                            shadowIntersection = Intersection();

                            bool hit = scene->hit(shadowIntersection, shadowRay);

                            if (!hit) shadowIntersection.t = constants::MAX;

                            SampledSpectrum mediumTransmittance = shadowRay.getMedium() ? shadowRay.getMedium()->transmittance(shadowIntersection, shadowRay, collapsed) : SampledSpectrum(1);

                            if (utils::randomFloat(state) >= mediumTransmittance[0]) {
                                transmittance = SampledSpectrum(0);

                                break;
                            }

                            transmittance *= mediumTransmittance / mediumTransmittance[0];

                            interfaceInstance = shadowIntersection.instance;

                            if (!interfaceInstance) break;

                            interfaceObjectIndex = shadowIntersection.i;

                            shadowRay = Ray(shadowIntersection.transformedPoint, lightDirection, r.getLambdas(), shadowIntersection.frontFacing ? interfaceInstance->getMedium1(interfaceObjectIndex) : interfaceInstance->getMedium0(interfaceObjectIndex));
                        } while (!interfaceInstance->getMaterial(interfaceObjectIndex));

                        if (transmittance.max() > 0) {
                            SampledSpectrum directLightingDensity = lightDensity;

                            if (!last) directLightingDensity += scatterToLightDensity;

                            directLightingDensity *= unidirectionalDensity * transmittance;

                            Float denominator = directLightingDensity.average();

                            if (denominator > 0) return attenuation * transmittance * cosTheta * emission / denominator;
                        }
                    }
                }

                return 0;
            }

            friend LAMBDA_GLOBAL void renderKernel(Integrator * integrator, int * completed);
    };
}