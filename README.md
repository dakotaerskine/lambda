# Lambda
A physically-based spectral path tracer implemented in C++ and CUDA built around wave-optical material appearance.

<div style="text-align: center"><img src="https://dakotaerskine.github.io/images/lambda.png" alt="Lambda" width="350" height="350"></div>

## Building
Lambda requires CMake 3.24 or later and a C++20 compiler. CUDA is optional and auto-detected: if `nvcc` isn't found, the build falls back to the CPU implementation. To force a CPU build manually, run CMake with `-DCPU=ON`.

```sh
cmake -S . -B build
cmake --build build
```

## Running
Lambda takes in a single `.lrd` scene file as well as an optional output file path and writes an `.exr`, `.pfm`, or `.png` image.

```sh
./lambda render.lrd
```

## Lambda Render Description (`.lrd`)
The Lambda Render Description is a plain text file format with one command per line. `#` starts a line comment. The first command must be `Render`. `none` can be used in place of media and materials.

### `Render`
`space` is one of `srgb`, `rec2020`, or `aces2065-1`, `depth` is the maximum bounce count, `samples` must be a perfect square, and `lambdaMin` and `lambdaMax` are wavelengths within [360, 830], the range covered by the built-in CIE 1931 tables.

```
Render <space> <width> <height> <samples> <depth> <lambdaMin> <lambdaMax> <seed>
```

### `Texture`
`scalar` textures are used to define material properties like film thickness and `spectrum` textures are used to define per-wavelength material properties like albedo and emission. `name` is a unique identifier used to map textures to materials.

```
Texture <name> scalar constant <value>
Texture <name> scalar scale <texture1> <texture2>
Texture <name> scalar mix <texture1> <texture2> <factor>
Texture <name> scalar checker <value1> <value2> <uScale> <vScale> <uOffset> <vOffset>
Texture <name> scalar perlin <frequency> <roughness> <octaves>
Texture <name> scalar worley <frequency> <roughness> <octaves>
Texture <name> scalar image <file> <uScale> <vScale> <uOffset> <vOffset>
Texture <name> spectrum constant (value)
Texture <name> spectrum scale <texture1> <texture2>
Texture <name> spectrum mix <texture1> <texture2> <factor>
Texture <name> spectrum checker (value1) (value2) <uScale> <vScale> <uOffset> <vOffset>
Texture <name> spectrum scalar <texture>
Texture <name> spectrum image <file> <uScale> <vScale> <uOffset> <vOffset>
```

### `Medium`
Some medium properties are described by the names of previously defined textures. `name` is a unique identifier used to map media to objects.

```
Medium <name> homogeneous (sigmaA) (sigmaS) <g>
```

### `Material`
Most material properties are described by the names of previously defined textures, with the notable exception of refractive indices. Alternatively, a constant value can be used. `name` is a unique identifier used to map materials to objects.

```
Material <name> lambertian <albedo>
Material <name> mirror <albedo>
Material <name> conductor (n0) (n1) <uAlpha> <vAlpha>
Material <name> dielectric (n0) (n1) <uAlpha> <vAlpha>
Material <name> coated [(n)] [<d>] <substrate>
Material <name> emissive <emission>
```

### `Object`
The same `name` can be used for consecutive object declarations. `material` is the name of a previously defined material. `medium0` and `medium1` are the names of previously defined media.

```
Object <name> sphere <material> <medium0> <medium1> <radius>
Object <name> disk <material> <medium0> <medium1> <radius>
Object <name> cylinder <material> <medium0> <medium1> <radius> <height>
Object <name> tri <material> <medium0> <medium1> <file>
Object <name> tri <material> <medium0> <medium1> [(vertices)] [(normals)] [(textureCoordinates)] [(faces)]
Object <name> patch <material> <medium0> <medium1> <file>
Object <name> patch <material> <medium0> <medium1> [(vertices)] [(normals)] [(textureCoordinates)] [(faces)]
```

### `Instance`
`object` is the name of a previously defined object. `material` is the name of a previously defined material. `medium0` and `medium1` are the names of previously defined media.

```
Instance <object> <material> <medium0> <medium1> (translation) (rotation) (scale)
```

### `Background`
`emission` is a texture returned for rays that hit nothing.

```
Background equirectangular <emission> (rotation)
Background octahedral <emission> (rotation)
```

### `Camera`
`lookAt` is a point in space that the camera points towards. `fov` and `height` describe the span of their respective cameras.

```
Camera perspective <medium> (position) (lookAt) (up) <fov>
Camera orthographic <medium> (position) (lookAt) (up) <height>
Camera spherical <medium> (position) (lookAt) (up)
```