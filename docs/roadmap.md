# Roadmap

This document outlines the long-term development plan for the graphics playground.

The goal of the project is to gradually explore modern graphics techniques through independent demos.

The roadmap is divided into phases that represent increasing complexity of rendering techniques.

---

# Phase 1 — Framework Foundation

Goal: establish a stable environment for graphics experiments.

Tasks:

- application runtime
- window and input system
- basic rendering loop
- shader management
- texture loading
- resource management
- framebuffer abstraction
- camera system

Deliverables:

- a minimal framework capable of running multiple demos
- clear project structure
- documentation for core components

---

# Phase 2 — Basic Real-Time Rendering

Goal: implement fundamental rendering techniques.

Topics may include:

- forward rendering
- multiple light sources
- material parameters
- skybox rendering
- shadow mapping

Deliverables:

- several lighting demos
- basic material system
- visual comparison screenshots

---

# Phase 3 — Modern Rendering Pipeline

Goal: transition to more advanced rendering pipelines.

Possible techniques:

- deferred rendering
- G-buffer architecture
- HDR rendering
- tone mapping
- bloom

Deliverables:

- deferred renderer demo
- comparison between forward and deferred pipelines

---

# Phase 4 — Physically Based Rendering

Goal: implement physically based shading.

Topics:

- Cook-Torrance BRDF
- metallic / roughness workflow
- image based lighting
- environment map filtering
- BRDF lookup tables

Deliverables:

- PBR material demo
- HDR environment lighting

---

# Phase 5 — Screen Space Effects

Goal: enhance visual realism using screen-space techniques.

Possible experiments:

- SSAO
- screen-space reflections
- screen-space global illumination (experimental)
- motion blur
- depth of field

Deliverables:

- visual comparison demos
- parameter exploration tools

---

# Phase 6 — Procedural Geometry

Goal: explore algorithmic content generation.

Topics may include:

- noise-based terrain generation
- procedural landscapes
- voxel terrain experiments
- chunk-based world representation

Deliverables:

- procedural terrain demos
- interactive terrain visualization

---

# Phase 7 — GPU Techniques

Goal: leverage modern GPU programming techniques.

Potential areas:

- compute shaders
- GPU particle systems
- GPU culling
- indirect rendering
- GPU-driven pipelines

Deliverables:

- GPU particle simulation demo
- performance experiments

---

# Phase 8 — Ray Tracing Experiments

Goal: explore ray-based rendering techniques.

Possible experiments:

- CPU ray tracing
- BVH acceleration structures
- path tracing
- hybrid rasterization + ray tracing

Deliverables:

- basic path tracer
- ray tracing visualization tools

---

# Phase 9 — Advanced Research Topics

Future exploration topics may include:

- volumetric rendering
- global illumination methods
- neural rendering experiments
- hybrid real-time / offline techniques

These areas are exploratory and may evolve over time.