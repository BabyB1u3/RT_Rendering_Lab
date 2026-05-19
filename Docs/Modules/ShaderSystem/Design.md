# Shader System

A Slang-based shader authoring, compilation, reflection, and parameter-binding
system for RTRLab's multi-backend renderer. The shader system owns compiler
interaction and reflection normalization; the RHI consumes compiled shader packages
without depending on Slang directly.

> **Design Philosophy**: Shader source is authored once, then lowered into backend
> artifacts by the shader system. Runtime rendering code binds resources through
> engine-owned reflection data rather than compiler-specific structures. The RHI sees
> only compiled code blobs, neutral reflection, and derived pipeline layouts.

---

## Table of Contents

- [Shader System](#shader-system)
  - [Table of Contents](#table-of-contents)
  - [1. Motivation](#1-motivation)
  - [2. Architecture Overview](#2-architecture-overview)
    - [2.1 System Boundary](#21-system-boundary)
    - [2.2 Compile Flow](#22-compile-flow)
    - [2.3 Runtime Binding Flow](#23-runtime-binding-flow)
  - [3. Shader Authoring Model](#3-shader-authoring-model)
    - [3.1 Slang as the Source Language](#31-slang-as-the-source-language)
    - [3.2 Entry Points and Stages](#32-entry-points-and-stages)
    - [3.3 Parameter Grouping](#33-parameter-grouping)
    - [3.4 Version 1 Feature Envelope](#34-version-1-feature-envelope)
  - [4. Compilation Model](#4-compilation-model)
    - [4.1 Public Compiler Request](#41-public-compiler-request)
    - [4.2 Slang Compile Plan](#42-slang-compile-plan)
    - [4.3 Development and Shipping Policy](#43-development-and-shipping-policy)
    - [4.4 Backend Outputs](#44-backend-outputs)
  - [5. Reflection Model](#5-reflection-model)
    - [5.1 Neutral Reflection Data](#51-neutral-reflection-data)
    - [5.2 Binding and Layout Semantics](#52-binding-and-layout-semantics)
    - [5.3 Normalization and Validation](#53-normalization-and-validation)
    - [5.4 Reflection Parser Boundary](#54-reflection-parser-boundary)
  - [6. ShaderProgram and RHI Boundary](#6-shaderprogram-and-rhi-boundary)
  - [7. Shader Parameter Writer](#7-shader-parameter-writer)
    - [7.1 Path-Based Convenience Tier](#71-path-based-convenience-tier)
    - [7.2 Handle-Based Hot Path Tier](#72-handle-based-hot-path-tier)
    - [7.3 ResourceSet Contract](#73-resourceset-contract)
  - [8. Pipeline Layout Integration](#8-pipeline-layout-integration)
  - [9. Resource System Integration](#9-resource-system-integration)
  - [10. Current Implementation Status](#10-current-implementation-status)
  - [11. Current Gaps and Near-Term Plan](#11-current-gaps-and-near-term-plan)
  - [12. File Layout](#12-file-layout)
  - [13. Key Design Decisions](#13-key-design-decisions)
    - [Why Slang is behind ShaderCompiler](#why-slang-is-behind-shadercompiler)
    - [Why reflection is engine-owned](#why-reflection-is-engine-owned)
    - [Why pipeline layout is derived, not stored](#why-pipeline-layout-is-derived-not-stored)
    - [Why parameter writes go through ResourceSet](#why-parameter-writes-go-through-resourceset)
    - [Why runtime compilation is development-only](#why-runtime-compilation-is-development-only)
  - [Appendix A: Alternatives Considered](#appendix-a-alternatives-considered)
    - [Raw GLSL / HLSL per backend](#raw-glsl--hlsl-per-backend)
    - [RHI directly invokes Slang](#rhi-directly-invokes-slang)
    - [Manual binding tables instead of reflection](#manual-binding-tables-instead-of-reflection)
    - [Global name lookup at every draw](#global-name-lookup-at-every-draw)

---

## 1. Motivation

The renderer needs a shader path that can grow beyond single-demo Vulkan shaders
without forcing every backend and every draw path to understand compiler details.
Hand-authored per-backend shader code works for simple examples, but it scales poorly
once the engine needs:

- one source representation for Vulkan and Metal
- stable reflection data for resource binding and pipeline layout creation
- parameter writes by semantic name instead of hard-coded descriptor indices
- a development compiler path and a future cooked shader asset path
- a clear boundary between compiler tooling and runtime RHI objects

The shader system provides that boundary. Slang remains the authoring and lowering
tool, but Slang types do not leak into the RHI. The runtime contract is an
engine-owned `CompiledShaderProgramDesc` containing backend code blobs plus neutral
reflection data.

**What the system provides**:

- Slang source compilation through a `ShaderCompiler` abstraction.
- Explicit shader entry-point descriptors for vertex, fragment, and compute stages.
- Backend-target descriptors for Vulkan SPIR-V and Metal MSL source.
- Reflection JSON parsing and conversion into engine-owned `ShaderReflectionData`.
- Validation and normalization of reflected fields, bindings, stage masks, and layout
  conventions.
- Name-based and handle-based parameter writing through `ShaderParameterWriter`.
- RHI-facing shader programs that derive `PipelineLayoutDesc` from stored reflection.

---

## 2. Architecture Overview

```text
Authoring / Tools                 Shader System                        RHI Runtime
+-------------------+       +-------------------------+        +---------------------+
| .slang modules    | ----> | ShaderCompiler          | -----> | Device              |
| entry descriptors |       |  - slangc invocation    |        |  CreateShaderProgram|
| target descriptors|       |  - artifact collection  |        |  CreatePipelineLayout
+-------------------+       |  - reflection parsing   |        +----------+----------+
                            +------------+------------+                   |
                                         |                                |
                                         v                                v
                            +-------------------------+        +---------------------+
                            | CompiledShaderProgramDesc|       | ShaderProgram       |
                            |  - backend blobs         |       |  - shader modules   |
                            |  - ShaderReflectionData  |       |  - reflection       |
                            +------------+------------+        +----------+----------+
                                         |                                |
                                         v                                v
                            +-------------------------+        +---------------------+
                            | ShaderParameterWriter   | -----> | ResourceSet         |
                            |  path/handle resolution |        | descriptor + consts |
                            +-------------------------+        +---------------------+
```

### 2.1 System Boundary

The primary boundary type is `CompiledShaderProgramDesc`:

- the shader system produces it
- `Device::CreateShaderProgram()` consumes it
- backend `ShaderProgram` objects store the reflection and create native shader
  handles
- `ShaderProgram::DerivePipelineLayoutDesc()` computes the RHI layout contract from
  reflection

The RHI does not invoke Slang, parse Slang reflection JSON, or inspect compiler
sessions. It consumes backend code bytes and engine reflection only.

### 2.2 Compile Flow

Normal development compilation proceeds as:

```text
ShaderCompileRequest
  -> ValidateShaderCompileRequest()
  -> SlangCompiler::BuildCompilePlan()
  -> slangc per target / per entry point
  -> read generated code artifacts
  -> parse reflection JSON sidecars
  -> merge entry-point reflection documents
  -> ConvertSlangReflectionToNeutral()
  -> NormalizeShaderReflectionData()
  -> ValidateCompiledShaderProgramDesc()
```

The current compiler implementation shells out to `slangc` rather than linking against
the Slang C++ API. That keeps the compiler dependency at the tooling edge and makes
temporary artifacts inspectable when compilation fails.

### 2.3 Runtime Binding Flow

Runtime resource binding proceeds as:

```text
ShaderProgram reflection
  -> ShaderParameterWriter lookup tables
  -> ResolveField("gFrame.viewProj") / ResolveBinding("gAlbedoTexture")
  -> ResourceSet constant writes or descriptor writes
  -> CommandList::BindResourceSet(setIndex, resourceSet)
  -> backend descriptor binding
```

The public RHI `ResourceSet` API is intentionally index-based. Name lookup belongs in
`ShaderParameterWriter`, which can be used ergonomically during setup or pre-resolved
for hot paths.

---

## 3. Shader Authoring Model

### 3.1 Slang as the Source Language

Project shaders are authored as `.slang` modules under `Project/Shaders/`. Current
demo shaders use HLSL-style syntax and explicit registers:

```hlsl
ConstantBuffer<FrameParams> gFrame : register(b0, space0);
ConstantBuffer<MaterialParams> gMaterial : register(b0, space1);
ConstantBuffer<ObjectParams> gObject : register(b0, space2);
Texture2D gAlbedoTexture : register(t0, space1);
SamplerState gAlbedoSampler : register(s0, space1);
```

This style makes the logical resource grouping visible in source while still allowing
Slang to lower the module to backend-specific code.

The long-term recommended authoring shape is logical parameter grouping:

- frame / scene data
- material data and resources
- object / instance data
- small draw-specific push constants where appropriate

### 3.2 Entry Points and Stages

Entry points are explicit. A shader module may expose separate functions for each
stage:

```hlsl
[shader("vertex")]
VertexOutput main_vertex(VertexInput input);

[shader("fragment")]
float4 main_fragment(VertexOutput input) : SV_Target;
```

The engine-side request uses `ShaderEntryPointDesc` to bind module name, entry-point
name, and stage:

```cpp
request.m_Source.m_Entries.push_back({shaderModule, "main_vertex", ShaderStage::Vertex});
request.m_Source.m_Entries.push_back({shaderModule, "main_fragment", ShaderStage::Fragment});
```

Each entry point must map to a single concrete stage. The compiler request validator
rejects empty modules, empty entry names, invalid stage masks, and duplicate entry
points for the same stage.

### 3.3 Parameter Grouping

The current renderer convention is:

| Set | Purpose | Typical Binding |
|-----|---------|-----------------|
| 0 | Frame / scene | camera matrices, frame time, global tint, lights |
| 1 | Material | material constants, textures, samplers |
| 2 | Object / instance | model matrix, object ID, per-object data |

This convention is used by the demos and `ForwardRenderer`. It is not hard-coded into
the compiler, but renderer helpers currently expect to find bindings named `gFrame`,
`gMaterial`, and `gObject` in the derived pipeline layout.

### 3.4 Version 1 Feature Envelope

The first implementation intentionally supports a modest shader feature set:

| Recommended | Deferred |
|-------------|----------|
| explicit entry points per stage | bindless descriptor arrays |
| `ConstantBuffer<>` / parameter-block-like grouping | nested parameter block hierarchies |
| separate textures and samplers | aggressive backend-specific source branches |
| fixed descriptor counts | runtime specialization systems |
| non-bindless resources | shader graph generation |
| simple frame/material/object structure | automatic material permutation cooking |

The goal is to stabilize the boundary and reflection model before broadening the
language surface.

---

## 4. Compilation Model

### 4.1 Public Compiler Request

The public compilation request is intentionally small:

```cpp
struct ShaderCompileRequest
{
    ShaderSourceDesc m_Source;
    std::vector<ShaderCompileTargetDesc> m_Targets;
};
```

`ShaderSourceDesc` owns entry points and compile defines. `ShaderCompileTargetDesc`
names the backend target and, for Metal, whether the desired output is MSL source or a
future `.metallib` byte stream.

The request does not expose raw `slangc` arguments. Backend and project policy are
translated into Slang command-line arguments by the compile-plan builder.

### 4.2 Slang Compile Plan

`BuildSlangCompilePlan()` expands one high-level request into one `SlangCompileJob`
per target and entry point. Each job records:

- backend type
- shader stage
- module name
- entry point
- Slang target string
- generated argument list
- reflection sidecar policy
- output code format

For Vulkan, the compile plan currently adds:

- `-target spirv`
- `-emit-spirv-directly`
- HLSL register-class binding shifts for `b`, `t`, `s`, and `u`

For Metal, the compile plan currently adds:

- `-target metal`
- MSL source output

Matrix layout is currently forced through `-matrix-layout-column-major` when configured.

### 4.3 Development and Shipping Policy

Runtime compilation is a development feature. `CreateShaderCompiler()` returns a real
`SlangCompiler` only when CMake has injected `RTRLAB_SLANGC_PATH`. Otherwise it returns
a disabled compiler that reports a clear error stating that shipping/release builds
must use cooked shader assets.

This keeps the runtime path honest:

- development builds may compile `.slang` files on demand
- shipping builds should load cooked shader artifacts
- long-term shader hot reload belongs to the development compiler path
- packaged games should not depend on an external `slangc` executable

### 4.4 Backend Outputs

The compiler output is a `CompiledShaderProgramDesc`:

```cpp
struct CompiledShaderProgramDesc
{
    std::vector<CompiledShaderBlob> m_Blobs;
    ShaderReflectionData m_Reflection;
};
```

Current backend blob policy:

| Backend | Code Format | Runtime Consumer |
|---------|-------------|------------------|
| Vulkan | SPIR-V bytes | `vkCreateShaderModule` |
| Metal | MSL source text | `newLibraryWithSource` path in Metal backend |
| Metal future | `.metallib` bytes | `newLibraryWithData` path |

Each blob records the backend, stage, backend entry-point name, code bytes, and Metal
code format. Vulkan SPIR-V entry points are currently normalized to `"main"` because
Slang emits the exported SPIR-V entry with that name.

---

## 5. Reflection Model

### 5.1 Neutral Reflection Data

The engine reflection model lives in `ShaderTypes.h` and is independent of Slang
runtime types:

```cpp
struct ShaderReflectionData
{
    std::vector<ReflectedField> m_Globals;
    std::vector<PushConstantRangeDesc> m_PushConstants;
};
```

`ReflectedField` is a tree. It can describe:

- struct nodes
- leaf constant-data fields
- textures
- storage textures
- samplers
- buffers
- parameter blocks / constant buffers

The same field structure carries both constant layout metadata and resource binding
metadata. Only the relevant subset is meaningful for a given `ReflectedTypeKind`.

### 5.2 Binding and Layout Semantics

For constant data, reflected fields carry:

- byte offset
- byte size
- alignment
- array stride
- matrix stride
- layout convention

For resources, reflected fields carry:

- descriptor set index
- binding index
- array count
- stage mask

The design intent is a canonical CPU-side constant layout so that
`ShaderParameterWriter` can write the same value blob regardless of backend. The
current validation requires `LayoutConvention::Std430` for struct-like fields.

Current implementation note: reflection conversion uses the offsets and binding indices
reported by Slang JSON. That is correct for the currently compiled active backend, but
multi-backend reflection caching still needs a stricter per-backend or canonicalization
policy before it can be considered complete.

### 5.3 Normalization and Validation

`NormalizeShaderReflectionData()` enforces stable reflection shape before runtime use:

- propagates set index from parameter blocks to children
- propagates stage masks when a child does not provide one
- normalizes resource array counts to at least 1
- sorts globals and child fields deterministically
- validates field names and sibling uniqueness
- rejects nested parameter blocks in version 1
- rejects non-Std430 struct-like layout
- validates push constant ranges

This normalization step makes downstream lookup tables and pipeline-layout derivation
deterministic.

### 5.4 Reflection Parser Boundary

Slang reflection enters the engine through a two-step boundary:

1. `ParseSlangReflectionJson()` parses raw `slangc -reflection-json` into
   `SlangReflectionDocument`.
2. `ConvertSlangReflectionToNeutral()` maps that document into `ShaderReflectionData`.

The parser owns the external schema details: `parameters`, `entryPoints`, `bindings`,
`elementType`, `elementVarLayout`, `genericArgs`, and field layout payloads. The
converter owns engine semantics: parameter block detection, resource-kind mapping,
stage mask collection, array count calculation, and neutral field construction.

This split is important. It keeps raw Slang JSON handling narrow and makes the rest of
the engine talk only about engine reflection types.

---

## 6. ShaderProgram and RHI Boundary

`ShaderProgram` is an RHI object, but its creation input comes from the shader system.
The public interface is intentionally minimal:

```cpp
class ShaderProgram
{
public:
    virtual const ShaderReflectionData& GetReflection() const = 0;
    virtual PipelineLayoutDesc DerivePipelineLayoutDesc() const = 0;
};
```

Backend implementations own native handles:

- Vulkan stores `VkShaderModule` objects per stage.
- Metal stores compiled Metal functions and rewritten/compiled library data.
- Shell/test implementations store reflection only.

`ShaderProgram` does not own `PipelineLayout`. It can derive a layout description from
reflection, and the caller passes that description to `Device::CreatePipelineLayout()`.
This preserves a clean ownership boundary:

- shader program: code and reflection
- pipeline layout: descriptor-set and push-constant layout
- graphics/compute pipeline: executable pipeline state using both

---

## 7. Shader Parameter Writer

`ShaderParameterWriter` is the ergonomic layer between reflected names and index-based
RHI resource sets.

It is constructed from `ShaderReflectionData` and builds:

- path-to-field-handle lookup table
- path-to-binding-handle lookup table
- compact field metadata for constants
- compact binding metadata for resources

### 7.1 Path-Based Convenience Tier

Path-based calls resolve dotted field paths at call time:

```cpp
parameterWriter.SetMatrix4x4(*frameSet, "gFrame.viewProj", viewProjection);
parameterWriter.SetFloat4(*materialSet, "gMaterial.baseColor", baseColor);
parameterWriter.SetTextureView(*materialSet, "gAlbedoTexture", albedoView);
```

This is suitable for setup code, demos, debug panels, and one-time material updates.
It is not intended for tight draw loops.

### 7.2 Handle-Based Hot Path Tier

Hot paths should pre-resolve handles:

```cpp
FieldHandle viewProj = writer.ResolveField("gFrame.viewProj");
BindingHandle albedo = writer.ResolveBinding("gAlbedoTexture");
```

The handle overloads avoid repeated string lookup. Handles are tied to the reflection
data used to create the writer and must be re-resolved after shader hot reload.

### 7.3 ResourceSet Contract

`ShaderParameterWriter` writes into an existing `ResourceSet`. It validates that the
target resource set has the expected descriptor set index before writing.

Constant writes:

- resolve to a byte offset and size
- write into the set's `ParameterBlockData`
- are bounds-checked against the reflected uniform-buffer byte size

Resource writes:

- resolve to a descriptor binding
- validate resource kind
- validate array count
- forward to `ResourceSet::SetTextureArray`, `SetSamplerArray`, or `SetBufferArray`

Current implementation note: constant field metadata records set index but not the
constant-buffer binding. This matches the current convention of one parameter block per
set. Supporting multiple constant buffers in the same set requires adding binding
identity to constant field handles and resource-set constant storage.

---

## 8. Pipeline Layout Integration

Pipeline layout derivation is shared by shell and real backends through
`BuildPipelineLayoutDescFromReflection()`.

The derivation walks reflection globals recursively and collects every pipeline-bindable
field:

- parameter blocks become `ResourceKind::UniformBuffer`
- textures become `ResourceKind::SampledTexture`
- storage textures become `ResourceKind::StorageTexture`
- samplers become `ResourceKind::Sampler`
- buffers become `ResourceKind::StorageBuffer`

Bindings with matching set, binding, and resource kind are merged by OR-ing their stage
masks. The final layout description is sorted by set, binding, kind, and name.

Vulkan then turns the derived layout into:

- `VkDescriptorSetLayout` objects, one per descriptor set index
- `VkPipelineLayout`
- descriptor pools and allocated descriptor sets for each `ResourceSet`

This is why reflection correctness matters. Incorrect set or binding data becomes an
incorrect backend descriptor layout.

---

## 9. Resource System Integration

Shader source files currently live under project content:

```text
Project/Shaders/DemoColored.slang
Project/Shaders/DemoTextured.slang
```

The current development path passes physical `std::filesystem::path` module strings to
Slang. The long-term resource-system-aligned path should treat shader source and cooked
shader artifacts as project resources:

- source `.slang` modules in `/Project/Shaders/...`
- generated SPIR-V / MSL / reflection artifacts in `/Cache/Shaders/...` during
  development
- cooked shader artifacts in packaged Project / Engine content for shipping
- optional saved or cache diagnostics for failed compile sessions

This document treats that as an integration direction. The current source code still
uses filesystem paths for direct `slangc` invocation.

---

## 10. Current Implementation Status

As of the current codebase, the shader system is an active subsystem with the following
implemented behavior:

- `Src/Render/Shader/` contains the compiler, reflection, and parameter-writer code.
- `ShaderTypes.h` defines backend, stage, reflection, and compiled-program boundary
  types.
- `ShaderCompiler` validates requests and compiled program packages.
- `SlangCompiler` shells out to an explicit CMake-injected `slangc` path.
- Slang compile jobs generate temporary session artifacts under the cache directory.
- Vulkan compilation emits SPIR-V directly.
- Metal compilation emits MSL source; `.metallib` output is declared but not yet
  implemented by the execution path.
- Slang reflection JSON is parsed into `SlangReflectionDocument`.
- Slang reflection is converted into `ShaderReflectionData`.
- Reflection is normalized and validated before runtime use.
- `ShaderParameterWriter` supports constants, textures, texture views, samplers,
  buffers, and arrays for resource descriptors.
- Vulkan `CreateShaderProgram()` creates `VkShaderModule` objects from shader blobs.
- Vulkan `CreatePipelineLayout()` derives descriptor set layouts from reflection.
- Vulkan `ResourceSet` writes descriptors and uploads constant data through per-frame
  uniform-buffer upload arenas.
- `ForwardRenderer` and demos use the shader system to compile demo shaders and update
  frame, material, and object parameters.

The system is functional for the current demo renderer, but it is not yet a complete
production shader asset pipeline.

---

## 11. Current Gaps and Near-Term Plan

### 11.1 Multi-backend reflection policy

The current reflection package is a single `ShaderReflectionData` object. That is
adequate when a program is compiled for the active backend only. It is not enough for a
true multi-backend cooked artifact if backend-specific lowering changes binding indices
or layout metadata.

Near-term plan:

- keep runtime compile requests single-backend for now
- document and test the current Vulkan binding-shift policy
- decide whether cooked artifacts store per-backend reflection or a canonical logical
  reflection plus per-backend lowering data

### 11.2 Constant-buffer binding identity

Current `ShaderParameterWriter` constant handles do not include a constant-buffer
binding. The resource-set implementation finds the first uniform buffer for the set.
This matches the frame/material/object convention but is not a general descriptor-set
model.

Near-term plan:

- validate that version 1 shaders contain at most one parameter-block/constant-buffer
  binding per set
- or extend constant field metadata to include the owning binding
- update `ResourceSet` constant storage if multiple constant buffers per set become a
  real requirement

### 11.3 Shader-system tests

The wider unit suite exists, but shader-system-specific coverage is thin.

Near-term plan:

- add tests for Slang reflection JSON parsing
- add tests for neutral conversion and normalization
- add tests for pipeline-layout derivation from hand-built reflection
- add tests for `ShaderParameterWriter` field and binding resolution
- add validation tests for invalid nested parameter blocks, duplicate names, and
  unsupported layouts

### 11.4 Cooked shader assets

Runtime compilation is not the final shipping path. Cooked shader artifacts are still a
future asset-pipeline step.

Near-term plan:

- define a cooked shader artifact schema
- store backend code blobs and reflection together
- route cooked shader reads through the resource system
- keep development compile sessions inspectable in `/Cache/Shaders/...`

### 11.5 Hot reload

The system has enough structure to support hot reload, but no full reload loop exists
yet.

Near-term plan:

- watch source dependencies in development builds
- recompile affected programs
- rebuild shader programs and pipeline layouts
- notify renderer/material code to re-resolve parameter handles
- preserve old programs until in-flight frames are safe to retire

---

## 12. File Layout

```text
Src/Render/Shader/
    ShaderTypes.h                    - public shader/RHI boundary data types
    ShaderCompiler.h / .cpp          - compiler interface, request/result validation
    SlangCompiler.h / .cpp           - slangc configuration, planning, invocation
    SlangReflectionJson.h / .cpp     - raw Slang reflection JSON parser
    SlangReflectionConverter.h / .cpp - Slang document to neutral reflection
    ShaderReflection.h / .cpp        - reflection normalization and validation
    ShaderParameterWriter.h / .cpp   - name/handle based ResourceSet writer

Src/Render/RHI/
    RHIPipeline.h                    - ShaderProgram and PipelineLayout public API
    RHIResources.h                   - ResourceSet and ParameterBlockData API
    Backends/Common/
        RHIShellCommon.h / .cpp      - shared reflection-to-layout derivation
    Backends/Vulkan/
        Pipeline/VulkanShaderProgram.h
        Pipeline/VulkanPipelineLayout.h
        Common/VulkanDescriptors.h / .cpp
        Resources/VulkanResourceSet.h / .cpp
        Device/VulkanDevice.h / .cpp
    Backends/Metal/
        Pipeline/MetalShaderProgram.h / .mm
        Common/MetalConversions.h / .mm
        Resources/MetalResourceSet.h / .mm
        Device/MetalDevice.h / .mm

Project/Shaders/
    DemoColored.slang
    DemoTextured.slang

Docs/Modules/ShaderSystem/
    Design.md
    Design/ShaderSystem.md           - older design note retained for history
    CodeReview/
    Internals/
```

---

## 13. Key Design Decisions

### Why Slang is behind ShaderCompiler

The renderer should not care whether shader compilation is implemented by a linked
Slang API, an external `slangc` process, a build-system step, or a cooked artifact
loader. `ShaderCompiler` keeps that decision behind a small interface and gives the
rest of the engine one request/result contract.

### Why reflection is engine-owned

Slang reflection is an external schema and may change with compiler versions or target
lowering. Engine-owned reflection lets the runtime validate and cache exactly the data
it needs: field paths, byte offsets, descriptor set/binding indices, array counts, and
stage masks.

### Why pipeline layout is derived, not stored

Pipeline layout belongs to the RHI backend, not to the compiler. Keeping it derived
from reflection lets `ShaderProgram` remain a code-and-reflection object while
`PipelineLayout` remains a descriptor layout object owned by the device.

### Why parameter writes go through ResourceSet

`ResourceSet` is the RHI binding object. `ShaderParameterWriter` resolves names into
resource-set operations, but it does not own GPU descriptors or upload buffers. That
keeps name lookup in the shader system and backend memory management in the RHI.

### Why runtime compilation is development-only

Shipping builds should not require an external compiler executable, should not pay
runtime compile cost, and should not fail because shader source files are missing from
the install. Runtime compilation is valuable for development and hot reload; cooked
shader artifacts are the correct production path.

---

## Appendix A: Alternatives Considered

### Raw GLSL / HLSL per backend

Separate source files per backend are simple at first, but every material and renderer
feature must be implemented multiple times. Reflection also becomes backend-specific
from day one.

**Verdict**: Acceptable for isolated experiments, poor as the engine's main shader
path.

### RHI directly invokes Slang

Letting Vulkan or Metal device code compile shader source would reduce one layer, but
it would mix compiler policy into backend runtime objects. The RHI would need to know
about source paths, compiler arguments, reflection JSON, cache directories, and error
reporting.

**Verdict**: Rejected. The RHI should consume compiled shader packages.

### Manual binding tables instead of reflection

Manual tables are deterministic and easy to debug, but they duplicate shader source
truth. Every time a shader changes, the C++ table must be updated by hand.

**Verdict**: Useful as a fallback or test fixture, not the primary runtime contract.

### Global name lookup at every draw

The ergonomic path of resolving `"gFrame.viewProj"` every time is pleasant, but it
allocates and hashes strings on hot paths. The writer therefore supports both
convenience lookup and pre-resolved handles.

**Verdict**: Keep string paths for setup and debugging; use handles for repeated work.
