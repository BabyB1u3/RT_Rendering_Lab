# Serialization System — Design Document

Updated 2026-03-24. Describes a format-agnostic serialization framework that decouples
data types from their on-disk representation, enabling JSON today and YAML / binary tomorrow.

> **Design Philosophy**: Types declare *what* to serialize via lightweight trait specialization.
> Format backends (JSON, YAML, binary) handle *how*. No macros, no inheritance tax, no runtime
> type registration. Serialize into an intermediate `PropertyTree` — backends read/write that tree.

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

Today, `InputActionMap::SaveToFile()` / `LoadFromFile()` contain ~170 lines of inline JSON
serialization using nlohmann/json directly. This works, but cannot scale:

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

```cpp
/// @file Serialization/PropertyTree.h

#include <string>
#include <vector>
#include <unordered_map>
#include <variant>
#include <optional>
#include <cstdint>

namespace Serialization {

class PropertyTree
{
public:
    // Supported value types
    using Null    = std::monostate;
    using Bool    = bool;
    using Int     = int64_t;
    using Float   = double;
    using String  = std::string;
    using Array   = std::vector<PropertyTree>;
    using Object  = std::unordered_map<std::string, PropertyTree>;

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
    bool IsString() const;
    bool IsArray()  const;
    bool IsObject() const;

    // Typed access (throws std::bad_variant_access on type mismatch)
    Bool         AsBool()   const;
    Int          AsInt()    const;
    Float        AsFloat()  const;  // also accepts Int and promotes
    const String &AsString() const;
    const Array  &AsArray()  const;
    const Object &AsObject() const;

    // Object helpers
    bool Contains(const std::string &key) const;
    const PropertyTree &operator[](const std::string &key) const;
    PropertyTree &operator[](const std::string &key);

    /// Get value with fallback — never throws.
    template<typename T>
    T GetOr(const std::string &key, const T &fallback) const;

    // Array helpers
    size_t Size() const;  // Array or Object size
    const PropertyTree &operator[](size_t index) const;

private:
    Value m_Value;
};

} // namespace Serialization
```

**Key decisions**:
- `Float` is `double` internally — covers both `float` and `double` without loss.
- `Int` is `int64_t` — covers all integer types the project uses.
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

Provided out of the box for common types:

```cpp
/// @file Serialization/BuiltinTraits.h

namespace Serialization {

// --- Primitives (bool, int, float, double, string) ---
// Trivial: direct assignment to/from PropertyTree.

void Serialize(PropertyTree &tree, bool v);
void Serialize(PropertyTree &tree, int v);
void Serialize(PropertyTree &tree, float v);
void Serialize(PropertyTree &tree, double v);
void Serialize(PropertyTree &tree, const std::string &v);
// ... corresponding Deserialize overloads ...

// --- GLM types → JSON arrays ---

void Serialize(PropertyTree &tree, const glm::vec3 &v);
bool Deserialize(const PropertyTree &tree, glm::vec3 &v);
// Produces: [x, y, z]

void Serialize(PropertyTree &tree, const glm::vec4 &v);
bool Deserialize(const PropertyTree &tree, glm::vec4 &v);
// Produces: [x, y, z, w]

void Serialize(PropertyTree &tree, const glm::mat4 &v);
bool Deserialize(const PropertyTree &tree, glm::mat4 &v);
// Produces: array of 16 floats (column-major)

// --- std::vector<T> where T is Serializable ---

template<Serializable T>
void Serialize(PropertyTree &tree, const std::vector<T> &vec);

template<Serializable T>
bool Deserialize(const PropertyTree &tree, std::vector<T> &vec);

// --- std::unordered_map<std::string, T> where T is Serializable ---

template<Serializable T>
void Serialize(PropertyTree &tree, const std::unordered_map<std::string, T> &map);

template<Serializable T>
bool Deserialize(const PropertyTree &tree, std::unordered_map<std::string, T> &map);

// --- std::optional<T> ---

template<Serializable T>
void Serialize(PropertyTree &tree, const std::optional<T> &opt);

template<Serializable T>
bool Deserialize(const PropertyTree &tree, std::optional<T> &opt);

} // namespace Serialization
```

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

### 5.1 JSON Backend (Phase 1 — immediate)

Wraps nlohmann/json. This is the only backend needed initially.

```cpp
/// @file Serialization/JsonBackend.h

namespace Serialization {

class JsonBackend : public IFormatBackend
{
public:
    explicit JsonBackend(int indent = 2) : m_Indent(indent) {}

    std::string WriteToString(const PropertyTree &tree) const override;
    bool ReadFromString(const std::string &data, PropertyTree &tree) const override;

private:
    int m_Indent;

    // Internal conversion helpers
    static nlohmann::json TreeToJson(const PropertyTree &tree);
    static PropertyTree JsonToTree(const nlohmann::json &j);
};

} // namespace Serialization
```

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

## 7. Layer 4 — File I/O Integration

A convenience layer that ties together FileSystem, format backends, and traits:

```cpp
/// @file Serialization/Serialization.h

namespace Serialization {

/// Get the default backend for a file extension.
/// ".json" → JsonBackend, ".yaml" → YamlBackend, etc.
IFormatBackend &GetBackendForExtension(std::string_view ext);

/// One-call save: Serialize T → PropertyTree → format string → file.
/// Creates parent directories. Returns false on failure.
template<Serializable T>
bool SaveToFile(const T &value, const std::filesystem::path &path);

/// One-call load: file → format string → PropertyTree → Deserialize into T.
/// On failure, `value` is unchanged. Returns false on parse or validation error.
template<Serializable T>
bool LoadFromFile(T &value, const std::filesystem::path &path);

/// Overloads that accept an explicit backend (when extension doesn't match).
template<Serializable T>
bool SaveToFile(const T &value, const std::filesystem::path &path,
                const IFormatBackend &backend);

template<Serializable T>
bool LoadFromFile(T &value, const std::filesystem::path &path,
                  const IFormatBackend &backend);

} // namespace Serialization
```

### Usage in demo code (after migration):

```cpp
// ShadowMapping.cpp — OnAttach()
auto resolved = FileSystem::ResolveConfigPath("input/ShadowMapping.json");
if (!resolved.empty() && Serialization::LoadFromFile(m_InputMap, resolved))
{
    // loaded
}
else
{
    m_InputMap.BindAction("ToggleLookMode", Key::T);
    // ... other defaults ...
    Serialization::SaveToFile(m_InputMap,
        FileSystem::GetSavedConfigPath("input/ShadowMapping.json"));
}
```

Exactly the same call pattern as today, but the JSON logic is no longer inside `InputActionMap`.

---

## 8. Migration Plan — InputActionMap

The first consumer to migrate. This validates the framework on a real, working type.

### Before (current)

```
InputAction.h   — declares SaveToFile() / LoadFromFile()
InputAction.cpp — 170 lines of inline nlohmann/json code
```

### After

```
InputAction.h   — no serialization methods (removed)
InputAction.cpp — no JSON includes

Serialization/PropertyTree.h         — tree type
Serialization/JsonBackend.h/.cpp     — nlohmann wrapper
Serialization/Serialization.h        — SaveToFile / LoadFromFile templates
Serialization/BuiltinTraits.h        — vec3, string, vector, map traits

core/input/InputActionSerialization.h — Serialize/Deserialize for InputActionMap
```

### Migration steps

1. Implement PropertyTree + JsonBackend.
2. Write `Serialize(PropertyTree&, const InputActionMap&)` that produces the **same JSON structure** as the current `SaveToFile()`.
3. Write `Deserialize(const PropertyTree&, InputActionMap&)` with identical validation logic.
4. Add a round-trip test: load existing config → serialize → deserialize → compare.
5. Replace `InputActionMap::SaveToFile/LoadFromFile` with calls to `Serialization::SaveToFile/LoadFromFile`.
6. Remove the old inline JSON code and the `#include <json.hpp>` from InputAction.cpp.

**JSON format must not change** — existing config files must load without modification.

---

## 9. Candidate Types for Future Serialization

Types that will likely need serialization, in rough priority order:

| Type | Use Case | Complexity |
|------|----------|------------|
| `InputActionMap` | Input config files (Phase 1 — migration) | Medium — enum mapping, nested structure |
| `Transform` | Scene save/load | Low — 3 vec3 fields |
| `DirectionalLight` | Scene save/load | Low — vec3 + float |
| `Camera` | Camera presets, scene save | Low — position + angles + projection params |
| `SceneData` | Full scene snapshot | Medium — contains RenderItems with Ref<> pointers |
| `Material` (properties only) | Material presets | Medium — heterogeneous property maps |
| Renderer settings | Quality presets, debug toggles | Low — flat key-value |
| Window/app settings | Window size, VSync, fullscreen | Low — flat key-value |

**Note on `Ref<Mesh>` / `Ref<ITexture2D>`**: These are runtime GPU resources — they cannot be
serialized as data. Instead, serialize an **asset reference** (path string) and resolve it
through the asset system on load. This is out of scope for Phase 1 but the PropertyTree
design accommodates it naturally (store `"mesh": "meshes/cube.obj"` as a string).

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
src/
  core/
    serialization/
      PropertyTree.h / .cpp        — Layer 0: intermediate representation
      SerializationTraits.h        — Layer 1: Serializable concept
      BuiltinTraits.h / .cpp       — Layer 1: primitives, glm, std containers
      IFormatBackend.h             — Layer 2: backend interface
      JsonBackend.h / .cpp         — Layer 2: nlohmann/json backend
      SchemaValidator.h / .cpp     — Layer 3: optional validation
      Serialization.h              — Layer 4: convenience SaveToFile/LoadFromFile
    input/
      InputActionSerialization.h   — Serialize/Deserialize for InputActionMap
  scene/
    SceneSerialization.h           — Serialize/Deserialize for Transform, Light, Camera
```

The `serialization/` module lives under `core/` because it is engine infrastructure,
not tied to any specific domain (input, scene, graphics).

---

## 12. Phased Implementation Plan

### Phase 1 — Foundation + InputActionMap migration

**Goal**: Replace inline JSON code in InputActionMap with the new framework.
Zero behavior change, zero config format change.

| Step | Deliverable | Test |
|------|-------------|------|
| 1a | `PropertyTree` with full variant API | Unit tests: construct, query, access, GetOr |
| 1b | `JsonBackend` — bidirectional conversion | Round-trip test: `json string → tree → json string` preserves structure |
| 1c | Builtin traits for primitives + glm::vec3 | Unit test per type |
| 1d | `InputActionMap` Serialize/Deserialize traits | Load existing .json → serialize back → byte-identical output |
| 1e | `Serialization::SaveToFile` / `LoadFromFile` | Integration test with temp file |
| 1f | Remove old code from InputAction.cpp | All existing demos still load configs correctly |

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
be auto-generated. No wasted work.
