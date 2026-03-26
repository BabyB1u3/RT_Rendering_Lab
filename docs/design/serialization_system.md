# Serialization System — Design Document

Updated 2026-03-26. Describes a format-agnostic serialization framework that decouples
data types from their on-disk representation, enabling JSON today and YAML / binary tomorrow.

> **Status**: Phase 1 complete — framework implemented, InputActionMap migrated.

> **Design Philosophy**: Types declare *what* to serialize via lightweight trait specialization.
> Format backends (JSON, YAML, binary) handle *how*. No macros, no inheritance tax, no runtime
> type registration. Serialize into an intermediate `PropertyTree` — backends read/write that tree.
> Named enum types may use `magic_enum` inside trait helpers for token conversion, but field-level
> serialization remains explicit trait code rather than generalized structural reflection.

---

## Table of Contents

- [Serialization System — Design Document](#serialization-system--design-document)
  - [Table of Contents](#table-of-contents)
  - [1. Motivation](#1-motivation)
  - [2. Architecture Overview](#2-architecture-overview)
  - [3. Layer 0 — PropertyTree (Intermediate Representation)](#3-layer-0--propertytree-intermediate-representation)
  - [4. Layer 1 — Serialization Traits](#4-layer-1--serialization-traits)
    - [4.1 Built-in Trait Specializations](#41-built-in-trait-specializations)
    - [4.2 Domain Type Example — Transform](#42-domain-type-example--transform)
    - [4.3 Domain Type Example — DirectionalLight](#43-domain-type-example--directionallight)
  - [5. Layer 2 — Format Backends](#5-layer-2--format-backends)
    - [5.1 JSON Backend (Phase 1 — immediate)](#51-json-backend-phase-1--immediate)
    - [5.2 Future Backends (not implemented now)](#52-future-backends-not-implemented-now)
  - [6. Layer 3 — Schema Validation](#6-layer-3--schema-validation)
  - [7. Layer 4 — File I/O Integration](#7-layer-4--file-io-integration)
    - [Usage in demo code (after migration):](#usage-in-demo-code-after-migration)
  - [8. Migration Plan — InputActionMap](#8-migration-plan--inputactionmap)
    - [Before (current)](#before-current)
    - [After](#after)
    - [Migration steps](#migration-steps)
  - [9. Candidate Types for Future Serialization](#9-candidate-types-for-future-serialization)
  - [10. Error Handling Strategy](#10-error-handling-strategy)
  - [11. File Layout](#11-file-layout)
  - [12. Phased Implementation Plan](#12-phased-implementation-plan)
    - [Phase 1 — Foundation + InputActionMap migration](#phase-1--foundation--inputactionmap-migration)
    - [Phase 2 — Scene types](#phase-2--scene-types)
    - [Phase 3 — Schema validation](#phase-3--schema-validation)
    - [Phase 4 — Additional backends (as needed)](#phase-4--additional-backends-as-needed)
  - [13. Appendix A: Alternatives Considered](#13-appendix-a-alternatives-considered)
    - [A1. nlohmann/json `from_json` / `to_json` ADL directly](#a1-nlohmannjson-from_json--to_json-adl-directly)
    - [A2. Inheritance-based `ISerializable` interface](#a2-inheritance-based-iserializable-interface)
    - [A3. Macro-based reflection (e.g., `SERIALIZE_FIELDS(Position, Rotation, Scale)`)](#a3-macro-based-reflection-eg-serialize_fieldsposition-rotation-scale)
    - [A4. C++20 compile-time reflection (future standard)](#a4-c20-compile-time-reflection-future-standard)

---

## 1. Motivation

~~Today, `InputActionMap::SaveToFile()` / `LoadFromFile()` contain ~170 lines of inline JSON
serialization using nlohmann/json directly.~~ *(Resolved in Phase 1 — see §8.)*
The original inline approach could not scale:

| Problem | Impact |
|---------|--------|
| JSON logic lives inside domain types | Every new serializable type duplicates file I/O, error handling, and format code |
| No shared validation strategy | Each consumer re-invents `.contains()` / `.is_object()` checks |
| Hard-coded to JSON | Adding YAML (human-authored configs) or binary (scene snapshots) requires rewriting every type |
| No versioning | Changing a field name silently breaks old save files |

**Goal**: A thin abstraction layer where:
- Domain types declare serialization via a **trait** (free function pair).
- An **intermediate tree** decouples types from format details.
- **Format backends** are interchangeable — JSON today, binary/YAML when needed.
- **Validation and versioning** live in one place, not scattered across LoadFromFile() calls.

---

## 2. Architecture Overview

```
   Domain types                    Intermediate             Format backends
 ┌──────────────┐               ┌──────────────┐         ┌───────────────┐
 │  Transform   │──Serialize──▸ │              │──JSON──▸│ JsonBackend   │──▸ .json file
 │  Material    │               │ PropertyTree │──YAML──▸│ YamlBackend   │──▸ .yaml file
 │  Camera      │◂─Deserialize─│              │──Bin───▸│ BinaryBackend │──▸ .bin  file
 │  InputAction │               └──────────────┘         └───────────────┘
 └──────────────┘                      ▲
                                       │
                               ┌───────┴───────┐
                               │ SchemaValidator│ (optional, Layer 3)
                               └───────────────┘
```

Data flows in two directions:
- **Save**: `Type` → trait `Serialize()` → `PropertyTree` → backend `Write()` → file
- **Load**: file → backend `Read()` → `PropertyTree` → (validate) → trait `Deserialize()` → `Type`

---

## 3. Layer 0 — PropertyTree (Intermediate Representation)

A lightweight, format-neutral value tree. Analogous to `nlohmann::json` in shape but
**owned by us** — no third-party types leak into domain headers.

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

    // Construction — implicit conversions for ergonomic building
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
- `IsNumber()` returns true for either Int or Float — simplifies numeric deserialization.
- `operator[](const std::string&)` on a Null tree auto-promotes to Object for ergonomic building.
- No `glm::vec3` etc. in the tree — those are composed from arrays in the trait layer.
- Variant-based, no heap allocation for scalars.

---

## 4. Layer 1 — Serialization Traits

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

Provided out of the box in `core/serialization/BuiltinTraits.h`. All are `inline`
functions (header-only — no `.cpp` needed):

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

### 4.2 Domain Type Example — Transform

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
    // All fields optional — keeps defaults if missing.
    if (tree.Contains("Position"))             Deserialize(tree["Position"],             t.Position);
    if (tree.Contains("RotationEulerDegrees")) Deserialize(tree["RotationEulerDegrees"], t.RotationEulerDegrees);
    if (tree.Contains("Scale"))                Deserialize(tree["Scale"],                t.Scale);
    return true;
}

} // namespace Serialization
```

### 4.3 Domain Type Example — DirectionalLight

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
    // Padding is a GPU alignment detail — not serialized.
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

## 5. Layer 2 — Format Backends

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

### 5.1 JSON Backend ✅

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
`nlohmann::json::value_t` for json→tree.

`#include <json.hpp>` only appears in `JsonBackend.cpp` — nowhere else in the engine.
This is the **sole point of coupling** to nlohmann/json.

### 5.2 Future Backends (not implemented now)

| Backend | Format | Use Case | Dependency |
|---------|--------|----------|------------|
| `YamlBackend` | YAML | Human-authored config files (more readable than JSON for large configs) | yaml-cpp or rapidyaml |
| `BinaryBackend` | Custom binary | Scene snapshots, fast save/load, asset packaging | None (custom format) |
| `MessagePackBackend` | MessagePack | Network serialization, compact on-disk | nlohmann/json (built-in support) |

Adding a backend requires **zero changes** to domain types or traits.

---

## 6. Layer 3 — Schema Validation

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
/// Does NOT reject unknown keys — only warns (forward-compatible).
SchemaResult Validate(const PropertyTree &tree,
                      std::span<const SchemaField> schema);

} // namespace Serialization
```

**Design choice**: Schema is **whitelist + warn**, not **strict reject**. Unknown keys produce
warnings, not errors. This keeps config files forward-compatible — an older engine version can
load a newer config without crashing.

---

## 7. Layer 4 — File I/O Integration ✅

A convenience layer that ties together FileSystem, format backends, and traits.
See `core/serialization/Serialization.h` — all template functions, header-only.

```cpp
namespace Serialization {

/// Get the default backend for a file extension (Phase 1: always returns JsonBackend).
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

### Usage in demo code (live — ShadowMapping.cpp, MaterialPlayground.cpp):

```cpp
#include "core/input/InputActionSerialization.h"
#include "core/serialization/Serialization.h"

// OnAttach()
constexpr auto kInputCfg = "input/ShadowMapping.json";
auto resolved = FileSystem::ResolveConfigPath(kInputCfg);
if (!resolved.empty() && Serialization::LoadFromFile(m_InputMap, resolved))
{
    // loaded successfully
}
else
{
    m_InputMap.BindAction("ShowFinalColor", Key::D1);
    // ... other defaults ...
    Serialization::SaveToFile(m_InputMap, FileSystem::GetSavedConfigPath(kInputCfg));
}
```

---

## 8. Migration Plan — InputActionMap ✅ Complete

The first consumer migrated. Validated the framework on a real, working type.

### Changes made

| File | Change |
|------|--------|
| `InputAction.h` | Removed `SaveToFile()` / `LoadFromFile()` declarations. Moved `AxisEntry` to public. Added `GetActions()` / `GetAxes()` const accessors for trait access. |
| `InputAction.cpp` | Removed ~170 lines of inline JSON serialization, anonymous namespace helpers, `<json.hpp>` include. |
| `InputActionSerialization.h` | New file — `Serialize` / `Deserialize` traits for `InputSource` and `InputActionMap`. |
| `ShadowMapping.cpp` | Calls `Serialization::LoadFromFile` / `SaveToFile` instead of member functions. |
| `MaterialPlayground.cpp` | Same migration as ShadowMapping. |

### Enum strategy (as implemented)

- `InputSource::Type` and `InputActionMap::MouseAxis` use the default `magic_enum`-backed
  enum traits — tokens match the C++ enumerator names exactly (`"Key"`, `"MouseButton"`,
  `"X"`, `"Y"`, `"ScrollY"`).
- `Key::Code` and `Mouse::Code` are **not** `enum class` types (they are `uint16_t` enums
  in a namespace) — they continue to use `InputNames.h` for stable canonical names and alias support.

**JSON format is unchanged** — existing config files load without modification.

### `magic_enum` Rollout Sequence

1. ✅ **Dependency + enum trait in BuiltinTraits.h**
   - Added `magic_enum` v0.9.7 as a vendored single header (`vendor/magic_enum/magic_enum.hpp`).
   - Enum support lives inside `BuiltinTraits.h` as a constrained template — the rest of the
     codebase does **not** `#include <magic_enum.hpp>` directly.

2. ✅ **First production use: InputActionMap serialization traits**
   - `InputSource::Type` and `MouseAxis` use the magic_enum-backed enum traits in
     `InputActionSerialization.h`.
   - `Key::Code` / `Mouse::Code` remain on `InputNames.h` — canonical names and aliases are stable.

3. **Second wave: engine/editor-facing enums**
   - Migrate enums such as `SceneRendererOutput`, renderer debug toggles, and future app/window
     settings once they become config-facing or need repeated UI stringification.
   - Typical uses: serialization tokens, log strings, ImGui combo/radio population.

4. **Third wave: material and rendering configuration enums**
   - Apply the same shared helper to future `ShadingModel`, `BlendMode`, and similar config-facing
     enums once material presets/editor tooling land.
   - For flags-style enums such as `MaterialFeatureFlag`, build a dedicated serializer/UI helper
     on top of the enum metadata rather than treating them like a plain one-of enum.

5. **Do not force adoption where it is a poor fit**
   - Keep backend conversion switches (`TextureFormat -> GLenum`, `LoadAction -> MTLLoadAction`,
     etc.) explicit.
   - Keep non-enum code tables and alias-heavy input code mappings explicit.

This ordering deliberately introduces the dependency **before** widespread usage, but delays
meaningful behavioral changes until the `InputActionMap` serialization migration is underway.

---

## 9. Candidate Types for Future Serialization

Types that will likely need serialization, in rough priority order:

| Type | Use Case | Complexity | Status |
|------|----------|------------|--------|
| `InputActionMap` | Input config files | Medium — enum mapping, nested structure | ✅ Phase 1 |
| `Transform` | Scene save/load | Low — 3 vec3 fields | Phase 2 |
| `DirectionalLight` | Scene save/load | Low — vec3 + float | Phase 2 |
| `Camera` | Camera presets, scene save | Low — position + angles + projection params | Phase 2 |
| `SceneData` | Full scene snapshot | Medium — contains RenderItems with Ref<> pointers | Phase 2 |
| `Material` (properties only) | Material presets | Medium — heterogeneous property maps | — |
| Renderer settings | Quality presets, debug toggles | Low — flat key-value | — |
| Window/app settings | Window size, VSync, fullscreen | Low — flat key-value | — |

**Note on `Ref<Mesh>` / `Ref<ITexture2D>`**: These are runtime GPU resources — they cannot be
serialized as data. Instead, serialize an **asset reference** (path string) and resolve it
through the asset system on load. This is out of scope for Phase 1 but the PropertyTree
design accommodates it naturally (store `"mesh": "meshes/cube.obj"` as a string).

**Note on enums**: For future config-facing enums, the default path is to use the shared
`magic_enum`-backed enum trait so every backend gets the same token mapping. If a token
must remain stable across C++ renames, add an explicit per-type serializer instead of
serializing raw enumerator spellings.

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
| Partial failure | Deserialize into temporaries, only commit on full success (same as current InputActionMap pattern) |
| Write failure (disk full, permissions) | `LOG_ERROR`, `SaveToFile` returns `false` |

---

## 11. File Layout

```
vendor/
  magic_enum/
    magic_enum.hpp                 — v0.9.7, vendored single header

src/
  core/
    serialization/
      PropertyTree.h / .cpp        — Layer 0: intermediate representation    ✅
      SerializationTraits.h        — Layer 1: Serializable concept           ✅
      BuiltinTraits.h              — Layer 1: primitives, glm, enum, containers (header-only) ✅
      IFormatBackend.h             — Layer 2: backend interface              ✅
      JsonBackend.h / .cpp         — Layer 2: nlohmann/json backend          ✅
      Serialization.h              — Layer 4: convenience SaveToFile/LoadFromFile (header-only) ✅
      SchemaValidator.h / .cpp     — Layer 3: optional validation            (Phase 3)
    input/
      InputActionSerialization.h   — Serialize/Deserialize for InputActionMap ✅
  scene/
    SceneSerialization.h           — Serialize/Deserialize for Transform, Light, Camera (Phase 2)
```

The `serialization/` module lives under `core/` because it is engine infrastructure,
not tied to any specific domain (input, scene, graphics).

Note: `BuiltinTraits.h` and `Serialization.h` are header-only (no `.cpp`) — all functions
are `inline` or templates.

---

## 12. Phased Implementation Plan

### Phase 1 — Foundation + InputActionMap migration ✅ Complete (2026-03-26)

**Goal**: Replace inline JSON code in InputActionMap with the new framework.
Zero behavior change, zero config format change.

| Step | Deliverable | Test | Status |
|------|-------------|------|--------|
| 0a | Add `magic_enum` v0.9.7 vendored + enum traits in `BuiltinTraits.h` | Build succeeds; no behavior changes | ✅ |
| 0b | Enum usage rules: `enum class` → magic_enum default, `Key::Code`/`Mouse::Code` → InputNames.h | Design doc updated | ✅ |
| 1a | `PropertyTree` with full variant API (`std::map` for ordered output) | Build succeeds | ✅ |
| 1b | `JsonBackend` — bidirectional conversion via anonymous-namespace helpers | Build succeeds | ✅ |
| 1c | Builtin traits: primitives, `uint8/16_t`, `glm::vec2/3/4`, `mat4`, containers, enums | Build succeeds | ✅ |
| 1d | `InputActionMap` Serialize/Deserialize traits in `InputActionSerialization.h` | Build succeeds, 84 existing tests pass | ✅ |
| 1e | `Serialization::SaveToFile` / `LoadFromFile` (header-only, temp-then-move pattern) | Build succeeds | ✅ |
| 1f | Remove old code from InputAction.cpp, migrate ShadowMapping + MaterialPlayground | All demos compile, 84 tests pass | ✅ |

**Implementation notes vs. original design**:
- `PropertyTree::Object` uses `std::map` instead of `std::unordered_map` for deterministic key ordering in JSON output.
- Added `IsNumber()`, mutable `AsArray()`/`AsObject()`, `GetValue()` accessor not in original spec.
- Added `glm::vec2`, `uint8_t`, `uint16_t` to built-in traits beyond original spec.
- `BuiltinTraits.h` and `Serialization.h` are fully header-only (no `.cpp` files needed).
- `JsonBackend` conversion helpers are anonymous-namespace functions, not static members.

### Phase 2 — Scene types

**Goal**: Serialize Transform, DirectionalLight, Camera for scene save/load.

| Step | Deliverable |
|------|-------------|
| 2a | Traits for Transform, DirectionalLight, Camera |
| 2b | Scene config files (e.g., `configs/scenes/ShadowMapping.json`) |
| 2c | Demo code loads scene setup from config |

### Phase 3 — Schema validation

**Goal**: User-editable config files get friendly error messages.

| Step | Deliverable |
|------|-------------|
| 3a | `SchemaValidator` implementation |
| 3b | Schema definitions for input configs and scene configs |
| 3c | Validation integrated into `LoadFromFile` pipeline |

### Phase 4 — Additional backends (as needed)

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
