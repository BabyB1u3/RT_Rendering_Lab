# Roadmap

This document outlines the long-term development plan for the Real Time Rendering Lab.

The goal of the project is to gradually explore modern graphics techniques through independent demos,
building on a shared framework of core rendering infrastructure.

The roadmap is divided into phases that represent increasing complexity of rendering techniques.

[中文路线图](../zh-CN/roadmap.zh-CN.md)

---

## Phase 1 — Framework Foundation ✅

Goal: establish a stable environment for graphics experiments.

Completed:

- [x] Application runtime with main loop and layer stack
- [x] GLFW window and input system (keyboard, mouse, scroll)
- [x] OpenGL 4.6 rendering loop with DSA (Direct State Access)
- [x] Shader management — file loading, multi-source, `#type` preprocessing, uniform caching
- [x] Texture loading via STB Image (R8, RGB8, RGBA8, Depth formats)
- [x] Resource management with smart pointers (`Scope<T>`, `Ref<T>`)
- [x] Framebuffer abstraction — multiple color/depth attachments, pixel read-back, resize
- [x] Perspective camera with first-person debug controller
- [x] Vertex buffer / index buffer / VAO abstractions with layout description
- [x] Mesh and MeshFactory (cube, plane, fullscreen quad)
- [x] Material system (shader + texture slot binding)
- [x] ImGui integration with debug panels (FPS, memory, demo selector)
- [x] Demo framework — DemoBase, DemoRegistry, LabLayer with hot-switching
- [x] Logging and file system utilities
- [x] Unit and integration test infrastructure (Google Test)

---

## Phase 2 — Basic Real-Time Rendering

Goal: implement fundamental rendering techniques.

Progress:

- [x] Forward rendering pass
- [x] Directional light with ambient + diffuse shading
- [x] Shadow mapping — depth pass from light, shadow comparison with bias
- [x] Shadow map debug visualization (TexturePreviewPass)
- [x] Multi-pass scene renderer (ShadowPass → ForwardPass)
- [ ] Multiple light sources (point lights, spot lights)
- [ ] Specular lighting (Blinn-Phong)
- [ ] Material parameter UI (per-demo ImGui controls)
- [ ] Skybox / environment map rendering
- [ ] Normal mapping
- [ ] Model loading (OBJ / glTF)

Deliverables:

- Several lighting demos with different light types
- Basic material system with adjustable parameters
- Visual comparison screenshots

---

## Phase 3 — Modern Rendering Pipeline

Goal: transition to more advanced rendering pipelines.

Techniques:

- Deferred rendering with G-buffer architecture
- HDR rendering and tone mapping (Reinhard, ACES, etc.)
- Bloom (threshold + Gaussian blur + composite)
- Gamma correction pipeline
- Anti-aliasing (MSAA, FXAA)

Deliverables:

- Deferred renderer demo
- Comparison between forward and deferred pipelines
- HDR / bloom demo

---

## Phase 4 — Physically Based Rendering

Goal: implement physically based shading.

Topics:

- Cook-Torrance BRDF (GGX / Schlick / Smith)
- Metallic / roughness workflow
- Image-based lighting (IBL)
- Environment map pre-filtering
- BRDF integration lookup tables
- PBR material editor

Deliverables:

- PBR material demo with material spheres
- HDR environment lighting

---

## Phase 5 — Screen Space Effects

Goal: enhance visual realism using screen-space techniques.

Techniques:

- Screen-space ambient occlusion (SSAO)
- Screen-space reflections (SSR)
- Screen-space global illumination (experimental)
- Motion blur
- Depth of field

Deliverables:

- Visual comparison demos (on/off toggles)
- Parameter exploration tools

---

## Phase 6 — Procedural Geometry

Goal: explore algorithmic content generation.

Topics:

- Noise-based terrain generation (Perlin, simplex)
- Procedural landscapes with LOD
- Voxel terrain experiments
- Chunk-based world representation
- Marching cubes / dual contouring

Deliverables:

- Procedural terrain demos
- Interactive terrain visualization with real-time parameter editing

---

## Phase 7 — GPU Techniques

Goal: leverage modern GPU programming techniques.

Topics:

- Compute shaders
- GPU particle systems
- GPU frustum / occlusion culling
- Indirect rendering (MultiDrawIndirect)
- GPU-driven pipelines

Deliverables:

- GPU particle simulation demo
- Performance comparison experiments

---

## Phase 8 — Ray Tracing Experiments

Goal: explore ray-based rendering techniques.

Topics:

- CPU ray tracing reference renderer
- BVH acceleration structures
- Path tracing with importance sampling
- Hybrid rasterization + ray tracing

Deliverables:

- Basic path tracer
- Ray tracing visualization tools

---

## Phase 9 — Advanced Research Topics

Future exploration topics may include:

- Volumetric rendering (fog, clouds, god rays)
- Global illumination methods (RSM, LPV, voxel cone tracing)
- Neural rendering experiments
- Hybrid real-time / offline techniques
- Virtual shadow maps
- Mesh shaders

These areas are exploratory and may evolve over time.
