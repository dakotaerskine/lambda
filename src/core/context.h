#pragma once

#include <iterator>
#include <vector>

#include "core/buffer.h"
#include "render/renderer.h"

namespace lambda {
    class Context {
        public:
            Context() : background(1), scene(1), camera(1), renderer(1) {
                objectOffsets.push_back(0);
                lightObjectOffsets.push_back(0);
            }

            template <typename T>
            void setOrdered(int i, T * value, bool light = false) {
                if (light) {
                    if constexpr (std::is_same_v<T, Object>) lightObjects[i] = value;
                    else if constexpr (std::is_same_v<T, Instance>) lightInstances[i] = value;
                }
                else {
                    if constexpr (std::is_same_v<T, Object>) orderedObjects[i] = value;
                    else if constexpr (std::is_same_v<T, Instance>) orderedInstances[i] = value;
                }
            }

            void setBackground(const Background & b) { background[0] = b; }

            void setCamera(const Camera & c) { camera[0] = c; }

            template <typename T>
            void reserveNodes(int n, bool light = false) {
                if (light) {
                    if constexpr (std::is_same_v<T, Object>) lightNodes.reserve(n);
                    else if constexpr (std::is_same_v<T, Instance>) lightInstanceNodes.reserve(n);
                }
                else {
                    if constexpr (std::is_same_v<T, Object>) nodes.reserve(n);
                    else if constexpr (std::is_same_v<T, Instance>) instanceNodes.reserve(n);
                }
            }

            template <typename T>
            T * addSpectrum(const T & spectrum) {
                if constexpr (std::is_same_v<T, DenseSpectrum<Float>>) return spectra.push(spectrum);
                else if constexpr (std::is_same_v<T, DenseSpectrum<Complex>>) return complexSpectra.push(spectrum);
                else return nullptr;
            }

            Float * addImage(const Float * begin, int n) { return images.push(begin, n); }

            ScalarTexture * addScalarTexture(const ScalarTexture & texture) { return scalarTextures.push(texture); }

            SpectrumTexture * addSpectrumTexture(const SpectrumTexture & texture) { return spectrumTextures.push(texture); }

            template <typename T>
            T ** addMaterialProperty(T ** begin, int n) {
                if constexpr (std::is_same_v<T, DenseSpectrum<Float>>) return spectrumMaterialProperties.push(begin, n);
                else if constexpr (std::is_same_v<T, DenseSpectrum<Complex>>) return complexSpectrumMaterialProperties.push(begin, n);
                else if constexpr (std::is_same_v<T, ScalarTexture>) return scalarTextureMaterialProperties.push(begin, n);
                else if constexpr (std::is_same_v<T, SpectrumTexture>) return spectrumTextureMaterialProperties.push(begin, n);
                else return nullptr;
            }

            Medium * addMedium(const Medium & medium) { return media.push(medium); }

            Material * addMaterial(const Material & material) { return materials.push(material); }

            void addObject(const Object & object) { currentObjects.push_back(object); }

            void addObjects(const Object * begin, int n) { currentObjects.insert(currentObjects.end(), begin, begin + n); }

            void flushObjects() {
                if (!currentObjects.empty()) {
                    objects.push(currentObjects.data(), int(std::ssize(currentObjects)));

                    objectOffsets.push_back(objects.size());

                    currentObjects.clear();
                }
            }

            Vector<Float, 3> * addMeshVertices(const Vector<Float, 3> * begin, int n) { return meshVertices.push(begin, n); }

            Vector<Float, 3> * addMeshNormals(const Vector<Float, 3> * begin, int n) { return meshNormals.push(begin, n); }

            Vector<Float, 2> * addMeshTextureCoordinates(const Vector<Float, 2> * begin, int n) { return meshTextureCoordinates.push(begin, n); }

            int * addMeshFaces(const int * begin, int n) { return meshFaces.push(begin, n); }

            Mesh * addMesh(const Mesh & mesh) { return meshes.push(mesh); }

            Instance * addInstance(const Instance & instance) { return instances.push(instance); }

            Object ** addOrderedObject(Object * object) { return orderedObjects.push(object); }

            Object ** addOrderedObjects(Object ** begin, int n) { return orderedObjects.push(begin, n); }

            Instance ** addOrderedInstance(Instance * instance) { return orderedInstances.push(instance); }

            Instance ** addOrderedInstances(Instance ** begin, int n) { return orderedInstances.push(begin, n); }

            template <typename T>
            BVHNode<T> * addNode(const BVHNode<T> & node, bool light = false) {
                if (light) {
                    if constexpr (std::is_same_v<T, Object>) return lightNodes.push(node);
                    else if constexpr (std::is_same_v<T, Instance>) return lightInstanceNodes.push(node);
                    else return nullptr;
                }
                else {
                    if constexpr (std::is_same_v<T, Object>) return nodes.push(node);
                    else if constexpr (std::is_same_v<T, Instance>) return instanceNodes.push(node);
                    else return nullptr;
                }
            }

            void addLightObject(Object * object) { currentLightObjects.push_back(object); }

            void flushLightObjects() {
                if (!currentLightObjects.empty()) {
                    lightObjects.push(currentLightObjects.data(), int(std::ssize(currentLightObjects)));

                    lightObjectOffsets.push_back(lightObjects.size());

                    currentLightObjects.clear();
                }
            }

            Instance ** addLightInstance(Instance * instance) { return lightInstances.push(instance); }

            Instance ** addLightInstances(Instance ** begin, int n) { return lightInstances.push(begin, n); }

            Float * addConditionalCDF(Float * c, int n) { return conditionalCDF.push(c, n); }

            Float * addMarginalCDF(Float * m, int n) { return marginalCDF.push(m, n); }

            Float * addOutput(int n) { return output.extend(n); }

            bool hasObjects() const { return !objects.empty(); }

            Object * getObject(int i) { return &objects[i]; }

            int getOffset(int i) const { return objectOffsets[i]; }

            int getObjectIndex() const { return int(std::ssize(objectOffsets)) - 1; }

            int getObjectCount() const { return objects.size(); }

            int getObjectCount(int i) const { return objectOffsets[i + 1] - objectOffsets[i]; }

            bool hasInstances() const { return !instances.empty(); }

            Instance * getInstance(int i) { return &instances[i]; }

            int getInstanceCount() const { return instances.size(); }

            int getLightOffset(int i) const { return lightObjectOffsets[i]; }

            int getLightObjectIndex() const { return int(std::ssize(lightObjectOffsets)) - 1; }

            template <typename T>
            T * getOrdered(int i, bool light = false) {
                if (light) {
                    if constexpr (std::is_same_v<T, Object>) return lightObjects[i];
                    else if constexpr (std::is_same_v<T, Instance>) return lightInstances[i];
                    else return nullptr;
                }
                else {
                    if constexpr (std::is_same_v<T, Object>) return orderedObjects[i];
                    else if constexpr (std::is_same_v<T, Instance>) return orderedInstances[i];
                    else return nullptr;
                }
            }

            template <typename T>
            T ** getOrderedBase(int i, bool light = false) {
                if (light) {
                    if constexpr (std::is_same_v<T, Object>) return &lightObjects[i];
                    else if constexpr (std::is_same_v<T, Instance>) return &lightInstances[i];
                    else return nullptr;
                }
                else {
                    if constexpr (std::is_same_v<T, Object>) return &orderedObjects[i];
                    else if constexpr (std::is_same_v<T, Instance>) return &orderedInstances[i];
                    else return nullptr;
                }
            }

            bool hasLightObjects() const { return !lightObjects.empty(); }

            int getLightObjectCount() const { return lightObjects.size(); }

            bool hasLightInstances() const { return !lightInstances.empty(); }

            int getLightInstanceCount() const { return lightInstances.size(); }

            Background * getBackground() { return background.data(); }

            Scene * getScene() { return scene.data(); }

            Camera * getCamera() { return camera.data(); }

            Renderer * getRenderer() { return renderer.data(); }

        private:
            std::vector<Object> currentObjects;
            std::vector<int> objectOffsets;

            std::vector<Object *> currentLightObjects;
            std::vector<int> lightObjectOffsets;

            Buffer<DenseSpectrum<Float>> spectra;
            Buffer<DenseSpectrum<Complex>> complexSpectra;
            Buffer<Float> images;
            Buffer<ScalarTexture> scalarTextures;
            Buffer<SpectrumTexture> spectrumTextures;
            Buffer<DenseSpectrum<Float> *> spectrumMaterialProperties;
            Buffer<DenseSpectrum<Complex> *> complexSpectrumMaterialProperties;
            Buffer<ScalarTexture *> scalarTextureMaterialProperties;
            Buffer<SpectrumTexture *> spectrumTextureMaterialProperties;
            Buffer<Medium> media;
            Buffer<Material> materials;
            Buffer<Object> objects;
            Buffer<Vector<Float, 3>> meshVertices;
            Buffer<Vector<Float, 3>> meshNormals;
            Buffer<Vector<Float, 2>> meshTextureCoordinates;
            Buffer<int> meshFaces;
            Buffer<Mesh> meshes;
            Buffer<Instance> instances;
            Buffer<Object *> orderedObjects;
            Buffer<Instance *> orderedInstances;
            Buffer<BVHNode<Object>> nodes;
            Buffer<BVHNode<Instance>> instanceNodes;
            Buffer<Object *> lightObjects;
            Buffer<Instance *> lightInstances;
            Buffer<BVHNode<Object>> lightNodes;
            Buffer<BVHNode<Instance>> lightInstanceNodes;
            Buffer<Float> conditionalCDF;
            Buffer<Float> marginalCDF;
            Buffer<Background> background;
            Buffer<Scene> scene;
            Buffer<Camera> camera;
            Buffer<Float> output;
            Buffer<Renderer> renderer;
    };
}