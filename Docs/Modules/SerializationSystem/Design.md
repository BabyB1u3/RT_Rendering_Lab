# Serialization System

A format-agnostic serialization framework that decouples data types from their on-disk
representation. Enables JSON today and YAML / binary tomorrow without modifying any
domain type.

> **Design Philosophy**: Types declare *what* to serialize via lightweight trait specialization.
> Format backends (JSON, YAML, binary) handle *how*. No macros, no inheritance tax, no runtime
> type registration. Serialize into an intermediate `PropertyTree` — backends read/write that tree.
> Named enum types may use `magic_enum` inside trait helpers for token conversion, but field-level
> serialization remains explicit trait code rather than generalized structural reflection.

---

## Table of Contents

- [Serialization System](#serialization-system)
  - [Table of Contents](#table-of-contents)
  - [1. Motivation](#1-motivation)
  - [2. Architecture Overview](#2-architecture-overview)
  - [3. Layer 0 - PropertyTree (Intermediate Representation)](#3-layer-0---propertytree-intermediate-representation)
  - [4. Layer 1 - Serialization Traits](#4-layer-1---serialization-traits)
    - [4.1 Built-in Trait Specializations](#41-built-in-trait-specializations)
    - [4.2 Domain Type Example - Transform](#42-domain-type-example---transform)
    - [4.3 Domain Type Example - DirectionalLight](#43-domain-type-example---directionallight)
  - [5. Layer 2 - Format Backends](#5-layer-2---format-backends)
    - [5.1 JSON Backend](#51-json-backend)
    - [5.2 Future Backends](#52-future-backends)
  - [6. Layer 3 - Schema Validation](#6-layer-3---schema-validation)
  - [7. Layer 4 - File I/O Integration](#7-layer-4---file-io-integration)
    - [Live usage (ShadowMapping.cpp, MaterialPlayground.cpp):](#live-usage-shadowmappingcpp-materialplaygroundcpp)
  - [8. InputActionMap Integration](#8-inputactionmap-integration)
    - [8.1 Enum Strategy](#81-enum-strategy)
    - [8.2 magic\_enum Rollout Sequence](#82-magic_enum-rollout-sequence)
  - [9. Candidate Types for Future Serialization](#9-candidate-types-for-future-serialization)
    - [9.1 Shader Reflection Sidecar - Deferred Integration Plan](#91-shader-reflection-sidecar---deferred-integration-plan)
      - [Why This Path Is Still Special Today](#why-this-path-is-still-special-today)
      - [Why It Still Belongs In The Serialization Story Eventually](#why-it-still-belongs-in-the-serialization-story-eventually)
      - [Recommended Future Shape](#recommended-future-shape)
      - [Best Time To Focus This](#best-time-to-focus-this)
  - [10. Error Handling Strategy](#10-error-handling-strategy)
  - [11. File Layout](#11-file-layout)
  - [12. Implementation Roadmap](#12-implementation-roadmap)
    - [Phase 2 - Scene types](#phase-2---scene-types)
    - [Phase 3 - Schema validation](#phase-3---schema-validation)
    - [Phase 4 - Additional backends (as needed)](#phase-4---additional-backends-as-needed)
  - [13. Appendix A: Alternatives Considered](#13-appendix-a-alternatives-considered)
    - [A1. nlohmann/json `from_json` / `to_json` ADL directly](#a1-nlohmannjson-from_json--to_json-adl-directly)
    - [A2. Inheritance-based `ISerializable` interface](#a2-inheritance-based-iserializable-interface)
    - [A3. Macro-based reflection (e.g., `SERIALIZE_FIELDS(Position, Rotation, Scale)`)](#a3-macro-based-reflection-eg-serialize_fieldsposition-rotation-scale)
    - [A4. C++20 compile-time reflection (future standard)](#a4-c20-compile-time-reflection-future-standard)

---

## 1. Motivation

The original serialization approach embedded ~170 lines of inline `nlohmann/json` code
directly inside `InputActionMap::SaveToFile()` / `LoadFromFile()`. Each field check was
hand-written, error handling was ad-hoc, and the format was hard-coded to JSON with no
path to YAML or binary.

| Problem | Impact |
|---------|--------|
| JSON logic lives inside domain types | Every new serializable type duplicates file I/O, error handling, and format code |
| No shared validation strategy | Each consumer re-invents `.contains()` / `.is_object()` checks |
| Hard-coded to JSON | Adding YAML (human-authored configs) or binary (scene snapshots) requires rewriting every type |
| No versioning | Changing a field name silently breaks old save files |

**What the framework provides**:

- **Format independence** — domain types serialize into a `PropertyTree`; backends translate that tree to JSON, YAML, or binary. Swapping formats requires zero changes to domain code.
- **ADL-based traits** — free function pairs `Serialize` / `Deserialize` discovered via argument-dependent lookup. No base class, no virtual functions, no registration macros.
- **Built-in trait library** — out-of-the-box support for primitives, GLM types, `enum class` (via `magic_enum`), and standard containers.
- **Single coupling point** — `#include <json.hpp>` appears only in `JsonBackend.cpp`. No other engine file imports nlohmann/json for serialization.
- **Consistent error handling** — unknown keys warn rather than reject, type mismatches warn and skip, partial failures never commit.
- **Atomic load** — `LoadFromFile` deserializes into a temporary, then moves on success; the caller's value is unchanged if deserialization fails.

---

## 2. Architecture Overview

```
   Domain types                    Intermediate             Format backends
 ┌──────────────┐               ┌──────────────┐         ┌───────────────┐
 │  Transform   │──Serialize──▸ │              │──JSON──▸│ JsonBackend   │──▸ .json file
 │  Material    │               │ PropertyTree │──YAML──▸│ YamlBackend   │──▸ .yaml file
 │  Camera      │◂─Deserialize─ │              │──Bin───▸│ BinaryBackend │──▸ .bin  file
 │  InputAction │               └──────────────┘         └───────────────┘
 └──────────────┘                      ▲
                                       │
                               ┌───────┴───────┐
                               │SchemaValidator│ (optional, Layer 3)
                               └───────────────┘
```

Data flows in two directions:
- **Save**: `Type` → trait `Serialize()` → `PropertyTree` → backend `Write()` → file
- **Load**: file → backend `Read()` → `PropertyTree` → (validate) → trait `Deserialize()` → `Type`

---

## 3. Layer 0 - PropertyTree (Intermediate Representation)

A lightweight, format-neutral value tree. Analogous to `nlohmann::json` in shape but
owned by the engine — no third-party types leak into domain headers.

See `core/serialization/PropertyTree.h` for the full declaration. Key surface:

```cpp
namespace Serialization {

class PropertyTree
{
public:
    using Null   = std::monostate;
    using Bool   = bool;
    using Int    = int64_t;
    using Float  = double;
    using String = std::string;
    using Array  = std::vector<PropertyTree>;
    using Object = std::map<std::string, PropertyTree>;  // ordered for stable output

    using Value = std::variant<Null, Bool, Int, Float, String, Array, Object>;

    // Construction - implicit conversions for ergonomic building
    PropertyTree() = default;
    PropertyTree(std::nullptr_t);
    PropertyTree(bool v);
    PropertyTree(int v);
    PropertyTree(int64_t v);
    PropertyTree(double v);
    PropertyTree(float v);
    PropertyTree(const char *v);
    PropertyTree(std::string v);
    PropertyTree(Array v);
    PropertyTree(Object v);

    // Type queries
    bool IsNull()   const;
    bool IsBool()   const;
    bool IsInt()    const;
    bool IsFloat()  const;
    bool IsNumber() const;  // Int or Float
    bool IsString() const;
    bool IsArray()  const;
    bool IsObject() const;

    // Typed access (throws std::bad_variant_access on type mismatch)
    Bool          AsBool()   const;
    Int           AsInt()    const;
    Float         AsFloat()  const;  // also accepts Int and promotes
    const String &AsString() const;
    const Array  &AsArray()  const;
    const Object &AsObject() const;
    Array  &AsArray();               // mutable access
    Object &AsObject();              // mutable access

    // Object helpers
    bool Contains(const std::string &key) const;
    const PropertyTree &operator[](const std::string &key) const;
    PropertyTree       &operator[](const std::string &key);  // auto-promotes Null → Object

    template <typename T>
    T GetOr(const std::string &key, const T &fallback) const;  // never throws

    // Array helpers
    size_t Size() const;  // Array or Object size
    const PropertyTree &operator[](size_t index) const;
    PropertyTree       &operator[](size_t index);

    // Direct variant access (used by JsonBackend via std::visit)
    const Value &GetValue() const;

private:
    Value m_Value;
};

} // namespace Serialization
```

**Key decisions**:
- `Object` uses `std::map` (not `unordered_map`) so JSON output has stable, deterministic key ordering.
- `Float` is `double` internally — covers both `float` and `double` without loss.
- `Int` is `int64_t` — covers all integer types the project uses.
- `IsNumber()` returns true for either Int or Float, simplifying numeric deserialization.
- `operator[](const std::string&)` on a Null tree auto-promotes to Object for ergonomic building.
- No `glm::vec3` etc. in the tree — those are composed from arrays in the trait layer.
- Variant-based, no heap allocation for scalars.
- `AsFloat()` accepts `Int` and promotes — simplifies numeric deserialization without requiring `IsNumber()` checks in every trait.

---

## 4. Layer 1 - Serialization Traits

Each serializable type provides a **free function pair** discovered via ADL (argument-dependent
lookup). No base class, no virtual functions, no registration macro.

```cpp
/// @file Serialization/SerializationTraits.h

namespace Serialization {

// The two functions every serializable type must provide:
//
//   void Serialize(PropertyTree &tree, const T &value);
//   bool Deserialize(const PropertyTree &tree, T &value);
//
// They live in the type's own namespace (or in Serialization::) and are found via ADL.
// Deserialize returns false if the tree doesn't contain valid data for T.

// --- Concept (C++20) to constrain templates ---

template<typename T>
concept Serializable = requires(PropertyTree &tree, const PropertyTree &ctree, T &val, const T &cval)
{
    { Serialize(tree, cval) };
    { Deserialize(ctree, val) } -> std::same_as<bool>;
};

} // namespace Serialization
```

### 4.1 Built-in Trait Specializations

Provided in `core/serialization/BuiltinTraits.h`. All are `inline` functions (header-only —
no `.cpp` needed):

| Category | Types | Notes |
|----------|-------|-------|
| **Primitives** | `bool`, `int`, `int64_t`, `float`, `double`, `std::string`, `uint8_t`, `uint16_t` | Direct tree assignment; numeric Deserialize uses `IsNumber()` for int→float promotion |
| **Named enums** | Any `enum class` via `magic_enum` | Serialize as string token; Deserialize with `enum_cast`. Opt out by providing a custom overload (e.g., `Key::Code` uses `InputNames.h`) |
| **GLM** | `glm::vec2`, `glm::vec3`, `glm::vec4`, `glm::mat4` | Arrays: `[x,y]`, `[x,y,z]`, `[x,y,z,w]`, 16 floats column-major |
| **Containers** | `std::vector<T>`, `std::unordered_map<std::string, T>`, `std::optional<T>` | Require `Serializable T`; optional serializes as null when empty |

Enum trait default path (magic_enum):
```cpp
template <typename E>
    requires std::is_enum_v<E>
void Serialize(PropertyTree &tree, E value);    // → enum_name string

template <typename E>
    requires std::is_enum_v<E>
bool Deserialize(const PropertyTree &tree, E &value);  // ← enum_cast
```

Provide a custom overload instead of the default path when:
- on-disk tokens must differ from C++ enumerator spellings,
- alias values need a canonical external name,
- the type is not a real `enum class` (e.g. `Key::Code` / `Mouse::Code` are `uint16_t`),
- bitflag enums need list/bitmask encoding instead of a single token.

### 4.2 Domain Type Example - Transform

> *This section shows the planned trait implementation for Phase 2 scene serialization.
> `TransformSerialization.h` does not exist yet.*

```cpp
// In Transform.h or a dedicated TransformSerialization.h

#include "Serialization/SerializationTraits.h"

namespace Serialization {

inline void Serialize(PropertyTree &tree, const Transform &t)
{
    tree = PropertyTree::Object{
        {"Position",             {}},
        {"RotationEulerDegrees", {}},
        {"Scale",                {}}
    };
    Serialize(tree["Position"],             t.Position);
    Serialize(tree["RotationEulerDegrees"], t.RotationEulerDegrees);
    Serialize(tree["Scale"],                t.Scale);
}

inline bool Deserialize(const PropertyTree &tree, Transform &t)
{
    if (!tree.IsObject()) return false;
    // All fields optional - keeps defaults if missing.
    if (tree.Contains("Position"))             Deserialize(tree["Position"],             t.Position);
    if (tree.Contains("RotationEulerDegrees")) Deserialize(tree["RotationEulerDegrees"], t.RotationEulerDegrees);
    if (tree.Contains("Scale"))                Deserialize(tree["Scale"],                t.Scale);
    return true;
}

} // namespace Serialization
```

### 4.3 Domain Type Example - DirectionalLight

> *This section shows the planned trait implementation for Phase 2 scene serialization.
> Traits for `DirectionalLight` do not exist yet.*

```cpp
namespace Serialization {

inline void Serialize(PropertyTree &tree, const DirectionalLight &light)
{
    tree = PropertyTree::Object{
        {"Direction", {}},
        {"Intensity", {}},
        {"Color",     {}}
    };
    Serialize(tree["Direction"], light.Direction);
    Serialize(tree["Intensity"], light.Intensity);
    Serialize(tree["Color"],     light.Color);
    // Padding is a GPU alignment detail - not serialized.
}

inline bool Deserialize(const PropertyTree &tree, DirectionalLight &light)
{
    if (!tree.IsObject()) return false;
    if (tree.Contains("Direction")) Deserialize(tree["Direction"], light.Direction);
    if (tree.Contains("Intensity")) Deserialize(tree["Intensity"], light.Intensity);
    if (tree.Contains("Color"))     Deserialize(tree["Color"],     light.Color);
    return true;
}

} // namespace Serialization
```

---

## 5. Layer 2 - Format Backends

Each backend converts between `PropertyTree` and a specific on-disk format.
All backends implement a common interface:

```cpp
/// @file Serialization/IFormatBackend.h

namespace Serialization {

class IFormatBackend
{
public:
    virtual ~IFormatBackend() = default;

    /// Serialize a PropertyTree to a string (for text formats) or byte buffer.
    virtual std::string WriteToString(const PropertyTree &tree) const = 0;

    /// Deserialize from a string/buffer into a PropertyTree.
    /// Returns false on parse error; details logged via LOG_ERROR.
    virtual bool ReadFromString(const std::string &data, PropertyTree &tree) const = 0;
};

} // namespace Serialization
```

### 5.1 JSON Backend

Wraps nlohmann/json. Currently the only backend.

```cpp
/// @file core/serialization/JsonBackend.h

namespace Serialization {

class JsonBackend : public IFormatBackend
{
public:
    explicit JsonBackend(int indent = 2) : m_Indent(indent) {}

    std::string WriteToString(const PropertyTree &tree) const override;
    bool ReadFromString(const std::string &data, PropertyTree &tree) const override;

private:
    int m_Indent;
};

} // namespace Serialization
```

Internal conversion helpers (`TreeToJson`, `JsonToTree`) are file-local functions
in an anonymous namespace inside `JsonBackend.cpp`. They use `std::visit` on
`PropertyTree::GetValue()` for the tree→json direction, and a `switch` on
`nlohmann::json::value_t` for the json→tree direction.

`#include <json.hpp>` appears only in `JsonBackend.cpp` — nowhere else in the engine.
This is the **sole point of coupling** to nlohmann/json.

### 5.2 Future Backends

> *None of the following backends are implemented. Add them only when there is a concrete
> use case. Adding a backend requires zero changes to domain types or traits.*

| Backend | Format | Use Case | Dependency |
|---------|--------|----------|------------|
| `YamlBackend` | YAML | Human-authored config files (more readable than JSON for large configs) | yaml-cpp or rapidyaml |
| `BinaryBackend` | Custom binary | Scene snapshots, fast save/load, asset packaging | None (custom format) |
| `MessagePackBackend` | MessagePack | Network serialization, compact on-disk | nlohmann/json (built-in support) |

---

## 6. Layer 3 - Schema Validation

> *This layer is not yet implemented (Phase 3). The interface below is the planned design.*

Optional layer that validates a `PropertyTree` before deserialization.
Useful for user-editable config files where typos are likely.

```cpp
/// @file Serialization/SchemaValidator.h

namespace Serialization {

struct SchemaField
{
    std::string Name;
    enum Type { Null, Bool, Int, Float, String, Array, Object } Expected;
    bool Required = false;
};

struct SchemaResult
{
    bool Valid = true;
    std::vector<std::string> Warnings;  // unknown keys, type coercions
    std::vector<std::string> Errors;    // missing required fields, wrong types
};

/// Validate a PropertyTree against a flat list of expected fields.
/// Does NOT reject unknown keys - only warns (forward-compatible).
SchemaResult Validate(const PropertyTree &tree,
                      std::span<const SchemaField> schema);

} // namespace Serialization
```

**Design choice**: Schema is **whitelist + warn**, not **strict reject**. Unknown keys produce
warnings, not errors. This keeps config files forward-compatible — an older engine version can
load a newer config without crashing.

---

## 7. Layer 4 - File I/O Integration

A convenience layer that ties together the filesystem, format backends, and traits.
See `core/serialization/Serialization.h` — all template functions, header-only.

```cpp
namespace Serialization {

/// Get the default backend for a file extension (currently always returns JsonBackend).
const IFormatBackend &GetBackendForExtension(std::string_view ext);

/// One-call save/load with auto-detected backend.
template <Serializable T>
bool SaveToFile(const T &value, const std::filesystem::path &path);

template <Serializable T>
bool LoadFromFile(T &value, const std::filesystem::path &path);

/// Overloads that accept an explicit backend.
template <Serializable T>
bool SaveToFile(const T &value, const std::filesystem::path &path,
                const IFormatBackend &backend);

template <Serializable T>
bool LoadFromFile(T &value, const std::filesystem::path &path,
                  const IFormatBackend &backend);

} // namespace Serialization
```

**Implementation details**:
- `SaveToFile` creates parent directories via `std::filesystem::create_directories`.
- `LoadFromFile` deserializes into a **temporary** `T`, then moves on success —
  the caller's value is unchanged if deserialization fails.

### Live usage (ShadowMapping.cpp, MaterialPlayground.cpp):

```cpp
#include "core/input/InputActionSerialization.h"
#include "core/serialization/Serialization.h"

// OnAttach()
constexpr auto kInputCfg = "input/ShadowMapping.json";
if (Serialization::LoadFromConfigPath(m_InputMap, kInputCfg))
{
    // loaded successfully
}
else
{
    m_InputMap.BindAction("ShowFinalColor", Key::D1);
    // ... other defaults ...
    Serialization::SaveToConfigPath(m_InputMap, kInputCfg);
}
```

---

## 8. InputActionMap Integration

`InputActionMap` is the first type migrated to the framework. The original
`InputAction.cpp` contained ~170 lines of inline `nlohmann/json` parsing inside
`SaveToFile()` and `LoadFromFile()` member functions. These have been replaced by
a dedicated trait file.

**Changes made**:

| File | Change |
|------|--------|
| `InputAction.h` | Removed `SaveToFile()` / `LoadFromFile()` declarations. Moved `AxisEntry` to public. Added `GetActions()` / `GetAxes()` const accessors for trait access. |
| `InputAction.cpp` | Removed ~170 lines of inline JSON serialization, anonymous namespace helpers, `<json.hpp>` include. |
| `InputActionSerialization.h` | New file — `Serialize` / `Deserialize` traits for `InputSource` and `InputActionMap`. |
| `ShadowMapping.cpp` | Calls `Serialization::LoadFromFile` / `SaveToFile` instead of member functions. |
| `MaterialPlayground.cpp` | Same migration as ShadowMapping. |

The JSON format is **unchanged** — existing config files load without modification.

### 8.1 Enum Strategy

`InputActionSerialization.h` contains two enum types with different token strategies:

- **`InputSource::Type` and `InputActionMap::MouseAxis`** use the default `magic_enum`-backed
  enum traits. Tokens match the C++ enumerator names exactly (`"Key"`, `"MouseButton"`,
  `"X"`, `"Y"`, `"ScrollY"`).

- **`Key::Code` and `Mouse::Code`** are not `enum class` types — they are `uint16_t` enums
  in a namespace and continue to use `InputNames.h` for stable canonical names and alias support.

This distinction matters: magic_enum tokens are tied to C++ enumerator spellings and will
silently change if an enumerator is renamed. Types where on-disk tokens must remain stable
across C++ renames get an explicit custom overload instead.

### 8.2 magic_enum Rollout Sequence

`magic_enum` v0.9.7 is vendored as a single header (`vendor/magic_enum/magic_enum.hpp`).
The enum support lives inside `BuiltinTraits.h` as a constrained template — the rest of the
codebase does **not** `#include <magic_enum.hpp>` directly.

| Wave | Scope | Status |
|------|-------|--------|
| 1 | Dependency added + enum trait in `BuiltinTraits.h` | ✅ Complete |
| 2 | First production use: `InputSource::Type`, `MouseAxis` in `InputActionSerialization.h` | ✅ Complete |
| 3 | Engine/editor-facing enums: `SceneRendererOutput`, renderer debug toggles, app/window settings once they become config-facing or need repeated UI stringification | Planned |
| 4 | Material and rendering enums: `ShadingModel`, `BlendMode`, and similar config-facing enums once material presets/editor tooling land. Flags-style enums like `MaterialFeatureFlag` need a dedicated serializer/UI helper rather than plain one-of treatment. | Planned |

**Do not force adoption where it is a poor fit**: keep backend conversion switches
(`TextureFormat → GLenum`, `LoadAction → MTLLoadAction`, etc.) explicit, and keep
non-enum code tables and alias-heavy input code mappings explicit.

---

## 9. Candidate Types for Future Serialization

Types that will likely need serialization, in rough priority order:

| Type | Use Case | Complexity | Status |
|------|----------|------------|--------|
| `InputActionMap` | Input config files | Medium — enum mapping, nested structure | ✅ Implemented |
| `Transform` | Scene save/load | Low — 3 vec3 fields | Phase 2 |
| `DirectionalLight` | Scene save/load | Low — vec3 + float | Phase 2 |
| `Camera` | Camera presets, scene save | Low — position + angles + projection params | Phase 2 |
| `SceneData` | Full scene snapshot | Medium — contains RenderItems with Ref<> pointers | Phase 2 |
| `Material` (properties only) | Material presets | Medium — heterogeneous property maps | — |
| Renderer settings | Quality presets, debug toggles | Low — flat key-value | — |
| Window/app settings | Window size, VSync, fullscreen | Low — flat key-value | — |
| Shader reflection sidecar | Loading Slang-emitted uniform/layout metadata | Medium — external schema, backend/tooling-owned | Deferred on purpose |

**Note on `Ref<Mesh>` / `Ref<ITexture2D>`**: These are runtime GPU resources — they cannot be
serialized as data. Instead, serialize an **asset reference** (path string) and resolve it
through the asset system on load. This is out of scope for Phase 2 but the PropertyTree
design accommodates it naturally (store `"mesh": "meshes/cube.obj"` as a string).

**Note on enums**: For future config-facing enums, the default path is to use the shared
`magic_enum`-backed enum trait so every backend gets the same token mapping. If a token
must remain stable across C++ renames, add an explicit per-type serializer instead of
serializing raw enumerator spellings.

### 9.1 Shader Reflection Sidecar - Deferred Integration Plan

The repository has a serialization framework, but shader reflection sidecars are
currently **not** routed through it. `MetalShader` still parses Slang's reflection JSON
directly with `nlohmann::json`.

That is a deliberate short-term exception, not the desired long-term pattern.

#### Why This Path Is Still Special Today

Reflection sidecars differ from the first wave of serialized engine/domain data:

- they are generated by tooling, not authored by users
- their schema is currently dictated by Slang output plus local loader assumptions
- the loader lives on a hot renderer path where correctness and low ambiguity matter
- the schema is still evolving as reflected packing is being proven out

In other words, this data is closer to a compiler artifact than to a user config file.
That makes it a weaker fit for immediate serialization-framework adoption than
`InputActionMap`, scene config, or future material preset files.

#### Why It Still Belongs In The Serialization Story Eventually

Even with that caveat, there are good long-term reasons to move it:

- keep `json.hpp` usage concentrated in `JsonBackend`
- avoid hand-written schema walking in renderer/backend code
- make reflection loading share the same parse/error/validation structure as other data
- enable future schema validation/versioning without re-implementing those concerns
- make tests easier by decoding sidecars into engine-owned intermediate structures

#### Recommended Future Shape

When the team decides to focus this area, prefer the following layering:

1. File read:
   - `FileSystem::ReadText()` for logical resource paths
   - `Resource::ReadTextFile()` only for low-level physical-path bridges

2. Format decode:
   - `JsonBackend::ReadFromString()` into `PropertyTree`

3. Reflection decode:
   - explicit `Deserialize(const PropertyTree&, ShaderReflectionArtifact&)`
   - or a narrow loader helper that converts `PropertyTree` into:
     - shader-level metadata
     - `ShaderUniformBlockLayout`
     - per-stage binding maps

4. Runtime validation:
   - validate required fields like `name`, `binding.kind`, `offset`, `size`, `type`
   - reject malformed or partially understood schemas loudly

Important: this should still be **explicit decoding**, not generic "serialize every JSON
object automatically" machinery. The reflection schema is external and semantics-heavy;
it deserves a dedicated decode step.

#### Best Time To Focus This

- not now, while reflected packing issues are still being shaken out
- earliest reasonable time: after reflected packing works on both Vulkan and Metal
- preferred time: after the shader/block API work is stable enough that sidecar schema
  changes are infrequent
- good practical trigger: when the team wants to remove the last direct `nlohmann::json`
  parsing sites from engine code as a cleanup pass

---

## 10. Error Handling Strategy

Consistent across all types — enforced by the framework, not by individual implementations:

| Situation | Behavior |
|-----------|----------|
| File not found | `LoadFromFile` returns `false`, no log (caller decides if this is expected) |
| Parse error (malformed JSON/YAML) | `LOG_ERROR` with file path and parser message, return `false` |
| Unknown key in config | `LOG_WARN` (forward-compatible — don't reject) |
| Missing optional field | Use type's default value silently |
| Missing required field | `LOG_WARN`, `Deserialize` returns `false` |
| Type mismatch (e.g., string where int expected) | `LOG_WARN` with field name, skip field |
| Partial failure | Deserialize into temporaries, only commit on full success |
| Write failure (disk full, permissions) | `LOG_ERROR`, `SaveToFile` returns `false` |

---

## 11. File Layout

```
vendor/
  magic_enum/
    magic_enum.hpp                 ✅ v0.9.7, vendored single header

src/
  core/
    serialization/
      PropertyTree.h / .cpp        ✅ Layer 0: intermediate representation
      SerializationTraits.h        ✅ Layer 1: Serializable concept
      BuiltinTraits.h              ✅ Layer 1: primitives, glm, enum, containers (header-only)
      IFormatBackend.h             ✅ Layer 2: backend interface
      JsonBackend.h / .cpp         ✅ Layer 2: nlohmann/json backend
      Serialization.h              ✅ Layer 4: convenience SaveToFile/LoadFromFile (header-only)
      SchemaValidator.h / .cpp        Layer 3: optional validation            (Phase 3)
    input/
      InputActionSerialization.h   ✅ Serialize/Deserialize for InputActionMap
  scene/
    SceneSerialization.h              Serialize/Deserialize for Transform, Light, Camera (Phase 2)
```

The `serialization/` module lives under `core/` because it is engine infrastructure,
not tied to any specific domain (input, scene, graphics).

`BuiltinTraits.h` and `Serialization.h` are fully header-only — all functions are
`inline` or templates. No `.cpp` files are needed for them.

---

## 12. Implementation Roadmap

### Phase 2 - Scene types

**Goal**: Serialize Transform, DirectionalLight, Camera for scene save/load.

| Step | Deliverable |
|------|-------------|
| 2a | Traits for Transform, DirectionalLight, Camera in `scene/SceneSerialization.h` |
| 2b | Scene config files (e.g., `configs/scenes/ShadowMapping.json`) |
| 2c | Demo code loads scene setup from config instead of hard-coded defaults |

### Phase 3 - Schema validation

**Goal**: User-editable config files get friendly error messages.

| Step | Deliverable |
|------|-------------|
| 3a | `SchemaValidator` implementation (see §6 for interface design) |
| 3b | Schema definitions for input configs and scene configs |
| 3c | Validation integrated into `LoadFromFile` pipeline |

### Phase 4 - Additional backends (as needed)

Only implement when there is a concrete use case:
- **Binary**: When scene save/load performance matters (large scenes).
- **YAML**: When config files become complex enough that JSON readability is a bottleneck.
- **MessagePack**: When network serialization is needed.

---

## 13. Appendix A: Alternatives Considered

### A1. nlohmann/json `from_json` / `to_json` ADL directly

nlohmann/json natively supports ADL-based `from_json` / `to_json` free functions.

**Pros**: Zero framework code needed — just write the functions.
**Cons**: Couples every serializable type to nlohmann/json. No path to YAML or binary without
rewriting all traits. nlohmann types leak into domain headers.

**Verdict**: Good for JSON-only projects. We want format independence.

### A2. Inheritance-based `ISerializable` interface

```cpp
class ISerializable {
    virtual void Serialize(PropertyTree &tree) const = 0;
    virtual bool Deserialize(const PropertyTree &tree) = 0;
};
```

**Pros**: Familiar OOP pattern.
**Cons**: Forces inheritance on POD structs like Transform and DirectionalLight. Cannot add
serialization to types you don't own. Virtual dispatch overhead for what is fundamentally a
static operation.

**Verdict**: Too invasive for this codebase's style (structs are POD, classes use composition).

### A3. Macro-based reflection (e.g., `SERIALIZE_FIELDS(Position, Rotation, Scale)`)

**Pros**: Extremely concise declarations.
**Cons**: Macro magic is hard to debug, breaks IDE tooling, and doesn't compose with templates.
Requires significant infrastructure (field name stringification, iteration).

**Verdict**: Not worth the complexity at current scale. Revisit if the engine reaches 50+ serializable types.

### A4. C++20 compile-time reflection (future standard)

**Pros**: Zero-overhead, no macros, no boilerplate.
**Cons**: Not available in any compiler yet (C++26 at earliest). Cannot depend on it.

**Verdict**: The trait-based design is forward-compatible — when reflection lands, traits can
be auto-generated. No wasted work. In the meantime, `magic_enum` is adopted only as a
small helper for named enum token conversion; it does not replace explicit trait code or
change the overall architecture.
