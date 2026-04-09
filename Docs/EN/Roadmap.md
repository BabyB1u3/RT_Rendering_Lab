# Roadmap

This document outlines the long-term development plan for the Real Time Rendering Lab.

The project is currently in a major architectural refactor. The render system is being redesigned from scratch around a modern explicit RHI and Slang shaders. The phases below reflect the new trajectory.

[中文路线图](../ZH_CN/roadmap.zh-CN.md)

---

## Phase 1 — Core Engine Foundation ✅

Goal: establish a stable, backend-agnostic engine foundation that all future rendering work builds on.

- [x] Application runtime — main loop, `Layer`, `LayerStack`, hot-switchable layers
- [x] Event system — `EventBus`, `ScopedConnection`, decoupled publisher/subscriber model
- [x] Input system — `InputAction` binding, keyboard/mouse codes, input replay
- [x] Resource system — logical path resolution, multi-mount (Project / Engine / Plugins), asset catalog, cook pipeline (`.rtrtex`, `.rtrpak`), overlay support
- [x] Diagnostics — structured logging (spdlog), assertion framework, crash handler
- [x] Serialization — engine-side serialization utilities
- [x] Scene system — `Camera`, `DebugCameraController`, `Light`, `Transform`, `SceneData`
- [x] GUI — `ImGuiLayer`, debug panels, demo selector; Metal backend wired
- [x] Test infrastructure — Google Test, unit and contract test suites

---

## Phase 2 — RenderSystem: Multi-Backend RHI 🔄

Goal: design and implement a modern explicit RHI with Slang shaders and at least one production-quality backend. This is the current focus of the project.

Design documents: [Docs/Modules/RenderSystem/Design/](../Modules/RenderSystem/Design/RHI.md)

**Shader system**
- [ ] Slang integration — compiler setup, module cache, per-backend compilation
- [ ] Reflection-driven parameter layout — neutral layout model derived from Slang reflection
- [ ] Parameter writer — type-safe C++ → shader parameter authoring
- [ ] Shader hot reload — module-level recompilation without full pipeline recreation

**RHI layer**
- [ ] Core resource types — `Buffer`, `Texture`, `Sampler`, `ShaderProgram`, `GraphicsPipeline`, `ComputePipeline`
- [ ] Pipeline layout — `PipelineLayout`, `ResourceSet`, reflection-driven binding
- [ ] Command recording — `CommandList`, draw calls, resource barriers
- [ ] Swap chain and frame presentation

**Backends**
- [ ] Metal backend — direct slot binding (v1), argument buffer migration path (v2)
- [ ] Vulkan backend — descriptor sets, render passes, synchronization
- [ ] OpenGL compatibility backend — state translation layer, SPIR-V path via Slang

**First render demo**
- [ ] Forward rendering pass in the new RHI
- [ ] Basic directional lighting
- [ ] ImGui integration with new backend

---

## Phase 3 — Basic Real-Time Rendering

Goal: implement fundamental rendering techniques on top of the new RHI.

- [ ] Shadow mapping — depth pass, PCF soft shadows, bias strategies
- [ ] Multiple light types — point lights, spot lights
- [ ] Normal mapping
- [ ] Model loading — OBJ / glTF
- [ ] Skybox / environment map
- [ ] Material parameter UI — per-demo ImGui controls

Deliverables:

- Lighting and shadow demos
- Importable scene with textured models

---

## Phase 4 — Modern Rendering Pipeline

Goal: transition to more advanced rendering pipelines.

- [ ] Deferred rendering — G-buffer architecture
- [ ] HDR rendering and tone mapping (Reinhard, ACES)
- [ ] Bloom — threshold + Gaussian blur + composite
- [ ] Gamma correction pipeline
- [ ] Anti-aliasing — MSAA, FXAA

Deliverables:

- Deferred renderer demo
- Forward vs. deferred comparison
- HDR / bloom demo

---

## Phase 5 — Physically Based Rendering

Goal: implement physically based shading.

- [ ] Cook-Torrance BRDF — GGX / Schlick / Smith
- [ ] Metallic / roughness workflow
- [ ] Image-based lighting (IBL)
- [ ] Environment map pre-filtering
- [ ] BRDF integration lookup tables
- [ ] PBR material editor

Deliverables:

- PBR material sphere demo
- HDR environment lighting

---

## Phase 6 — Screen Space Effects

Goal: enhance visual realism using screen-space techniques.

- [ ] Screen-space ambient occlusion (SSAO)
- [ ] Screen-space reflections (SSR)
- [ ] Motion blur
- [ ] Depth of field
- [ ] Screen-space global illumination (experimental)

Deliverables:

- Visual comparison demos with on/off toggles
- Parameter exploration tools

---

## Phase 7 — Procedural Geometry

Goal: explore algorithmic content generation.

- [ ] Noise-based terrain — Perlin, simplex
- [ ] Procedural landscapes with LOD
- [ ] Voxel terrain experiments
- [ ] Chunk-based world representation
- [ ] Marching cubes / dual contouring

Deliverables:

- Procedural terrain demos
- Interactive real-time parameter editing

---

## Phase 8 — GPU-Driven Rendering

Goal: leverage modern GPU programming techniques.

- [ ] Compute shaders
- [ ] GPU particle systems
- [ ] GPU frustum / occlusion culling
- [ ] Indirect rendering (MultiDrawIndirect)
- [ ] GPU-driven pipelines

Deliverables:

- GPU particle simulation demo
- Performance comparison experiments

---

## Phase 9 — Ray Tracing Experiments

Goal: explore ray-based rendering techniques.

- [ ] CPU ray tracing reference renderer
- [ ] BVH acceleration structures
- [ ] Path tracing with importance sampling
- [ ] Hybrid rasterization + ray tracing

Deliverables:

- Basic path tracer
- Ray tracing visualization tools

---

## Phase 10 — Advanced Research Topics

Future exploration topics, likely to evolve over time:

- Volumetric rendering (fog, clouds, god rays)
- Global illumination (RSM, LPV, voxel cone tracing)
- Virtual shadow maps
- Mesh shaders
- Neural rendering experiments
- Hybrid real-time / offline techniques
