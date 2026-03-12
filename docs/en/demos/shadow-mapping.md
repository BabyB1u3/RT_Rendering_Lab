# Shadow Mapping Demo

The first demo in the RT Rendering Lab — a directional light shadow mapping implementation
with Blinn-Phong shading and PCF soft shadows.

[中文文档](../../zh-CN//demos/shadow-mapping.zh-CN.md)

---

## Overview

This demo renders a simple scene (ground plane + cubes) lit by a single directional light,
with real-time shadow mapping. It demonstrates the complete forward rendering pipeline:
depth pass from the light's point of view, followed by a forward lighting pass that samples
the shadow map, plus a debug visualization mode.

### Rendering Pipeline

```
SceneRenderer::Render(scene)
├── BuildDirectionalLightViewProjection()   → Build light-space VP matrix
├── ShadowPass::Execute(scene, lightVP)     → Render 2048x2048 depth map (front face culling)
├── ForwardPass::Execute(scene, shadowMap)   → Forward shading + PCF shadow sampling
└── TexturePreviewPass::Execute(texture)     → Output final image or shadow map preview
```

### Lighting Model

**Blinn-Phong** with three components:

| Component | Formula | Description |
|-----------|---------|-------------|
| Ambient   | `0.15 * albedo` | Constant base illumination |
| Diffuse   | `(1 - shadow) * NdotL * albedo * lightColor * intensity` | Lambertian diffuse |
| Specular  | `(1 - shadow) * pow(dot(N, H), 32) * lightColor * intensity` | Blinn-Phong highlight |

---

## Shadow Mapping Technique

### Depth Pass (ShadowPass)

1. Build an orthographic projection from the directional light's perspective
2. Render all scene geometry into a 2048x2048 depth-only framebuffer
3. **Front face culling** is enabled during this pass to reduce self-shadowing artifacts (shadow acne)

### Shadow Sampling (ForwardPass)

1. Transform each fragment into light space using the light VP matrix
2. Compare the fragment's depth against the shadow map
3. Apply a **slope-scaled bias**: `max(0.0001, 0.002 * (1 - NdotL))` to prevent acne
4. Use **3x3 PCF** (Percentage Closer Filtering) for soft shadow edges:

```glsl
float shadow = 0.0;
vec2 texelSize = 1.0 / textureSize(u_ShadowMap, 0);
for (int x = -1; x <= 1; ++x)
    for (int y = -1; y <= 1; ++y)
        shadow += currentDepth - bias > texture(u_ShadowMap, uv + vec2(x,y) * texelSize).r ? 1.0 : 0.0;
shadow /= 9.0;
```

### Debug Visualization

Press **2** to switch to shadow map preview mode (depth buffer visualized as grayscale).
Press **1** to return to the final rendered output.

---

## Controls

| Input | Action |
|-------|--------|
| W / A / S / D | Move camera forward / left / backward / right |
| Q / E | Move camera down / up |
| Mouse | Look around (always active) |
| Scroll wheel | Adjust FOV |
| 1 | Show final color output |
| 2 | Show shadow map debug view |

---

## Scene Setup

- **Ground plane**: 10x10 at Y=0
- **3 cubes**: different positions, scales, and rotations
- **Directional light**: direction `(-0.8, -1.0, -0.4)`, white, intensity 1.0
- **Camera**: starts at `(0, 2.5, 6)` looking toward the origin

---

## File Structure

```
src/demos/ShadowMapping/
├── ShadowMapping.h           # Demo class declaration
└── ShadowMapping.cpp         # Scene setup, input handling, render orchestration

src/renderer/
├── SceneRenderer.h/cpp       # Multi-pass render orchestration
└── passes/
    ├── ShadowPass.h/cpp      # Depth-only pass from light POV
    ├── ForwardPass.h/cpp     # Forward shading with shadow sampling
    └── TexturePreviewPass.h/cpp  # Fullscreen texture visualization

assets/shaders/
├── ShadowDepth.glsl          # Minimal vertex-only shader for depth writing
├── ForwardLit.glsl           # Blinn-Phong + PCF shadow sampling
└── TexturePreview.glsl       # Fullscreen quad for texture/depth visualization
```

---

## Key Implementation Details

| Aspect | Detail |
|--------|--------|
| Shadow map resolution | 2048 x 2048 |
| Shadow projection | Orthographic, size 10, near 0.1, far 30 |
| Bias strategy | Slope-scaled: `max(0.0001, 0.002 * (1 - NdotL))` |
| PCF kernel | 3x3 (9 samples) |
| Shadow pass culling | Front face culling (reduces acne without large bias) |
| Normal matrix | Precomputed on CPU (`transpose(inverse(mat3(model)))`) |
| Texture slot binding | `enum class TextureSlot` — `ShadowMap = 0`, `Albedo = 1` |
| Fallback shadow map | 1x1 white texture (depth = 1.0 → no shadow when shadow pass is skipped) |
