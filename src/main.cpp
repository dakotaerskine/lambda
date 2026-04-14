#include <fstream>
#include <iostream>
#include <vector>

#include "camera.h"
#include "intersection.h"
#include "object.h"
#include "renderer.h"
#include "scene.h"

int main() {
    int width = 1000;
    int height = 1000;
    int depth = 1;
    int samples = 25;
    int wavelengthSamples = 9;
    double lambdaMin = 380;
    double lambdaMax = 780;

    Spectrum spectrum(wavelengthSamples, 1);

    Scene * scene = new Scene(spectrum);
    /**
    Material * material1 = new Lambertian(Vector(1, 0, 1));
    Material * material2 = new Metal(Vector(1, 1, 0));
    Material * material3 = new Dielectric(1.5, Vector(0, 1, 1));
    Object * object1 = new Sphere(material1, Vector(0, 1, 0), 1);
    Object * object2 = new Sphere(material2, Vector(0, 0, 1), 1);
    Object * object3 = new Plane(material3, Vector(0, 0, 0), Vector(0, 0, 1));

    Material * material1 = new Metal(Vector(0, 0.7, 0.7));
    Material * material2 = new Metal(Vector(0.8, 0.6, 0));
    Material * material3 = new Dielectric(1.05, Vector(0, 1, 1));
    Vector dir(0, -1, 1);
    Vector dir2(0, 1, 1);
    Vector dir3(1, 1, 1);
    dir.normalize();
    dir2.normalize();
    dir3.normalize();
    Object * object1 = new Plane(material1, Vector(0, 0, 0), Vector(0, 0, 1));
    Object * object2 = new Sphere(material2, Vector(1, 1, 1), 1);
    Object * object3 = new Sphere(material2, Vector(2, 2, 2), 2);
    Object * object4 = new Sphere(material2, Vector(3, 3, 3), 3);
    Object * object5 = new Plane(material3, Vector(-1, -1, 0), dir3);
    
    scene->addObject(object1);
    scene->addObject(object2);
    scene->addObject(object3);
    scene->addObject(object4);
    scene->addObject(object5);

    std::vector<double> indices;
    indices.push_back(1);
    indices.push_back(1.5);
    indices.push_back(1);
    std::vector<double> thicknesses;
    thicknesses.push_back(200);
    std::vector<double> ranges;
    ranges.push_back(500);
    std::vector<double> scales;
    scales.push_back(2);

    Material * material1 = new ThinFilm(indices, thicknesses, ranges, scales);

    indices.clear();
    indices.push_back(1);
    indices.push_back(1.15);
    indices.push_back(1);
    thicknesses.clear();
    thicknesses.push_back(100);
    ranges.clear();
    ranges.push_back(100);
    scales.clear();
    scales.push_back(0.5);

    Material * material2 = new ThinFilmVector(0, -1, 0)
    std::vector<double> indices;
    indices.push_back(1);
    indices.push_back(5);2
    Material * material3 = new Dielectric(1.05, Vector(0, 1, 1));
    Material * material4 = new ThinFilm(indices, thicknesses, ranges, scales);
    Vector dir(0, -1, 1);
    Vector dir2(0, 1, 1);
    Vector dir3(0, 0, 1);
    dir.normalize();
    dir2.normalize();
    dir3.normalize();
    Object * object2 = new Sphere(material4, Vector(1, 1, 1), 1);
    Object * object3 = new Sphere(material4, Vector(2, 2, 2), 2);
    Object * object4 = new Sphere(material4, Vector(3, 3, 3), 3);
    Object * object5 = new Plane(material3, Vector(0, 0, 2), dir3);

    scene->addObject(object2);
    scene->addObject(object3);
    scene->addObject(object4);
    scene->addObject(object5);
    */

    std::vector<double> indices;
    indices.push_back(1);
    indices.push_back(5);
    indices.push_back(1.5);
    indices.push_back(5);
    indices.push_back(1);

    std::vector<double> thicknesses;
    thicknesses.push_back(1000);
    thicknesses.push_back(600);

    std::vector<double> ranges;
    ranges.push_back(400);
    ranges.push_back(600);
    
    std::vector<double> scales;
    scales.push_back(0.75);
    scales.push_back(0.375);

    Material * material1 = new Lambertian(Spectrum(wavelengthSamples, 0.5));
    Material * material2 = new ThinFilm(indices, thicknesses, ranges, scales);
    Material * material3 = new Emissive(Spectrum(wavelengthSamples, 3));

    Object * object1 = new Plane(material2, Vector(0, 0, 0), Vector(0, 0, 1));
    Object * object2 = new Plane(material2, Vector(0, 0, -1), Vector(0, 0, 1));

    scene->addObject(object1);

    std::ofstream outputFile;

    for (int i = 0; i <= 0; i++) {
        outputFile.open(std::to_string(i) + ".ppm");

        Vector position(-25 + 0.5 * i, 0, 2);
        Vector corner(-2, -2, 0);
        Vector horizontal(4, 0, 0);
        Vector vertical(0, 4, 0);

        Camera * camera = new Camera(position, corner, horizontal, vertical);

        Renderer renderer(width, height, depth, samples, wavelengthSamples, lambdaMin, lambdaMax, camera, scene);
        renderer.render(outputFile);
        
        outputFile.close();
    }

    return 0;
}
