# Shader Pipeline: Vulkan Migration Guide (R3a → R3b/R5)

This document describes what is needed to evolve the current OpenGL-semantics SPIR-V pipeline (R3a)
into a Vulkan-ready resource model (R3b/R5). It is a design reference, not a commitment to implement immediately.

---

## 1. Current State After R3a

- **Shader format**: GLSL source → SPIR-V (compiled with `glslang -G --auto-map-locations --auto-map-bindings`) → runtime transpile back to GLSL 460 via SPIRV-Cross
- **Uniform model**: bare `uniform` declarations (no UBOs, no descriptor sets, no push constants)
- **Setter API**: per-uniform name-based setters on `IShader` (`SetMat4("u_Model", ...)`, `SetFloat3(...)`, etc.)
- **Binding**: samplers bound by slot index, uniforms located by `glGetUniformLocation`
- **SPIRV-Cross options**: `vulkan_semantics = false`, binding/location decorations stripped from plain uniforms during transpilation

This model works for OpenGL but is fundamentally incompatible with Vulkan's explicit resource binding model.

---

## 2. What Vulkan Requires

### 2.1 Uniform Blocks (UBOs)

All non-opaque uniforms **must** be in explicit `uniform` blocks with `set` and `binding` qualifiers:

```glsl
// NOT valid in Vulkan GLSL:
uniform mat4 u_ViewProjection;

// Valid:
layout(set = 0, binding = 0) uniform PerFrameUBO {
    mat4 ViewProjection;
    vec3 CameraPosition;
    // ...
} u_PerFrame;
```

### 2.2 Sampler/Texture Bindings

Samplers and textures need explicit `layout(set=N, binding=M)` qualifiers:

```glsl
layout(set = 1, binding = 0) uniform sampler2D u_AlbedoMap;
layout(set = 1, binding = 1) uniform sampler2D u_ShadowMap;
```

### 2.3 Push Constants

Small per-draw data (model matrix, etc.) should use push constants for performance:

```glsl
layout(push_constant) uniform PushConstants {
    mat4 Model;
    mat3 NormalMatrix;
} u_Push;
```

### 2.4 Compilation Flag

Change from `-G` (OpenGL semantics) to `-V` (Vulkan semantics). This enforces:
- All non-opaque uniforms must be in blocks
- Explicit `set` and `binding` required
- No implicit uniform locations

---

## 3. Per-Shader Rewrite Sketches

### 3.1 ForwardLit

**Current bare uniforms → Vulkan resource layout:**

```glsl
// Per-frame data (updated once per frame, shared across draws)
layout(set = 0, binding = 0) uniform PerFrameUBO {
    mat4 ViewProjection;
    mat4 LightViewProjection;
    vec3 CameraPosition;
    vec3 LightDirection;
    vec3 LightColor;
    float LightIntensity;
} u_PerFrame;

// Per-draw data (updated per draw call)
layout(push_constant) uniform PushConstants {
    mat4 Model;
    mat3 NormalMatrix;  // Note: mat3 has alignment implications in std140
} u_Push;

// Material data (per-material UBO or push constant range)
layout(set = 1, binding = 0) uniform MaterialUBO {
    vec4 Albedo;
    float Metallic;
    float Roughness;
    bool UseAlbedoMap;
    bool UseNormalMap;
} u_Material;

// Textures
layout(set = 1, binding = 1) uniform sampler2D u_AlbedoMap;
layout(set = 1, binding = 2) uniform sampler2D u_NormalMap;
layout(set = 2, binding = 0) uniform sampler2D u_ShadowMap;
```

**Key changes in shader body**: Replace all `u_ViewProjection` → `u_PerFrame.ViewProjection`, `u_Model` → `u_Push.Model`, etc.

### 3.2 ShadowDepth

```glsl
// Per-pass
layout(set = 0, binding = 0) uniform PerPassUBO {
    mat4 LightViewProjection;
} u_PerPass;

// Per-draw
layout(push_constant) uniform PushConstants {
    mat4 Model;
} u_Push;
```

Fragment shader remains empty (depth-only pass).

### 3.3 TexturePreview

```glsl
layout(set = 0, binding = 0) uniform PerPassUBO {
    bool IsDepthTexture;
} u_PerPass;

layout(set = 0, binding = 1) uniform sampler2D u_Texture;
```

No push constants needed (fullscreen quad, no per-draw variation).

---

## 4. IShader Interface Evolution

The current `IShader` interface uses per-uniform name-based setters:

```cpp
virtual void SetMat4(const std::string& name, const glm::mat4& value) = 0;
virtual void SetFloat3(const std::string& name, const glm::vec3& value) = 0;
// ... etc.
```

This model does not map to Vulkan, where resources are bound by descriptor set/binding index, not by name.

### Possible directions (not mutually exclusive):

1. **UBO buffer upload**: Pass pre-packed buffers to descriptor set bindings
   ```cpp
   virtual void BindUniformBuffer(uint32_t set, uint32_t binding, const Ref<IBuffer>& buffer) = 0;
   ```

2. **Push constants**: Upload small per-draw data directly
   ```cpp
   virtual void PushConstants(ShaderStage stage, uint32_t offset, uint32_t size, const void* data) = 0;
   ```

3. **Resource binding**: Bind textures/samplers by set+binding
   ```cpp
   virtual void BindTexture(uint32_t set, uint32_t binding, const Ref<ITexture2D>& texture) = 0;
   ```

4. **Reflection-driven material system**: Use SPIR-V reflection to auto-extract binding layouts, then materials pack their properties into matching buffer layouts automatically.

The exact API shape is TBD — it could be material buffers + pass buffers, push constants, reflected resource tables, or a combination. The key constraint is that the interface must not assume any specific backend's binding model (e.g., don't expose Vulkan descriptor sets directly in the abstract interface).

---

## 5. Material System Impact

`Material::UploadToShader` currently calls individual setters:

```cpp
void Material::UploadToShader(const Ref<IShader>& shader) {
    shader->SetFloat4("u_Albedo", m_Albedo);
    shader->SetFloat("u_Metallic", m_Metallic);
    // ...
}
```

In Vulkan, materials would need to:
1. Pack properties into a `MaterialUBO` struct matching the shader layout
2. Upload the entire buffer to the appropriate descriptor set binding
3. Bind texture resources to their descriptor set slots

This could be driven by SPIR-V reflection: at shader load time, inspect the material UBO layout and build a property-to-offset mapping. Materials then fill a byte buffer according to this mapping.

---

## 6. Reflection-Driven Binding

SPIRV-Cross (already vendored) can extract resource metadata from SPIR-V:

```cpp
auto resources = compiler.get_shader_resources();
for (auto& ubo : resources.uniform_buffers) {
    uint32_t set = compiler.get_decoration(ubo.id, spv::DecorationDescriptorSet);
    uint32_t binding = compiler.get_decoration(ubo.id, spv::DecorationBinding);
    // Extract member types, offsets, sizes...
}
```

This enables:
- Auto-generating descriptor set layouts from shader metadata
- Validating material property packs against shader expectations
- Reducing manual bookkeeping when shaders change

Enable `SPIRV_CROSS_ENABLE_REFLECT` in `vendor/CMakeLists.txt` when this is needed.

---

## 7. Recommended Migration Order

1. **Rewrite shaders** to use UBOs, push constants, and explicit bindings (keep `-G` flag initially to verify they still work on OpenGL via transpilation)
2. **Evolve `IShader` interface** to support buffer/resource binding alongside (or replacing) name-based setters
3. **Update `Material` system** to pack properties into UBO-compatible buffers
4. **Update render passes** to use new binding API (per-frame UBO, per-draw push constants)
5. **Switch compilation flag** from `-G` to `-V` once all shaders use Vulkan-compatible resource declarations
6. **Implement Vulkan backend** (R5) using the now-Vulkan-compatible SPIR-V directly (no transpilation needed)

### OpenGL compatibility during migration

During steps 1-4, shaders can use Vulkan-style declarations while still targeting OpenGL via SPIRV-Cross transpilation. SPIRV-Cross maps UBOs to `uniform` blocks and push constants to plain uniforms in the GLSL output. This allows incremental migration without breaking the existing OpenGL backend.

---

## 8. std140 Layout Considerations

Vulkan uses std140 (or std430) layout rules for UBOs. Key gotchas:

- `vec3` is aligned to 16 bytes (same as `vec4`) — wastes 4 bytes per `vec3`
- `mat3` occupies 3 × `vec4` columns (48 bytes, not 36) due to column alignment
- Arrays of scalars/vectors have per-element alignment of 16 bytes
- `bool` maps to 4-byte `int` in std140

The `NormalMatrix` (`mat3`) in ForwardLit's push constants needs special attention — it may need to be passed as 3 × `vec4` or replaced with a `mat4` to avoid alignment issues.

---

## References

- [Vulkan Specification — Descriptor Sets](https://registry.khronos.org/vulkan/specs/1.3-extensions/html/chap14.html)
- [SPIRV-Cross README](https://github.com/KhronosGroup/SPIRV-Cross)
- [glslang README — OpenGL vs Vulkan semantics](https://github.com/KhronosGroup/glslang)
- [std140 Layout Rules](https://www.khronos.org/opengl/wiki/Interface_Block_(GLSL)#Memory_layout)
