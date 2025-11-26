#ifndef RENDERER_H
#define RENDERER_H

#include <algorithm>
#include <iostream>

#include "camera.h"
#include "scene.h"

class Renderer {
    public:
        Renderer() {}
        Renderer(int w, int h, int d, int s, int ws, double min, double max, Camera * c, Scene * sc) : width(w), height(h), depth(d), samples(s), wavelengthSamples(ws), lambdaMin(min), lambdaMax(max), lambdaStep((max - min) / (ws - 1)), camera(c), scene(sc) {}

        Camera * getCamera() const { return camera; }
        Scene * getScene() const { return scene; }

        void render(std::ostream & out) {
            out << "P3\n" << width << " " << height << "\n255\n";

            for (int j = 0; j < height; j++) {
                for (int i = 0; i < width; i++) {
                    int sqrtSamples = sqrt(samples);
                    Spectrum spectrum(wavelengthSamples);

                    for (int k = 0; k < sqrtSamples; k++)
                        for (int l = 0; l < sqrtSamples; l++)
                            for (int m = 0; m < wavelengthSamples; m++) {
                                double u = double(i + (k + randomDouble()) / sqrtSamples) / width;
                                double v = double(j + (l + randomDouble()) / sqrtSamples) / height;
                                double lambda = lambdaMin + m * lambdaStep;
                                Ray ray = camera->getRay(u, v, lambda, m);
                                spectrum[m] += trace(ray, scene, depth);
                            }

                    spectrum /= samples;

                    Vector color = spectrumToRGB(spectrum, wavelengthSamples, lambdaMin, lambdaMax);
                    color = Vector(sRGB(std::clamp(color[0], 0.0, 1.0)), sRGB(std::clamp(color[1], 0.0, 1.0)), sRGB(std::clamp(color[2], 0.0, 1.0)));
                    color = sRGBToHSV(color);
                    color[1] *= 2;
                    color[1] = std::clamp(color[1], 0.0, 1.0);
                    color = HSVTosRGB(color);

                    int ir = int(255.999 * color[0]);
                    int ig = int(255.999 * color[1]);
                    int ib = int(255.999 * color[2]);

                    out << ir << " " << ig << " " << ib << "\n";
                }

                std::cerr << "\rRendering: " << int(double(j) / height * 100) << "\% complete" << std::flush;
            }
            
            std::cerr << "\rRendering: 100\% complete" << std::flush << std::endl;
        }

    private:
        double trace(const Ray & r, Scene * s, int d) {
            Intersection intersection;

            if (s->hit(r, intersection)) {
                Ray scattered;
                double attenuation;

                if (intersection.material->scatter(r, intersection, scattered, attenuation) && d > 0) return attenuation * trace(scattered, s, d - 1);
                else return attenuation;
            }

            return s->getBackground()[r.getLambdaIndex()];
        }

        int width, height, depth, samples, wavelengthSamples;
        double lambdaMin, lambdaMax, lambdaStep;
        Camera * camera;
        Scene * scene;
};

#endif