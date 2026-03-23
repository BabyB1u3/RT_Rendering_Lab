# Slang Migration — Design Document

This document describes the migration from the current GLSL + glslang + SPIRV-Cross shader toolchain
to Slang as the single source language with direct multi-backend output. It serves as both the
migration plan and a reference for the target architecture.

---

## 1. Motivation

### 1.1 Current Pain Points

The existing pipeline (GLSL → glslang → SPIR-V → SPIRV-Cross → GLSL 460) works for the OpenGL
backend but has fundamental issues for multi-backend support:

- **Bare uniforms** (`uniform mat4 u_Model`) are incompatible with Vulkan and Metal resource models.
  Migrating GLSL shaders to use UBOs, push constants, and explicit `set`/`binding` qualifiers is a
  manual process documented in `shader-vulkan-migration.md`.
- **Two-tool pipeline**: glslang compiles at build time, SPIRV-Cross transpiles at runtime. Each tool
  has its own configuration, quirks, and version constraints.
- **Workarounds in transpilation**: `GLShader::TranspileSpirvToGlsl()` must strip binding/location
  decorations from plain uniforms because glslang's `--auto-map-bindings` assigns them indiscriminately.
- **No module/include system**: GLSL lacks proper imports. As shaders grow (PBR, deferred, post-process),
  code duplication across shader files becomes unavoidable.
- **No variant management**: Adding `#ifdef` permutations to GLSL requires manual `glslang -D` invocations
  per variant. No language-level support for generic/interface-based specialization.

### 1.2 Why Slang

Slang is an open-source shading language under Khronos Group governance (since November 2024),
originally developed at NVIDIA. Key reasons for adopting it:

- **Direct multi-backend output**: A single `slangc` invocation can emit GLSL, SPIR-V, MSL, HLSL,
  WGSL, and CUDA. This replaces both glslang and SPIRV-Cross with a single tool.
- **Explicit resource model by default**: As an HLSL superset, Slang requires structured buffer
  declarations (`cbuffer`, `ParameterBlock`, `StructuredBuffer`). The UBO/push-constant migration
  described in `shader-vulkan-migration.md` becomes unnecessary — it is just how Slang works.
- **Module system**: `import` / `module` with proper namespaces, visibility control, and separate
  compilation. Replaces fragile `#include` with real dependency tracking.
- **Generics + interfaces**: Enables type-safe shader specialization at compile time, replacing
  `#ifdef` permutation explosion with zero-overhead polymorphism.
- **Production proven**: Valve shipped CS2 and Dota 2 with Slang-generated SPIR-V via Source 2 engine.
  id Software, Autodesk, and Adobe are also participating in the Slang ecosystem.
- **HLSL compatibility**: Existing HLSL knowledge transfers directly. Slang is a strict superset of
  HLSL — all HLSL code compiles as Slang with minimal or no changes.
- **Reflection API**: Slang's reflection provides complete type information (struct layouts, binding
  indices, sizes, offsets) without requiring a separate SPIRV-Cross reflection pass.

---

## 2. Current State

### 2.1 Toolchain

```
GLSL (.vert/.frag)
    │
    ▼  build-time: glslang -G --auto-map-locations --auto-map-bindings
SPIR-V (.vert.spv/.frag.spv)
    │
    ▼  runtime: SPIRV-Cross (vulkan_semantics=false, strip plain uniform bindings)
GLSL 460 source string
    │
    ▼  runtime: glCompileShader / glLinkProgram
GL Program
```

### 2.2 Files Involved

| Component | Files |
|-----------|-------|
| Shader sources | `assets/shaders/{ForwardLit,ShadowDepth,TexturePreview}.{vert,frag}` (6 files) |
| Build system | `cmake/CompileShaders.cmake` (glslang custom commands) |
| Vendor deps | `vendor/glslang/`, `vendor/spirv-cross/`, `vendor/CMakeLists.txt` |
| Runtime transpile | `src/graphics/opengl/GLShader.cpp` → `TranspileSpirvToGlsl()` |
| Shader loading | `GLShader::CreateFromStem()`, `IGraphicsDevice::CreateShaderFromStem()` |
| Asset resolution | `FileSystem::GetShaderStem()` → returns `assets/shaders/{name}` (no extension) |

### 2.3 Vendor Configuration

```cmake
# glslang
set(ENABLE_HLSL OFF)
set(ENABLE_OPT OFF)
add_subdirectory(glslang)
set(GLSLANG_TARGET "glslang-standalone")

# SPIRV-Cross
set(SPIRV_CROSS_ENABLE_MSL OFF)
set(SPIRV_CROSS_ENABLE_HLSL OFF)
set(SPIRV_CROSS_ENABLE_REFLECT OFF)
# Links: spirv-cross-core, spirv-cross-glsl
```

### 2.4 IShader Interface

Name-based uniform setters, incompatible with Vulkan/Metal:

```cpp
class IShader {
    virtual void SetMat4(const std::string& name, const glm::mat4& value) = 0;
    virtual void SetFloat3(const std::string& name, const glm::vec3& value) = 0;
    // ... 10 setter methods total
};
```

---

## 3. Target State

### 3.1 Toolchain

```
Slang (.slang)
    │
    ▼  build-time: slangc
    ├──────────────────────────────────────┐
    │                                      │
    ▼ -target glsl -profile glsl_460       ▼ -target spirv
GLSL 460 source                       SPIR-V binary
    │                                      │
    ▼ runtime: glCompileShader             ▼ future: vkCreateShaderModule
GL Program                            VkPipeline

    ├──────────────────────────────────────┐
    ▼ -target metal                        ▼ -target wgsl (future)
MSL source                            WGSL source
    │
    ▼ future: MTLLibrary
Metal Pipeline
```

Key change: **slangc replaces both glslang and SPIRV-Cross**. The runtime no longer does any
transpilation — it receives ready-to-use source or binary for the target backend.

### 3.2 Output Artifacts

Build-time compilation produces per-backend output files:

```
assets/shaders/
    ForwardLit.slang          ← source (checked into repo)
    ShadowDepth.slang
    TexturePreview.slang
    modules/                  ← shared Slang modules
        lighting.slang
        shadow.slang

build/shaders/                ← generated (build directory, not in repo)
    glsl/
        ForwardLit.vert.glsl
        ForwardLit.frag.glsl
        ...
    spirv/
        ForwardLit.vert.spv
        ForwardLit.frag.spv
        ...
    metal/
        ForwardLit.metal
        ...
```

### 3.3 Shader Loading (Revised)

`FileSystem::GetShaderPath()` returns the backend-specific compiled artifact:

```cpp
// FileSystem
static std::filesystem::path GetShaderPath(std::string_view shaderName, ShaderStage stage);
// → build/shaders/glsl/ForwardLit.vert.glsl   (OpenGL)
// → build/shaders/spirv/ForwardLit.vert.spv   (Vulkan)
// → build/shaders/metal/ForwardLit.metal       (Metal)
```

`IGraphicsDevice::CreateShader()` replaces `CreateShaderFromStem()`:

```cpp
// IGraphicsDevice — proposed
virtual Ref<IShader> CreateShader(const std::string& name) = 0;
// Backend resolves the correct artifact path internally
```

### 3.4 Vendor Dependencies After Migration

| Dependency | Status | Reason |
|-----------|--------|--------|
| **Slang** | **ADD** | Shader compiler (slangc CLI for build-time, optionally slang.h for runtime reflection) |
| glslang | **REMOVE** | Replaced by slangc |
| SPIRV-Cross | **REMOVE** | Replaced by slangc direct GLSL/MSL output |

---

## 4. Shader Rewrites

### 4.1 ForwardLit

**Current GLSL** (vertex + fragment, bare uniforms):

```glsl
// ForwardLit.vert
uniform mat4 u_ViewProjection;
uniform mat4 u_Model;
uniform mat3 u_NormalMatrix;
uniform mat4 u_LightViewProjection;
```

```glsl
// ForwardLit.frag
uniform sampler2D u_ShadowMap;
uniform sampler2D u_AlbedoMap;
uniform bool u_UseAlbedoMap;
uniform vec3 u_Albedo;
uniform float u_SpecularPower;
uniform float u_AmbientStrength;
uniform vec3 u_CameraPosition;
uniform vec3 u_LightDirection;
uniform vec3 u_LightColor;
uniform float u_LightIntensity;
```

**Target Slang** (single file, structured resources):

```slang
// ForwardLit.slang

import shadow;  // ComputeShadow() from shared module

// Per-frame data (bound once per frame, shared across all draws)
struct PerFrameData
{
    float4x4 viewProjection;
    float4x4 lightViewProjection;
    float3   cameraPosition;
    float3   lightDirection;
    float3   lightColor;
    float    lightIntensity;
    float    ambientStrength;
};
ParameterBlock<PerFrameData> gPerFrame;

// Per-draw data (updated per draw call)
struct PerDrawData
{
    float4x4 model;
    float3x3 normalMatrix;
};
ParameterBlock<PerDrawData> gPerDraw;

// Material data
struct MaterialData
{
    float3 albedo;
    float  specularPower;
    bool   useAlbedoMap;
};
ParameterBlock<MaterialData> gMaterial;

// Textures
Texture2D    gAlbedoMap;
SamplerState gAlbedoSampler;
Texture2D    gShadowMap;
SamplerState gShadowSampler;

// Vertex input / output
struct VertexInput
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 texCoord : TEXCOORD0;
};

struct VertexOutput
{
    float4 svPosition        : SV_Position;
    float3 worldPosition     : WORLD_POS;
    float3 worldNormal       : WORLD_NORMAL;
    float2 texCoord          : TEXCOORD0;
    float4 lightSpacePosition: LIGHT_SPACE_POS;
};

[shader("vertex")]
VertexOutput vertexMain(VertexInput input)
{
    VertexOutput output;

    float4 worldPos = mul(gPerDraw.model, float4(input.position, 1.0));

    output.worldPosition      = worldPos.xyz;
    output.worldNormal        = mul(gPerDraw.normalMatrix, input.normal);
    output.texCoord           = input.texCoord;
    output.lightSpacePosition = mul(gPerFrame.lightViewProjection, worldPos);
    output.svPosition         = mul(gPerFrame.viewProjection, worldPos);

    return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target
{
    float3 normal   = normalize(input.worldNormal);
    float3 lightDir = normalize(gPerFrame.lightDirection);

    // Albedo
    float3 albedo = gMaterial.albedo;
    if (gMaterial.useAlbedoMap)
        albedo *= gAlbedoMap.Sample(gAlbedoSampler, input.texCoord).rgb;

    // Shadow
    float shadow = ComputeShadow(
        gShadowMap, gShadowSampler,
        input.lightSpacePosition, normal, lightDir
    );

    // Diffuse
    float NdotL = max(dot(normal, -lightDir), 0.0);
    float3 ambient = gPerFrame.ambientStrength * albedo;
    float3 diffuse = (1.0 - shadow) * NdotL * albedo
                     * gPerFrame.lightColor * gPerFrame.lightIntensity;

    // Blinn-Phong specular
    float3 viewDir = normalize(gPerFrame.cameraPosition - input.worldPosition);
    float3 halfDir = normalize(viewDir + (-lightDir));
    float  spec    = pow(max(dot(normal, halfDir), 0.0), gMaterial.specularPower);
    float3 specular = (1.0 - shadow) * spec
                      * gPerFrame.lightColor * gPerFrame.lightIntensity;

    return float4(ambient + diffuse + specular, 1.0);
}
```

### 4.2 ShadowDepth

```slang
// ShadowDepth.slang

struct PerPassData
{
    float4x4 lightViewProjection;
};
ParameterBlock<PerPassData> gPerPass;

struct PerDrawData
{
    float4x4 model;
};
ParameterBlock<PerDrawData> gPerDraw;

struct VertexInput
{
    float3 position : POSITION;
};

[shader("vertex")]
float4 vertexMain(VertexInput input) : SV_Position
{
    return mul(gPerPass.lightViewProjection, mul(gPerDraw.model, float4(input.position, 1.0)));
}

[shader("fragment")]
void fragmentMain()
{
    // Depth-only pass — no color output
}
```

### 4.3 TexturePreview

```slang
// TexturePreview.slang

struct PerPassData
{
    bool isDepthTexture;
};
ParameterBlock<PerPassData> gPerPass;

Texture2D    gTexture;
SamplerState gSampler;

struct VertexInput
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 texCoord : TEXCOORD0;
};

struct VertexOutput
{
    float4 svPosition : SV_Position;
    float2 texCoord   : TEXCOORD0;
};

[shader("vertex")]
VertexOutput vertexMain(VertexInput input)
{
    VertexOutput output;
    output.texCoord   = input.texCoord;
    output.svPosition = float4(input.position.xy, 0.0, 1.0);
    return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target
{
    if (gPerPass.isDepthTexture)
    {
        float depth = gTexture.Sample(gSampler, input.texCoord).r;
        return float4(depth, depth, depth, 1.0);
    }
    return gTexture.Sample(gSampler, input.texCoord);
}
```

### 4.4 Shared Module: Shadow

```slang
// modules/shadow.slang
module shadow;

public float ComputeShadow(
    Texture2D    shadowMap,
    SamplerState shadowSampler,
    float4       lightSpacePosition,
    float3       normal,
    float3       lightDir)
{
    float3 projCoords = lightSpacePosition.xyz / lightSpacePosition.w;
    projCoords = projCoords * 0.5 + 0.5;

    if (projCoords.z > 1.0)
        return 0.0;

    float currentDepth = projCoords.z;
    float bias = max(0.0001, 0.002 * (1.0 - dot(normalize(normal), normalize(-lightDir))));

    // 3x3 PCF
    float shadow = 0.0;
    float2 texelSize;
    uint width, height;
    shadowMap.GetDimensions(width, height);
    texelSize = float2(1.0 / width, 1.0 / height);

    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            float pcfDepth = shadowMap.Sample(shadowSampler,
                projCoords.xy + float2(x, y) * texelSize).r;
            shadow += (currentDepth - bias > pcfDepth) ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;

    return shadow;
}
```

---

## 5. Build System Changes

### 5.1 Slang Integration in CMake

Replace `cmake/CompileShaders.cmake`:

```cmake
option(GLAB_COMPILE_SHADERS "Compile Slang shaders to backend-specific formats" ON)

# Find or build slangc
# Option A: System-installed slangc
# find_program(SLANGC slangc REQUIRED)
# Option B: Vendored slang (build from source or use prebuilt binary)
set(SLANGC "${CMAKE_SOURCE_DIR}/vendor/slang/bin/slangc" CACHE FILEPATH "Path to slangc compiler")

function(glab_compile_shaders)
    if(NOT GLAB_COMPILE_SHADERS)
        return()
    endif()

    set(SHADER_SOURCE_DIR "${CMAKE_SOURCE_DIR}/assets/shaders")
    set(SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/shaders")

    set(SHADER_MODULES
        modules/shadow.slang
    )

    set(SHADER_SOURCES
        ForwardLit.slang
        ShadowDepth.slang
        TexturePreview.slang
    )

    # Determine which targets to compile based on platform / config
    set(SHADER_TARGETS "")
    if(GLAB_BACKEND_OPENGL)
        list(APPEND SHADER_TARGETS glsl)
    endif()
    if(GLAB_BACKEND_VULKAN)
        list(APPEND SHADER_TARGETS spirv)
    endif()
    if(GLAB_BACKEND_METAL)
        list(APPEND SHADER_TARGETS metal)
    endif()

    set(ALL_OUTPUTS "")

    foreach(SHADER ${SHADER_SOURCES})
        get_filename_component(SHADER_NAME ${SHADER} NAME_WE)
        set(INPUT "${SHADER_SOURCE_DIR}/${SHADER}")

        # Collect module dependencies for -I flag
        set(MODULE_DEPS "")
        foreach(MOD ${SHADER_MODULES})
            list(APPEND MODULE_DEPS "${SHADER_SOURCE_DIR}/${MOD}")
        endforeach()

        foreach(TARGET_LANG ${SHADER_TARGETS})
            if(TARGET_LANG STREQUAL "glsl")
                # Emit separate vertex + fragment GLSL files
                set(VERT_OUTPUT "${SHADER_OUTPUT_DIR}/glsl/${SHADER_NAME}.vert.glsl")
                set(FRAG_OUTPUT "${SHADER_OUTPUT_DIR}/glsl/${SHADER_NAME}.frag.glsl")

                add_custom_command(
                    OUTPUT ${VERT_OUTPUT}
                    COMMAND ${SLANGC} ${INPUT}
                        -target glsl
                        -profile glsl_460
                        -stage vertex -entry vertexMain
                        -I "${SHADER_SOURCE_DIR}/modules"
                        -o ${VERT_OUTPUT}
                    DEPENDS ${INPUT} ${MODULE_DEPS}
                    COMMENT "Slang → GLSL vertex: ${SHADER_NAME}"
                    VERBATIM
                )
                add_custom_command(
                    OUTPUT ${FRAG_OUTPUT}
                    COMMAND ${SLANGC} ${INPUT}
                        -target glsl
                        -profile glsl_460
                        -stage fragment -entry fragmentMain
                        -I "${SHADER_SOURCE_DIR}/modules"
                        -o ${FRAG_OUTPUT}
                    DEPENDS ${INPUT} ${MODULE_DEPS}
                    COMMENT "Slang → GLSL fragment: ${SHADER_NAME}"
                    VERBATIM
                )
                list(APPEND ALL_OUTPUTS ${VERT_OUTPUT} ${FRAG_OUTPUT})

            elseif(TARGET_LANG STREQUAL "spirv")
                set(VERT_OUTPUT "${SHADER_OUTPUT_DIR}/spirv/${SHADER_NAME}.vert.spv")
                set(FRAG_OUTPUT "${SHADER_OUTPUT_DIR}/spirv/${SHADER_NAME}.frag.spv")

                add_custom_command(
                    OUTPUT ${VERT_OUTPUT}
                    COMMAND ${SLANGC} ${INPUT}
                        -target spirv
                        -emit-spirv-directly
                        -stage vertex -entry vertexMain
                        -I "${SHADER_SOURCE_DIR}/modules"
                        -o ${VERT_OUTPUT}
                    DEPENDS ${INPUT} ${MODULE_DEPS}
                    COMMENT "Slang → SPIR-V vertex: ${SHADER_NAME}"
                    VERBATIM
                )
                add_custom_command(
                    OUTPUT ${FRAG_OUTPUT}
                    COMMAND ${SLANGC} ${INPUT}
                        -target spirv
                        -emit-spirv-directly
                        -stage fragment -entry fragmentMain
                        -I "${SHADER_SOURCE_DIR}/modules"
                        -o ${FRAG_OUTPUT}
                    DEPENDS ${INPUT} ${MODULE_DEPS}
                    COMMENT "Slang → SPIR-V fragment: ${SHADER_NAME}"
                    VERBATIM
                )
                list(APPEND ALL_OUTPUTS ${VERT_OUTPUT} ${FRAG_OUTPUT})

            elseif(TARGET_LANG STREQUAL "metal")
                set(MTL_OUTPUT "${SHADER_OUTPUT_DIR}/metal/${SHADER_NAME}.metal")

                add_custom_command(
                    OUTPUT ${MTL_OUTPUT}
                    COMMAND ${SLANGC} ${INPUT}
                        -target metal
                        -stage vertex -entry vertexMain
                        -stage fragment -entry fragmentMain
                        -I "${SHADER_SOURCE_DIR}/modules"
                        -o ${MTL_OUTPUT}
                    DEPENDS ${INPUT} ${MODULE_DEPS}
                    COMMENT "Slang → Metal: ${SHADER_NAME}"
                    VERBATIM
                )
                list(APPEND ALL_OUTPUTS ${MTL_OUTPUT})
            endif()
        endforeach()
    endforeach()

    add_custom_target(CompileShaders ALL DEPENDS ${ALL_OUTPUTS})
endfunction()
```

### 5.2 Vendor Dependency Changes

```cmake
# vendor/CMakeLists.txt — REMOVE these sections:

# glslang          ← REMOVE entirely
# SPIRV-Cross      ← REMOVE entirely

# ADD:
# Slang — either vendored prebuilt or built from source
# See Section 6 for integration options
```

### 5.3 Link Target Changes

```cmake
# src/CMakeLists.txt — REMOVE:
target_link_libraries(RTRLabCore PRIVATE spirv-cross-core spirv-cross-glsl)

# No new link target needed if using slangc CLI only (build-time tool).
# If using Slang runtime API for reflection, add:
# target_link_libraries(RTRLabCore PRIVATE slang)
```

---

## 6. Slang Vendor Integration Options

### Option A: Prebuilt Binary (Recommended for Initial Migration)

Download `slangc` from Slang GitHub releases. Place in `vendor/slang/bin/`.
Only the CLI tool is needed for build-time compilation.

**Pros**: Simplest setup, no build overhead, works immediately.
**Cons**: Binary checked into repo or downloaded at configure time.

```cmake
# cmake/FindSlang.cmake
if(WIN32)
    set(SLANGC_DEFAULT "${CMAKE_SOURCE_DIR}/vendor/slang/bin/windows-x64/slangc.exe")
elseif(APPLE)
    set(SLANGC_DEFAULT "${CMAKE_SOURCE_DIR}/vendor/slang/bin/macos-arm64/slangc")
else()
    set(SLANGC_DEFAULT "${CMAKE_SOURCE_DIR}/vendor/slang/bin/linux-x64/slangc")
endif()

set(SLANGC "${SLANGC_DEFAULT}" CACHE FILEPATH "Path to slangc compiler")

if(NOT EXISTS "${SLANGC}")
    message(FATAL_ERROR
        "slangc not found at ${SLANGC}.\n"
        "Download from https://github.com/shader-slang/slang/releases\n"
        "and place in vendor/slang/bin/")
endif()
```

### Option B: Git Submodule (Full Build)

Add Slang as a git submodule and build from source. Heavier but fully reproducible.

### Option C: FetchContent / CPM

Download at CMake configure time. Good middle ground for CI.

**Recommendation**: Start with **Option A** for speed, migrate to B or C once the pipeline is validated.

---

## 7. Runtime Changes

### 7.1 GLShader — Remove SPIRV-Cross Transpilation

The entire `TranspileSpirvToGlsl()` function and SPIRV-Cross dependency are removed.
`CreateFromStem()` is replaced with `CreateFromCompiledGlsl()`:

```cpp
// GLShader — after migration (simplified)
Ref<GLShader> GLShader::CreateFromCompiledGlsl(const std::string& name)
{
    auto basePath = FileSystem::GetShaderOutputDir() / "glsl";
    auto vertPath = basePath / (name + ".vert.glsl");
    auto fragPath = basePath / (name + ".frag.glsl");

    std::string vertSrc = FileSystem::ReadTextFile(vertPath);
    std::string fragSrc = FileSystem::ReadTextFile(fragPath);

    // Optional: geometry shader
    std::string geomSrc;
    auto geomPath = basePath / (name + ".geom.glsl");
    if (FileSystem::Exists(geomPath))
        geomSrc = FileSystem::ReadTextFile(geomPath);

    return CreateFromSource(name, vertSrc, fragSrc, geomSrc);
}
```

The existing `CreateFromSource()` and `CompileStage()` / `LinkProgram()` remain unchanged —
they already accept GLSL source strings.

### 7.2 IGraphicsDevice Interface Update

```cpp
// Simplified — backend resolves artifact format internally
virtual Ref<IShader> CreateShader(const std::string& name) = 0;

// Keep for tests and procedural shaders
virtual Ref<IShader> CreateShaderFromSource(
    const std::string& name,
    const std::string& vertexSrc,
    const std::string& fragmentSrc,
    const std::string& geometrySrc = "") = 0;

// REMOVE:
// CreateShaderFromFiles()   ← no longer loading raw GLSL from asset dir
// CreateShaderFromStem()    ← SPIR-V stem concept replaced by backend-specific output
```

### 7.3 FileSystem Updates

```cpp
// New: returns build output directory for compiled shaders
static std::filesystem::path GetShaderOutputDir();
// → {build_dir}/shaders/

// Deprecated: GetShaderStem() — replace call sites with CreateShader(name)
```

### 7.4 Render Pass Updates

All passes currently call `GetDevice()->CreateShaderFromStem(shaderStem, "ForwardLit")`.
After migration:

```cpp
// Before
auto stem = FileSystem::GetShaderStem("ForwardLit");
m_Shader = GetDevice()->CreateShaderFromStem(stem, "ForwardLit");

// After
m_Shader = GetDevice()->CreateShader("ForwardLit");
```

---

## 8. IShader Interface Evolution

The migration to Slang is a natural opportunity to evolve the uniform setter interface, because
Slang's structured resource model maps directly to the buffer-based binding that Vulkan and Metal
require. However, the interface change can be done **independently** of the Slang migration.

### Current: Name-Based Setters

```cpp
shader->SetMat4("u_ViewProjection", viewProjection);
shader->SetFloat3("u_CameraPosition", cameraPos);
```

This works for OpenGL (`glGetUniformLocation` + `glUniform*`), but cannot map to
Vulkan descriptor sets or Metal argument buffers.

### Target: Buffer + Resource Binding

```cpp
// Upload a pre-packed uniform buffer to a binding slot
virtual void BindUniformBuffer(uint32_t slot, const Ref<IBuffer>& buffer) = 0;

// Bind texture + sampler to a slot
virtual void BindTexture(uint32_t slot, const Ref<ITexture2D>& texture) = 0;

// Small per-draw data (maps to push constants on Vulkan, setVertexBytes on Metal)
virtual void SetPushConstants(const void* data, uint32_t size) = 0;
```

### Transition Strategy

1. **Phase 1 (Slang migration)**: Keep name-based setters working on OpenGL. The Slang-generated
   GLSL still has uniform names that `glGetUniformLocation` can find.
2. **Phase 2 (Buffer binding)**: Add buffer/resource binding methods to IShader. OpenGL implements
   them via UBOs (`glBindBufferBase`). Vulkan/Metal use their native binding models.
3. **Phase 3 (Deprecate name setters)**: Once all passes and Material use buffer binding,
   remove the per-uniform setter methods.

This is detailed further in `docs/shader-and-material-system.md`.

---

## 9. Migration Steps

### Step 1: Add Slang Toolchain

- Download slangc prebuilt binary, place in `vendor/slang/bin/`
- Create `cmake/FindSlang.cmake` or equivalent
- Verify `slangc --version` works in CMake configure

### Step 2: Write First Slang Shader (TexturePreview)

- Create `assets/shaders/TexturePreview.slang`
- Add slangc compile command to CMake (GLSL target only for now)
- Verify the generated GLSL compiles and renders correctly via OpenGL
- Compare output with existing SPIRV-Cross transpiled GLSL

Choose TexturePreview because it is the simplest shader (no lighting, no shadow, no material).

### Step 3: Migrate Remaining Shaders

- `ShadowDepth.slang` — next simplest (vertex-only, no fragment output)
- `ForwardLit.slang` + `modules/shadow.slang` — most complex, validates module import

### Step 4: Update Runtime Loading

- Add `GLShader::CreateFromCompiledGlsl()` or modify `CreateFromStem()` to read
  from `build/shaders/glsl/` instead of `assets/shaders/*.spv`
- Update `IGraphicsDevice` to expose `CreateShader(name)`
- Update all render passes to use new loading path

### Step 5: Remove Old Toolchain

- Remove glslang from `vendor/CMakeLists.txt` and `vendor/glslang/` submodule
- Remove SPIRV-Cross from `vendor/CMakeLists.txt` and `vendor/spirv-cross/` submodule
- Remove `GLShader::TranspileSpirvToGlsl()` and `spirv_glsl.hpp` include
- Remove old `cmake/CompileShaders.cmake` glslang commands
- Delete old `.vert`, `.frag`, `.spv` files from `assets/shaders/`
- Update `src/CMakeLists.txt` link targets (remove spirv-cross-core, spirv-cross-glsl)

### Step 6: Add SPIR-V Target (Vulkan Prep)

- Add `-target spirv` output to CMake for each shader
- Verify generated SPIR-V with `spirv-val` (from Vulkan SDK)
- This prepares for the Vulkan backend without requiring it

### Step 7: Add Metal Target (Metal Backend Prep)

- Add `-target metal` output to CMake
- Verify generated MSL compiles with `xcrun -sdk macosx metal` (on macOS)

### Step 8: Validate and Clean Up

- Run all tests (78/78 should pass)
- Verify both demos render identically to pre-migration
- Update `docs/next-steps.md` Phase 4 R3 section
- Remove `docs/shader-vulkan-migration.md` (superseded by this doc and `shader-and-material-system.md`)

---

## 10. Risk Assessment

| Risk | Severity | Mitigation |
|------|----------|------------|
| Slang GLSL output has subtle differences from SPIRV-Cross output | Medium | Visual comparison test; keep old shaders in a branch until verified |
| slangc GLSL output uses constructs unsupported by older GL drivers | Low | Target GLSL 460 profile; project already requires GL 4.6 |
| Slang module system edge cases | Low | Only one module (shadow) initially; simple import graph |
| Build time increase from slangc | Low | Only 3 shaders; slangc is fast for small files |
| slangc binary size / distribution | Low | ~50MB; can use FetchContent to download on demand |
| `ParameterBlock` maps to UBOs, but current OpenGL code uses bare uniforms | Medium | Slang GLSL output maps ParameterBlock to GLSL uniform blocks; GLShader needs UBO binding support (see Section 8) |

### Fallback Plan

If Slang's GLSL output is incompatible with the current OpenGL uniform setter model:

1. **Option A**: Use Slang with `cbuffer` instead of `ParameterBlock`. Slang can emit these as
   plain GLSL uniform blocks that `glGetUniformLocation` can still access member-by-member.
2. **Option B**: Keep the old GLSL shaders as a parallel path during transition. Both pipelines
   can coexist in CMake.

---

## 11. Relationship to Other Documents

| Document | Status After Migration |
|----------|----------------------|
| `shader-vulkan-migration.md` | **Superseded** — UBO/push-constant migration is handled by Slang's resource model. Remove or archive. |
| `material_system_design.md` | **Merged** into `shader-and-material-system.md` — Material evolution plan updated to account for Slang reflection and buffer binding. |
| `next-steps.md` | **Update** Phase 4 R3 description and R4/R5 to reflect Slang toolchain. |
| `roadmap.md` | **Update** Phase 1 shader pipeline description. |

---

## References

- [Slang Official Documentation](https://shader-slang.org/slang/user-guide/)
- [Slang GitHub Repository](https://github.com/shader-slang/slang)
- [Compiling Code with Slang](https://shader-slang.org/slang/user-guide/compiling)
- [Khronos Slang Initiative Announcement](https://www.khronos.org/news/press/khronos-group-launches-slang-initiative-hosting-open-source-compiler-contributed-by-nvidia)
- [Sascha Willems — Vulkan Samples in Slang](https://www.saschawillems.de/blog/2025/06/03/shaders-for-vulkan-samples-now-also-available-in-slang/)
