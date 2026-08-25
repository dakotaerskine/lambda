#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#ifdef USE_OPENMP
    #include <omp.h>
#endif

#include "core/buffer.h"
#include "core/constants.h"
#include "core/parser.h"
#include "core/platform.h"
#include "core/utils.h"
#include "core/writer.h"
#include "math/matrix.h"
#include "math/random.h"
#include "math/spectrum.h"
#include "render/renderer.h"
#include "scene/background.h"
#include "scene/bvh.h"
#include "scene/material.h"
#include "scene/object.h"
#include "scene/scene.h"
#include "scene/texture.h"

void loadLUT(const std::string & file, ColorSpace space, std::string & errorPrefix, std::vector<float> & scale, std::vector<float> & lut) {
    errorPrefix = file + ": ";

    if (!Parser::parseUpsamplingLUT(file, scale, lut)) {
        solveGrid(space, scale, lut);

        Writer::writeUpsamplingLUT(file, scale, lut);
    }
}

int main(int argc, char * argv[]) {
    if (argc != 2 && argc != 3) {
        std::cerr << argv[0] << ": invalid number of arguments" << std::endl;
        return 1;
    }

    std::string input(argv[1]);

    if (!Parser::hasValidExtension(input)) {
        std::cerr << input << ": invalid file extension" << std::endl;
        return 1;
    }

    std::string output;

    if (argc == 3) {
        output = argv[2];

        if (!Writer::hasValidExtension(output)) {
            std::cerr << output << ": invalid file extension" << std::endl;
            return 1;
        }
    } else {
        output = input;

        output.erase(output.size() - 4);

        output += ".exr";
    }

    std::string errorPrefix;

    try {
        Matrix3<double> h_SRGB_TO_XYZ = toXYZMatrix(CHROMATICITIES_SRGB[0], CHROMATICITIES_SRGB[1], CHROMATICITIES_SRGB[2], CHROMATICITIES_SRGB[3], CHROMATICITIES_SRGB[4], CHROMATICITIES_SRGB[5], CHROMATICITIES_SRGB[6], CHROMATICITIES_SRGB[7]);
        Matrix3<double> h_XYZ_TO_SRGB = inverse(h_SRGB_TO_XYZ);

        Matrix3<double> h_REC2020_TO_XYZ = toXYZMatrix(CHROMATICITIES_REC2020[0], CHROMATICITIES_REC2020[1], CHROMATICITIES_REC2020[2], CHROMATICITIES_REC2020[3], CHROMATICITIES_REC2020[4], CHROMATICITIES_REC2020[5], CHROMATICITIES_REC2020[6], CHROMATICITIES_REC2020[7]);
        Matrix3<double> h_XYZ_TO_REC2020 = inverse(h_REC2020_TO_XYZ);

        Matrix3<double> h_ACES2065_TO_XYZ = toXYZMatrix(CHROMATICITIES_ACES2065[0], CHROMATICITIES_ACES2065[1], CHROMATICITIES_ACES2065[2], CHROMATICITIES_ACES2065[3], CHROMATICITIES_ACES2065[4], CHROMATICITIES_ACES2065[5], CHROMATICITIES_ACES2065[6], CHROMATICITIES_ACES2065[7]);
        Matrix3<double> h_XYZ_TO_ACES2065 = inverse(h_ACES2065_TO_XYZ);

        Matrix3<double> h_BRADFORD_INVERSE = inverse(Matrix3<double>(BRADFORD));
    
        ConstantBuffer<double> d_SRGB_TO_XYZ(h_SRGB_TO_XYZ.data(), 3 * 3);
        ConstantBuffer<double> d_XYZ_TO_SRGB(h_XYZ_TO_SRGB.data(), 3 * 3);

        d_SRGB_TO_XYZ.copy(SRGB_TO_XYZ);
        d_XYZ_TO_SRGB.copy(XYZ_TO_SRGB);

        ConstantBuffer<double> d_REC2020_TO_XYZ(h_REC2020_TO_XYZ.data(), 3 * 3);
        ConstantBuffer<double> d_XYZ_TO_REC2020(h_XYZ_TO_REC2020.data(), 3 * 3);

        d_REC2020_TO_XYZ.copy(REC2020_TO_XYZ);
        d_XYZ_TO_REC2020.copy(XYZ_TO_REC2020);

        ConstantBuffer<double> d_ACES2065_TO_XYZ(h_ACES2065_TO_XYZ.data(), 3 * 3);
        ConstantBuffer<double> d_XYZ_TO_ACES2065(h_XYZ_TO_ACES2065.data(), 3 * 3);

        d_ACES2065_TO_XYZ.copy(ACES2065_TO_XYZ);
        d_XYZ_TO_ACES2065.copy(XYZ_TO_ACES2065);

        ConstantBuffer<double> d_BRADFORD_INVERSE(h_BRADFORD_INVERSE.data(), 3 * 3);

        d_BRADFORD_INVERSE.copy(BRADFORD_INVERSE);

        ConstantBuffer<int> d_PERMUTATION(&h_PERMUTATION[0], PERMUTATION_SIZE);
        ConstantBuffer<double> d_CIE_XYZ_1931(&h_CIE_XYZ_1931[0][0], CIE_LAMBDA_BINS * 3);

        d_PERMUTATION.copy(PERMUTATION);
        d_CIE_XYZ_1931.copy(CIE_XYZ_1931);

        std::vector<float> h_UPSAMPLING_SCALE_SRGB(UPSAMPLING_RESOLUTION);
        std::vector<float> h_UPSAMPLING_LUT_SRGB(3 * UPSAMPLING_RESOLUTION * UPSAMPLING_RESOLUTION * UPSAMPLING_RESOLUTION * 3);

        loadLUT("data/srgb.coeff", ColorSpace::SRGB, errorPrefix, h_UPSAMPLING_SCALE_SRGB, h_UPSAMPLING_LUT_SRGB);

        std::vector<float> h_UPSAMPLING_SCALE_REC2020(UPSAMPLING_RESOLUTION);
        std::vector<float> h_UPSAMPLING_LUT_REC2020(3 * UPSAMPLING_RESOLUTION * UPSAMPLING_RESOLUTION * UPSAMPLING_RESOLUTION * 3);
        
        loadLUT("data/rec2020.coeff", ColorSpace::REC2020, errorPrefix, h_UPSAMPLING_SCALE_REC2020, h_UPSAMPLING_LUT_REC2020);

        std::vector<float> h_UPSAMPLING_SCALE_ACES2065(UPSAMPLING_RESOLUTION);
        std::vector<float> h_UPSAMPLING_LUT_ACES2065(3 * UPSAMPLING_RESOLUTION * UPSAMPLING_RESOLUTION * UPSAMPLING_RESOLUTION * 3);

        loadLUT("data/aces2065-1.coeff", ColorSpace::ACES2065, errorPrefix, h_UPSAMPLING_SCALE_ACES2065, h_UPSAMPLING_LUT_ACES2065);

        Buffer<float> d_UPSAMPLING_SCALE_SRGB(h_UPSAMPLING_SCALE_SRGB);
        Buffer<float> d_UPSAMPLING_LUT_SRGB(h_UPSAMPLING_LUT_SRGB);

        UPSAMPLING_SCALE_SRGB = d_UPSAMPLING_SCALE_SRGB.data();
        UPSAMPLING_LUT_SRGB = d_UPSAMPLING_LUT_SRGB.data();

        Buffer<float> d_UPSAMPLING_SCALE_REC2020(h_UPSAMPLING_SCALE_REC2020);
        Buffer<float> d_UPSAMPLING_LUT_REC2020(h_UPSAMPLING_LUT_REC2020);

        UPSAMPLING_SCALE_REC2020 = d_UPSAMPLING_SCALE_REC2020.data();
        UPSAMPLING_LUT_REC2020 = d_UPSAMPLING_LUT_REC2020.data();

        Buffer<float> d_UPSAMPLING_SCALE_ACES2065(h_UPSAMPLING_SCALE_ACES2065);
        Buffer<float> d_UPSAMPLING_LUT_ACES2065(h_UPSAMPLING_LUT_ACES2065);

        UPSAMPLING_SCALE_ACES2065 = d_UPSAMPLING_SCALE_ACES2065.data();
        UPSAMPLING_LUT_ACES2065 = d_UPSAMPLING_LUT_ACES2065.data();

        errorPrefix = "";

        ColorSpace space;
        Renderer renderer;
        std::vector<DenseSpectrum<Float>> spectra;
        std::vector<DenseSpectrum<Complex>> complexSpectra;
        Background background;
        std::vector<Object> objects;
        std::vector<Instance> instances;
        std::vector<BVHNode> nodes;
        std::vector<int> lightInstances;
        std::vector<int> lightObjects;
        std::vector<Float> lightPowers;
        Float totalLightPower = 0;
        std::vector<Material> materials;
        std::vector<int> materialProperties;
        std::vector<ScalarTexture> scalarTextures;
        std::vector<SpectrumTexture> spectrumTextures;
        std::vector<Float> images;

        Parser::parseLRD(input, space, renderer, spectra, complexSpectra, background, objects, instances, nodes, lightInstances, lightObjects, lightPowers, totalLightPower, materials, materialProperties, scalarTextures, spectrumTextures, images);

        errorPrefix = std::string(argv[0]) + ": ";

        std::vector<Float> h_output(renderer.getTotalPixels() * 3);

        Buffer<DenseSpectrum<Float>> d_spectra(spectra);
        Buffer<DenseSpectrum<Complex>> d_complexSpectra(complexSpectra);
        Buffer<Object> d_objects(objects);
        Buffer<Instance> d_instances(instances);
        Buffer<BVHNode> d_nodes(nodes);
        Buffer<int> d_lightInstances(lightInstances);
        Buffer<int> d_lightObjects(lightObjects);
        Buffer<Float> d_lightPowers(lightPowers);
        Buffer<Material> d_materials(materials);
        Buffer<int> d_materialProperties(materialProperties);
        Buffer<ScalarTexture> d_scalarTextures(scalarTextures);
        Buffer<SpectrumTexture> d_spectrumTextures(spectrumTextures);
        Buffer<Float> d_images(images);

        renderer.setScene(d_spectra.data(), d_complexSpectra.data(), background, d_objects.data(), d_instances.data(), d_nodes.data(), d_lightInstances.data(), d_lightObjects.data(), int(lightInstances.size()), d_lightPowers.data(), totalLightPower, d_materials.data(), d_materialProperties.data(), d_scalarTextures.data(), d_spectrumTextures.data(), d_images.data());

        Buffer<Float> d_output(h_output.size());

        renderer.setBuffer(d_output.data());

        Buffer<int> d_completed(1);

        typename std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();

        std::cout << "\r" << argv[0] << ": 0% complete" << std::flush;

        #ifdef __CUDACC__
            int BLOCK_W = 16;
            int BLOCK_H = 16;

            dim3 block(BLOCK_W, BLOCK_H);
            dim3 grid((renderer.getWidth() + BLOCK_W - 1) / BLOCK_W, (renderer.getHeight() + BLOCK_H - 1) / BLOCK_H);

            renderKernel<<<grid, block>>>(d_completed.data(), renderer, 42ULL);

            checkCudaError(cudaGetLastError(), "failed to launch kernel");
        #else
            std::thread thread(&Renderer::renderImage, &renderer, d_completed.data(), 42ULL);
        #endif

        int completed = 0;

        while (completed < renderer.getTotalPixels()) {
            #ifdef __CUDACC__
                checkCudaError(cudaPeekAtLastError(), "failed to execute kernel");
            #endif

            completed = *(volatile int *)d_completed.data();

            #ifdef USE_OPENMP
                #pragma omp critical
            #endif
            {
                std::cout << "\r" << argv[0] << ": " << int(100.0 * completed / renderer.getTotalPixels()) << "% complete" << std::flush;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        #ifdef __CUDACC__
            checkCudaError(cudaDeviceSynchronize(), "failed to synchronize device");
        #else
            thread.join();
        #endif

        std::cout << "\r" << argv[0] << ": 100% complete" << std::flush << std::endl;

        typename std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();

        std::chrono::duration<Float> duration = end - start;

        #ifdef __CUDACC__
            checkCudaError(cudaMemcpy(h_output.data(), d_output.data(), renderer.getTotalPixels() * 3 * sizeof(Float), cudaMemcpyDeviceToHost), "failed to copy to host memory");
        #else
            h_output = d_output;
        #endif

        errorPrefix = output + ": ";

        Writer::writeImage(output, space, renderer, duration.count(), h_output.data());
    }
    catch (const std::exception & e) {
        std::cerr << errorPrefix << e.what() << std::endl;
        return 1;
    }

    return 0;
}