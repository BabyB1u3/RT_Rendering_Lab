# Real Time Rendering Lab

A modern C++ graphics playground for experimenting with real-time rendering techniques and graphics algorithms.

This repository is designed as a long-term platform for graphics experiments, learning, and visualization.  
It provides a lightweight framework where different rendering techniques can be implemented and explored as independent demos.

---

# Demo Gallery

Each rendering technique in this repository is implemented as an independent demo.

Examples of experiments that may appear in this repository include:

- Forward Lighting
- Shadow Mapping
- Deferred Rendering
- Physically Based Rendering
- Screen Space Ambient Occlusion
- Bloom / HDR
- Volumetric Lighting
- Procedural Terrain
- Voxel Rendering
- Ray Tracing Experiments

Screenshots and visual results will be added as the demos evolve.

---

# Building

## Requirements

Typical requirements may include:

- C++20 compatible compiler
- CMake 3.20+
- OpenGL or other graphics API
- GLFW / SDL for windowing
- GLM for math
- FreeType or similar library for text rendering

Actual dependencies may evolve as the project grows.

---

## Build Example

Clone the repository:

    git clone <repository-url>

Generate build files:

    cmake -B build

Compile:

    cmake --build build

Run the program:

    ./build/Playground

---

# Long-Term Direction

The project is expected to gradually explore topics such as:

- modern real-time rendering
- physically based shading
- screen-space techniques
- GPU-driven rendering
- procedural generation
- ray tracing experiments

More detailed plans can be found in **docs/ROADMAP.md**.
