#include <curand_kernel.h>

#include "camera.cuh"
#include "scene.cuh"

__global__ void initCurand(curandState * states, int totalPixels, unsigned long long seed) {
    int index = blockIdx.x * blockDim.x + threadIdx.x;
    if (index < totalPixels)
        curand_init(seed, index, 0, &states[index]);
}

__device__ double trace(Ray r, const Scene & s, int d, curandState * state) {
    double totalAttenuation = 1;

    for (int i = 0; i <= d; i++) {
        Intersection intersection;

        if (!s.hit(r, intersection)) return totalAttenuation * s.getBackground()[r.getLambdaIndex()];

        Ray scattered;
        double attenuation;

        if (!s.getObject().getMaterial().scatter(r, intersection, scattered, attenuation, state)) return totalAttenuation * attenuation;

        totalAttenuation *= attenuation;
        r = scattered;
    }
    return totalAttenuation;
}

__global__ void renderKernel(double * __restrict__ outputBuffer, int width, int height, int depth, int sqrtSamples, double lambdaMin, double lambdaStep, Camera camera, Scene scene, curandState * randStates) {
    int px = blockIdx.x * blockDim.x + threadIdx.x;
    int py = blockIdx.y * blockDim.y + threadIdx.y;

    if (px >= width || py >= height) return;

    double cx = px - width * 0.5;
    double cy = py - height * 0.5;
    double r = width * 0.5;

    if (cx * cx + cy * cy > r * r) return;

    int idx = py * width + px;
    curandState * state = &randStates[idx];

    Spectrum spectrum;

    for (int k = 0; k < sqrtSamples; k++)
        for (int l = 0; l < sqrtSamples; l++)
            for (int m = 0; m < 9; m++) {
                double u = double((width - 1 - px) + (k + randomDouble(state)) / sqrtSamples) / width;
                double v = double(py + (l + randomDouble(state)) / sqrtSamples) / height;
                double lambda = lambdaMin + m * lambdaStep;
                Ray ray = camera.getRay(u, v, lambda, m);
                spectrum[m] += trace(ray, scene, depth, state);
            }

    spectrum /= sqrtSamples * sqrtSamples;

    Vector color = spectrumToRGB(spectrum, lambdaMin, lambdaMin + 8 * lambdaStep);
    color = Vector(sRGB(fmin(fmax(color[0], 0.0), 1.0)), sRGB(fmin(fmax(color[1], 0.0), 1.0)), sRGB(fmin(fmax(color[2], 0.0), 1.0)));
    color = sRGBToHSV(color);
    color[1] *= 2;
    color[1] = fmin(fmax(color[1], 0.0), 1.0);
    color = HSVTosRGB(color);

    outputBuffer[idx * 3 + 0] = color[0];
    outputBuffer[idx * 3 + 1] = color[1];
    outputBuffer[idx * 3 + 2] = color[2];
}