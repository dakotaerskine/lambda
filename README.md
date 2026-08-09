# Lambda
A physically-based spectral path tracer implemented in C++ and CUDA built around wave-optical material appearance.

## Building
Lambda requires CMake 3.18 or later and a C++20 compiler. CUDA is optional and auto-detected: if `nvcc` isn't found, the build falls back to the CPU implementation. To force a CPU build manually, run CMake with `-DCPU=ON`.

```sh
mkdir build && cd build
cmake ../src
make
```

## Running
Lambda takes in a single `.lrd` scene file and writes a `.ppm` image next to it (`render.lrd` → `render.ppm`).

```sh
./pathtracer_cpu render.lrd
./pathtracer_gpu render.lrd
```

## Lambda Render Description (`.lrd`)
The Lambda Render Description is a plain text file format with one command per line. `#` starts a line comment. The first command must be `Render`.

### `Render`
`depth` is the maximum bounce count, `samples` must be a perfect square, `wavelengthSamples` ranges from 2 to 64, and `lambdaMin` and `lambdaMax` are wavelengths within [360, 830], the range covered by the built-in CIE 1931 tables.

```
Render <width> <height> <depth> <samples> <wavelengthSamples> <lambdaMin> <lambdaMax>
```

### `Background`
`background` is a radiance spectrum returned for rays that hit nothing.

```
Background (background)
```

### `Camera`
`corner` is the image plane's top-left position and `horizontal` and `vertical` span the plane.

```
Camera (position) (corner) (horizontal) (vertical)
```

### `Texture`
`scalar` textures are used to define material properties like film thickness and `spectrum` textures are used to define per-wavelength material properties like albedo and emission. `name` is a unique identifier used to map textures to materials.

```
Texture <name> scalar constant <value>
Texture <name> scalar perlin <min> <max> <frequency>
Texture <name> scalar worley <min> <max> <frequency>
Texture <name> spectrum constant (value)
Texture <name> spectrum checker (value1) (value2) <scale>
```

### `Material`
Most material properties are described by the names of previously defined textures, with the notable exception of refractive indices. Alternatively, a constant value can be used. `name` is a unique identifier used to map materials to objects.

```
Material <name> lambertian <albedo>
Material <name> metal <albedo>
Material <name> dielectric <n0> <n1>
Material <name> emissive <emission>
Material <name> thinfilm [n] [d]
```

### `Object`
`material` is the name of a previously defined material.

```
Object sphere <material> (center) <radius>
Object plane <material> (point) (normal)
```