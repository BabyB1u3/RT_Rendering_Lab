# Serialization System Internals

This document describes how the serialization system actually works at the code level,
and compares RTRLab's approach with the serialization systems found in modern game
engines.

It is intended as a companion to the main
[Design.md](Design.md) design document, which focuses on the public contract and
design rationale. This document focuses on implementation mechanics and industry context.

> **Snapshot date**: 2026-04-09

---

## Table of Contents

- [Serialization System Internals](#serialization-system-internals)
  - [Table of Contents](#table-of-contents)
  - [Part I: How the System Works](#part-i-how-the-system-works)
    - [1. Architecture at a Glance](#1-architecture-at-a-glance)
    - [2. Layer 0: PropertyTree — The Intermediate Representation](#2-layer-0-propertytree--the-intermediate-representation)
      - [2.1 Variant Storage](#21-variant-storage)
      - [2.2 Implicit Constructors and Ergonomic Building](#22-implicit-constructors-and-ergonomic-building)
      - [2.3 Null Auto-Promotion](#23-null-auto-promotion)
      - [2.4 GetOr — Safe Lookup with Fallback](#24-getor--safe-lookup-with-fallback)
      - [2.5 Key Ordering and Determinism](#25-key-ordering-and-determinism)
    - [3. Layer 1: Serialization Traits](#3-layer-1-serialization-traits)
      - [3.1 The Serializable Concept](#31-the-serializable-concept)
      - [3.2 ADL Discovery Mechanics](#32-adl-discovery-mechanics)
      - [3.3 Builtin Trait Catalog](#33-builtin-trait-catalog)
      - [3.4 Enum Serialization via magic\_enum](#34-enum-serialization-via-magic_enum)
      - [3.5 Math Type Traits](#35-math-type-traits)
      - [3.6 Container Traits and Atomicity](#36-container-traits-and-atomicity)
      - [3.7 Domain Type Traits — InputActionMap Case Study](#37-domain-type-traits--inputactionmap-case-study)
    - [4. Layer 2: Format Backends](#4-layer-2-format-backends)
      - [4.1 The IFormatBackend Interface](#41-the-iformatbackend-interface)
      - [4.2 JsonBackend Internals](#42-jsonbackend-internals)
      - [4.3 JSON Isolation Guarantee](#43-json-isolation-guarantee)
    - [5. Layer 3: File I/O Integration](#5-layer-3-file-io-integration)
      - [5.1 SaveToFile — Serialize → Write Pipeline](#51-savetofile--serialize--write-pipeline)
      - [5.2 LoadFromFile — Atomic Read → Deserialize Pipeline](#52-loadfromfile--atomic-read--deserialize-pipeline)
      - [5.3 Virtual Path Integration](#53-virtual-path-integration)
      - [5.4 Config Path Fallback Chain](#54-config-path-fallback-chain)
    - [6. End-to-End Data Flow Walkthrough](#6-end-to-end-data-flow-walkthrough)
      - [6.1 Save Path](#61-save-path)
      - [6.2 Load Path](#62-load-path)
    - [7. Error Handling Model](#7-error-handling-model)
    - [8. Test Coverage Architecture](#8-test-coverage-architecture)
  - [Part II: Comparison with Modern Game Engines](#part-ii-comparison-with-modern-game-engines)
    - [9. Intermediate Representation Strategy](#9-intermediate-representation-strategy)
    - [10. Trait / Registration Model](#10-trait--registration-model)
    - [11. Format Backend Architecture](#11-format-backend-architecture)
    - [12. Enum Serialization Strategy](#12-enum-serialization-strategy)
    - [13. Atomic Load Semantics](#13-atomic-load-semantics)
    - [14. Schema and Versioning](#14-schema-and-versioning)
    - [15. Config Fallback and Layering](#15-config-fallback-and-layering)
    - [16. Mechanisms Present in Mature Engines but Absent in RTRLab](#16-mechanisms-present-in-mature-engines-but-absent-in-rtrlab)
    - [17. Overall Positioning](#17-overall-positioning)

---

## Part I: How the System Works

### 1. Architecture at a Glance

The serialization system is organized as a four-layer stack. Each layer has a single
responsibility and depends only on the layer below it:

```
Layer 3 -- File I/O Integration        Serialization.h
           -> (logical paths + text I/O)
Layer 2 -- Format Backends             IFormatBackend.h, JsonBackend.h/.cpp
           -> (string -> PropertyTree)
Layer 1 -- Serialization Traits        SerializationTraits.h, BuiltinTraits.h
           -> (T -> PropertyTree)
Layer 0 -- Intermediate Representation PropertyTree.h/.cpp
```

Data flows bidirectionally through the stack:

- **Save path**: `T` → Serialize trait → `PropertyTree` → backend `WriteToString` → string → file
- **Load path**: file → string → backend `ReadFromString` → `PropertyTree` → Deserialize trait → `T`

The key invariant is that domain types never interact with format-specific libraries.
A `Transform` struct knows how to convert itself to/from a `PropertyTree`, but has no
idea whether that tree becomes JSON, YAML, or binary on disk.

---

### 2. Layer 0: PropertyTree — The Intermediate Representation

**Files**: `PropertyTree.h`, `PropertyTree.cpp`

`PropertyTree` is the pivot between domain types and format backends. It is a recursive
value type that can represent any structured data: null, boolean, integer, float, string,
array, or object (key-value map).

#### 2.1 Variant Storage

The internal storage is a single `std::variant`:

```cpp
using Null   = std::monostate;
using Bool   = bool;
using Int    = int64_t;
using Float  = double;
using String = std::string;
using Array  = std::vector<PropertyTree>;
using Object = std::map<std::string, PropertyTree>;

using Value = std::variant<Null, Bool, Int, Float, String, Array, Object>;

Value m_Value;  // the only data member
```

Key type decisions:

| Type | Representation | Rationale |
|------|---------------|-----------|
| Int | `int64_t` | Covers all integer widths the project uses (int, int64_t, uint8_t, uint16_t) without loss |
| Float | `double` | Covers both `float` and `double` without precision loss |
| Object | `std::map` | Sorted keys produce deterministic, diff-friendly output |
| Array | `std::vector<PropertyTree>` | Natural recursive structure |

Scalars (Null, Bool, Int, Float) are stored inline in the variant — no heap allocation.
String, Array, and Object allocate on the heap through their respective standard library
containers.

#### 2.2 Implicit Constructors and Ergonomic Building

`PropertyTree` provides implicit constructors from all value types:

```cpp
PropertyTree(bool v);
PropertyTree(int v);       // promotes to int64_t internally
PropertyTree(float v);     // promotes to double internally
PropertyTree(const char *v);
PropertyTree(std::string v);
PropertyTree(Array v);
PropertyTree(Object v);
```

This enables concise tree construction using initializer lists:

```cpp
PropertyTree tree = PropertyTree::Object{
    {"name",    PropertyTree("RTRLab")},
    {"version", PropertyTree(1)},
    {"values",  PropertyTree::Array{PropertyTree(1), PropertyTree(2.5)}}
};
```

The `int → int64_t` and `float → double` promotions in the constructors ensure that
callers can pass literal integers and floats without explicit casts.

#### 2.3 Null Auto-Promotion

The mutable `operator[](const std::string&)` auto-promotes a Null tree to an Object:

```cpp
PropertyTree &PropertyTree::operator[](const std::string &key)
{
    if (IsNull())
        m_Value = Object{};
    RTRLAB_ASSERT_MSG(IsObject(), "...");
    return std::get<Object>(m_Value)[key];
}
```

This means you can build nested trees without pre-initializing intermediate objects:

```cpp
PropertyTree tree;                           // starts as Null
tree["render"]["samples"] = PropertyTree(8); // auto-promotes to nested Object
```

The const `operator[]` does *not* auto-promote — it asserts on non-Object and asserts
on missing keys. This asymmetry is intentional: reads should be explicit, writes should
be convenient.

#### 2.4 GetOr — Safe Lookup with Fallback

`GetOr<T>` provides safe access for the common case of reading optional config values:

```cpp
template <typename T>
T PropertyTree::GetOr(const std::string &key, const T &fallback) const
```

It handles four cases via `if constexpr`:

1. **bool** — requires exact `IsBool()` match (prevents `0`/`1` → bool coercion)
2. **integral types** — requires `IsInt()`, casts via `static_cast<T>(AsInt())`
3. **floating-point types** — requires `IsNumber()` (accepts int→float promotion via `AsFloat()`)
4. **std::string** — requires `IsString()`

For any other type, or if the key is missing, or if the tree is not an Object, the
fallback is returned. This function never throws — it always returns a valid value.

#### 2.5 Key Ordering and Determinism

`PropertyTree::Object` uses `std::map<std::string, PropertyTree>` (not `unordered_map`).
This means:

- Keys are stored in **lexicographic order**
- Iteration order is deterministic across platforms and runs
- JSON output is alphabetically sorted by key
- Diffs between serialized files are meaningful (no random key reordering)
- Cost: O(log n) lookup instead of O(1) — acceptable for the config-scale data sizes
  this system handles

---

### 3. Layer 1: Serialization Traits

**Files**: `SerializationTraits.h`, `BuiltinTraits.h`

The trait layer converts between domain types and `PropertyTree`. It uses C++20 concepts
and argument-dependent lookup (ADL) — no base classes, no virtual functions, no macros.

#### 3.1 The Serializable Concept

```cpp
template <typename T>
concept Serializable = requires(PropertyTree &tree, const PropertyTree &ctree,
                                T &val, const T &cval)
{
    { Serialize(tree, cval) };
    { Deserialize(ctree, val) } -> std::same_as<bool>;
};
```

Any type that provides a matching `Serialize` + `Deserialize` free function pair
satisfies the concept. The File I/O layer (`Serialization.h`) constrains all its
templates with `Serializable`, producing clear compiler errors if a type lacks traits.

#### 3.2 ADL Discovery Mechanics

Trait functions are discovered via ADL. The lookup rules:

1. Builtin traits live in `namespace Serialization` — always found because `PropertyTree`
   is in that namespace (ADL searches the namespaces of all argument types).
2. Domain traits can live in the type's own namespace or in `Serialization::` — both
   work because ADL searches the namespaces of all arguments.
3. Template trait specializations (enum, vector, map, optional) are constrained with
   `requires std::is_enum_v<E>` or `Serializable T` to prevent ambiguity.

For `enum class` types, the generic enum template in `BuiltinTraits.h` acts as a
"default path." Domain types that need custom enum serialization (e.g., `Key::Code`)
simply provide a more-specific overload, which C++ overload resolution prefers.

#### 3.3 Builtin Trait Catalog

`BuiltinTraits.h` provides all inline trait implementations. The full catalog:

| Category | Types | Serialize format | Deserialize validation |
|----------|-------|-----------------|----------------------|
| Primitives | `bool` | Bool node | `IsBool()` — exact type match |
| Primitives | `int`, `int64_t` | Int node | `IsInt()` — exact type match |
| Primitives | `float`, `double` | Float node | `IsNumber()` — accepts int→float promotion |
| Primitives | `std::string` | String node | `IsString()` |
| Narrow ints | `uint8_t`, `uint16_t` | Int node (promoted via `static_cast<int>`) | `IsInt()` + range check (0..max) |
| Asset refs | `Resource::AssetPath` | String (validated logical path) | `IsString()` + `TryCreate` validation |
| Enums | any `enum class` | String via `magic_enum::enum_name` | `IsString()` + `magic_enum::enum_cast` |
| Math | `Vec2`, `Vec3`, `Vec4` | Array of floats | `IsArray()` + size check |
| Math | `Mat4` | Array of 16 floats (column-major) | `IsArray()` + `Size() == 16` |
| Containers | `std::vector<T>` | Array of serialized `T` | Element-wise deserialize into temp |
| Containers | `std::unordered_map<std::string, T>` | Object with string keys | Entry-wise deserialize into temp |
| Containers | `std::optional<T>` | Null when empty, serialized T otherwise | `IsNull()` → nullopt, else deserialize |

All functions are `inline` — the header is entirely header-only with no `.cpp` file needed.

#### 3.4 Enum Serialization via magic_enum

The enum trait uses `magic_enum` v0.9.7 (vendored single header) for compile-time
enum name reflection:

```cpp
// Serialize: enum → string token
template <typename E>
    requires std::is_enum_v<E>
void Serialize(PropertyTree &tree, E value)
{
    auto name = magic_enum::enum_name(value);    // returns string_view
    tree = PropertyTree(std::string(name));
}

// Deserialize: string token → enum
template <typename E>
    requires std::is_enum_v<E>
bool Deserialize(const PropertyTree &tree, E &value)
{
    if (!tree.IsString()) return false;
    auto result = magic_enum::enum_cast<E>(tree.AsString());
    if (!result.has_value())
    {
        LOG_WARN_CAT(LogCategory::Core, "unknown enum value '{}' for type {}",
                     tree.AsString(), magic_enum::enum_type_name<E>());
        return false;
    }
    value = *result;
    return true;
}
```

Important caveats:

- `magic_enum::enum_name` for out-of-range values returns an empty `string_view` — the
  serialized result is `""`. On round-trip, `enum_cast("")` fails and the original
  numeric value is lost. (See CodeReview finding #7.)
- magic_enum tokens are tied to C++ enumerator spellings. If an enumerator is renamed,
  the on-disk token changes silently. Types needing stable tokens (like `Key::Code`)
  use explicit custom overloads instead.
- `magic_enum` instantiates significant compile-time templates per enum type. This is
  concentrated in `BuiltinTraits.h` — any file including it pays the compilation cost.

#### 3.5 Math Type Traits

Engine math vectors serialize as JSON arrays:

```
Math::Vec3{1.0, 2.0, 3.0}  →  [1.0, 2.0, 3.0]
Math::Mat4                  →  [16 floats, column-major]
```

Deserialization validates the array size before reading elements. For `vec3`:

```cpp
inline bool Deserialize(const PropertyTree &tree, Math::Vec3 &v)
{
    if (!tree.IsArray() || tree.Size() != 3)
        return false;
    v.x = static_cast<float>(tree[size_t(0)].AsFloat());
    v.y = static_cast<float>(tree[size_t(1)].AsFloat());
    v.z = static_cast<float>(tree[size_t(2)].AsFloat());
    return true;
}
```

Note: math traits write directly into the output parameter rather than deserializing
into a local temporary. This means if `AsFloat()` were to throw after `v.x` is already
written, `v` would be left in a partially modified state. In practice the preceding
`IsArray() && Size() == 3` check prevents reaching `AsFloat()` with bad data, so the
risk is theoretical. (See CodeReview finding #1.)

#### 3.6 Container Traits and Atomicity

Container traits (`vector`, `unordered_map`, `optional`) all follow the atomic
deserialization pattern — they deserialize into a local temporary and only
`std::move` into the output on success:

```cpp
template <Serializable T>
bool Deserialize(const PropertyTree &tree, std::vector<T> &vec)
{
    if (!tree.IsArray()) return false;
    std::vector<T> result;       // ← local temporary
    result.reserve(arr.size());
    for (const auto &elem : arr)
    {
        T val{};
        if (!Deserialize(elem, val))
            return false;        // ← vec unchanged on failure
        result.push_back(std::move(val));
    }
    vec = std::move(result);     // ← commit on success
    return true;
}
```

If any element fails to deserialize, the function returns `false` and the original `vec`
is untouched. This is the "partial failures never commit" guarantee the design doc
promises.

#### 3.7 Domain Type Traits — InputActionMap Case Study

`InputActionSerialization.h` is the first (and currently only) domain trait file. It
demonstrates several patterns:

**Mixed enum strategies**: `InputSource::Type` and `MouseAxis` use the generic
magic_enum trait (tokens match C++ names: `"Key"`, `"MouseButton"`, `"X"`, `"Y"`).
`Key::Code` and `Mouse::Code` use `InputNames.h` for stable canonical names with alias
support — these are `uint16_t` namespaced constants, not `enum class`, so magic_enum
does not apply.

**Heterogeneous arrays**: The `actions` object maps action names to arrays that can
contain either plain `InputSource` objects or `ChordBinding` objects. During
deserialization, each element is tried first as a `ChordBinding` (checking for
`"kind": "Chord"`), then as an `InputSource`. This try-first-then-fallback pattern
handles polymorphic entries without requiring a type discriminator at the array level.

**Inner atomicity**: `Deserialize(tree, InputActionMap&)` creates a local `InputActionMap
temp` and only moves it to the output at the end. Individual action/axis deserialization
failures produce `LOG_WARN` and `continue` (skip the bad entry) rather than aborting the
entire map. This is a pragmatic choice: one corrupted key binding should not prevent
loading all other bindings.

**JSON format preservation**: The trait produces the exact same JSON structure as the
original inline `SaveToFile`/`LoadFromFile` that it replaced. Existing config files
load without modification.

---

### 4. Layer 2: Format Backends

**Files**: `IFormatBackend.h`, `JsonBackend.h`, `JsonBackend.cpp`

#### 4.1 The IFormatBackend Interface

```cpp
class IFormatBackend
{
public:
    virtual ~IFormatBackend() = default;
    virtual std::string WriteToString(const PropertyTree &tree) const = 0;
    virtual bool ReadFromString(const std::string &data, PropertyTree &tree) const = 0;
};
```

The interface is deliberately minimal — just two methods. Backends convert between
a `PropertyTree` and a string (or byte buffer for future binary backends). This
narrow contract means adding a new backend (YAML, binary, MessagePack) requires
implementing exactly two functions and zero changes to any domain type or trait.

#### 4.2 JsonBackend Internals

`JsonBackend` wraps `nlohmann/json` (commonly known as `json.hpp`). The implementation
uses two file-local helper functions in an anonymous namespace:

**TreeToJson** (PropertyTree → nlohmann::json): Uses `std::visit` on
`PropertyTree::GetValue()` to dispatch on the variant type. Each variant alternative
maps directly to a nlohmann::json type. Recursive for Array and Object.

**JsonToTree** (nlohmann::json → PropertyTree): Uses a `switch` on
`nlohmann::json::value_t` to dispatch on the JSON type. Notable handling:

- `number_unsigned`: nlohmann parses large unsigned integers as `uint64_t`. If the
  value exceeds `int64_t::max()`, an `std::overflow_error` is thrown (caught by the
  outer try-catch in `ReadFromString`). This prevents silent truncation.
- `number_integer`: stored as `int64_t` directly.
- `number_float`: stored as `double` directly.

**WriteToString** delegates to `TreeToJson` then calls `nlohmann::json::dump(m_Indent)`.
The default indent is 2 spaces (pretty-printed).

**ReadFromString** wraps the parse-and-convert in a three-level try-catch:

```cpp
try {
    auto j = nlohmann::json::parse(data);
    tree = JsonToTree(j);
    return true;
}
catch (const nlohmann::json::parse_error &e) { /* parse failure */ }
catch (const nlohmann::json::exception &e)   { /* other json error */ }
catch (const std::exception &e)              { /* overflow, etc. */ }
```

All errors are logged via `LOG_ERROR_CAT` and `ReadFromString` returns `false`.

The test suite (`TestJsonBackend.cpp`) verifies that on parse failure, `tree` is left
unchanged — tested by pre-populating `tree` with a sentinel value (`"sentinel"`) and
asserting it survives a failed parse.

#### 4.3 JSON Isolation Guarantee

`#include <json.hpp>` appears in exactly one file: `JsonBackend.cpp`. No other engine
header or source file imports nlohmann/json for serialization purposes.

This means:

- Swapping JSON libraries (e.g., to RapidJSON or simdjson) requires changes to one
  `.cpp` file
- Domain type headers never depend on third-party JSON types
- `PropertyTree` can be used in tests without linking any JSON library (only needed
  if actually serializing to JSON string)

---

### 5. Layer 3: File I/O Integration

**File**: `Serialization.h` (entirely header-only, all template functions)

This layer ties together the filesystem, format backends, and traits into convenient
one-call save/load functions.

#### 5.1 SaveToFile — Serialize → Write Pipeline

```
SaveToFile(value, path, backend)
    1. Serialize(tree, value)          — trait converts T → PropertyTree
    2. backend.WriteToString(tree)     — backend converts tree → string
    3. create_directories(parent_path) — ensure directory exists
    4. ofstream << data                — write to disk
```

Returns `false` if directory creation or file write fails, with `LOG_ERROR`.

The auto-detect overload extracts the file extension and calls
`GetBackendForExtension()` — currently always returns `JsonBackend` since only JSON
is implemented.

#### 5.2 LoadFromFile — Atomic Read → Deserialize Pipeline

```
LoadFromFile(value, path, backend)
    1. ifstream → ostringstream → string  — read entire file
    2. backend.ReadFromString(data, tree)  — parse string → PropertyTree
    3. T temp{};                           — create default-constructed temporary
    4. Deserialize(tree, temp)             — trait converts PropertyTree → T
    5. value = std::move(temp)             — commit on success
```

The critical step is #3–#5: deserialization happens into a **temporary**. If
`Deserialize` returns `false`, `temp` is discarded and the caller's `value` is
completely unchanged. This is the "atomic load" guarantee — no partial updates.

The tradeoff: `T` must be default-constructible for `T temp{}` to compile. This is
not enforced by the `Serializable` concept — it is an implicit requirement of
`LoadFromFile` specifically. (See CodeReview finding #4.)

#### 5.3 Virtual Path Integration

`SaveToVirtualPath` and `LoadFromVirtualPath` bridge the serialization system with
the Resource System's logical path model:

- Save: `FileSystem::ResolveWritePath(virtualPath)` -> physical path -> `SaveToFile`
- Load: `FileSystem::ReadText(virtualPath)` -> backend parse -> deserialize

Both come in two overloads: explicit backend and auto-detect. Save still resolves a
writable physical path before delegating to `SaveToFile`, while load uses the resource
system's text-returning API and deserializes directly from the returned string.

#### 5.4 Config Path Fallback Chain

The config path functions implement a three-tier fallback for configuration files:

```
LoadFromConfigPath("input/ShadowMapping.json")

    1. Try /Saved/Config/input/ShadowMapping.json
       -> if readable: return its text (user has saved config)

    2. Try /Project/Config/input/ShadowMapping.json
       -> if readable: write the same text to /Saved/Config/... and return the text
       (auto-seeding: project defaults become user-editable saved configs)

    3. Try /Engine/Config/input/ShadowMapping.json
       -> if readable: write the same text to /Saved/Config/... and return the text
       (engine defaults as last resort)

    4. All three miss: return false
```

**Auto-seeding**: When a config is found in Project or Engine but not in Saved, the
system automatically writes the resolved text to the Saved directory. This means:

- First run: engine defaults are copied into Saved, where users can edit them
- Subsequent runs: the Saved copy is used directly
- The Project/Engine originals are never modified

`SaveToConfigPath` always writes to `/Saved/Config/...` -- the Project and Engine tiers
are read-only sources.

The implementation lives in `detail::ResolveConfigReadText()`, which encapsulates the
three-tier lookup and seeding logic. The `seedToSaved` lambda handles the Saved write
with error handling:

```cpp
auto seedToSaved = [&](std::string_view sourceVirtualPath, std::string_view label)
    -> std::optional<std::string>
{
    // Read source text via FileSystem::ReadText
    // Write the same text to /Saved/Config/... via FileSystem::WriteText
    // Return the source text either way
};
```

If the Saved write fails (e.g., permissions), the function still returns the source
text and logs a warning. This ensures the system degrades gracefully rather than
failing outright.

---

### 6. End-to-End Data Flow Walkthrough

#### 6.1 Save Path

Tracing `Serialization::SaveToConfigPath(inputMap, "input/ShadowMapping.json")`:

```
1. SaveToConfigPath
   -> builds virtualPath = "/Saved/Config/input/ShadowMapping.json"
   -> calls SaveToVirtualPath(inputMap, virtualPath)

2. SaveToVirtualPath
   -> FileSystem::ResolveWritePath("/Saved/Config/input/ShadowMapping.json")
   -> returns e.g. "C:/Users/name/AppData/Local/RTRLab/Saved/Config/input/ShadowMapping.json"
   -> calls SaveToFile(inputMap, resolvedPath)

3. SaveToFile
   -> Serialize(tree, inputMap)
      -> InputActionSerialization.h Serialize trait runs
      -> builds PropertyTree::Object{} with "actions" and "axes" subtrees
      -> each InputSource serializes its Type (magic_enum) and Code (InputNames.h)
   -> GetBackendForExtension(".json") -> JsonBackend instance
   -> JsonBackend::WriteToString(tree)
      -> TreeToJson: recursive std::visit converts PropertyTree -> nlohmann::json
      -> nlohmann::json::dump(2): produces pretty-printed JSON string
   -> create_directories for parent path
   -> write string to file via ofstream
```

#### 6.2 Load Path

Tracing `Serialization::LoadFromConfigPath(inputMap, "input/ShadowMapping.json")`:

```
1. LoadFromConfigPath
   -> detail::ResolveConfigReadText("input/ShadowMapping.json")
   -> tries /Saved/Config/input/ShadowMapping.json -> found
   -> returns file contents as text

2. LoadFromConfigPath
   -> GetBackendForExtension(".json") -> JsonBackend instance
   -> JsonBackend::ReadFromString(data, tree)
      -> nlohmann::json::parse(data): tokenize + parse JSON
      -> JsonToTree: recursive switch converts nlohmann::json -> PropertyTree
      -> uint64 overflow check for large unsigned integers
   -> InputActionMap temp{};  // default-constructed temporary
   -> Deserialize(tree, temp)
      -> InputActionSerialization.h Deserialize trait runs
      -> reads "actions" object: for each action name, tries each array
         element as ChordBinding then InputSource
      -> reads "axes" object: switches on "kind" string ("KeyPair",
         "MouseAxis", "GamepadAxis")
      -> unknown entries produce LOG_WARN and continue (not fail)
   -> inputMap = std::move(temp);  // atomic commit
```

---

### 7. Error Handling Model

The serialization system implements a layered error handling strategy:

| Layer | Failure mode | Behavior | Recovery |
|-------|-------------|----------|----------|
| File I/O | File not found | `LoadFromFile` returns `false`, no log | Caller decides (expected for first run) |
| File I/O | Write failure | `LOG_ERROR`, returns `false` | Caller retries or reports |
| Backend | Malformed JSON | `LOG_ERROR` with parser message, returns `false` | `tree` left unchanged |
| Backend | uint64 overflow | `std::overflow_error` caught, `LOG_ERROR` | `tree` left unchanged |
| Trait | Type mismatch | `LOG_WARN` (unknown enum, bad type), returns `false` | Field skipped |
| Trait | Missing optional field | Silent — field retains default | By design |
| Trait | Missing required field | Trait returns `false` | Caller's value unchanged |
| Domain | Unknown key in config | `LOG_WARN` | Forward-compatible: ignored |
| Domain | Bad action binding | `LOG_WARN`, continue to next | Other bindings still load |
| Top-level | Any deserialization failure | `LoadFromFile` discards temp, returns `false` | Caller's value unchanged |

The consistent pattern is: **warn and skip** for individual field failures,
**error and abort** for structural failures, **never commit partial results**.

---

### 8. Test Coverage Architecture

The test suite is organized to mirror the layered architecture:

**Unit tests** (per-layer isolation):

| Test file | Layer | Test count | What it covers |
|-----------|-------|-----------|----------------|
| `TestPropertyTree.cpp` | 0 | 25 | Construction for all types, type queries, GetOr with type matching/fallback, auto-promote Null→Object, nested writes, stable key ordering |
| `TestJsonBackend.cpp` | 2 | 20 | Write/read for all scalar types, arrays, objects, nested structures, round-trips, malformed JSON rejection, empty string rejection, tree-unchanged-on-parse-failure, uint64 overflow |
| `TestBuiltinTraits.cpp` | 1 | 33 | Round-trips for all builtin types (bool, int, int64, float, double, string, uint8, uint16, AssetPath, enums, vec2/3/4, mat4, vector, map, optional), type mismatch rejection, container partial failure, nested containers |

**Contract tests** (cross-layer integration):

| Test file | Layers | Test count | What it covers |
|-----------|--------|-----------|----------------|
| `TestSerializationFileIO.cpp` | 1–3 | 18 | File round-trips (simple types, containers, domain types), virtual path round-trips, config path fallback/seeding (Saved→Project→Engine), AssetPath rejection (absolute paths, Saved domain) |

Total: **96 tests** covering all layers and their interactions.

Notable test patterns:

- **Sentinel values**: Tests pre-populate output variables with known values, then
  verify they survive failed operations (e.g., `PropertyTree tree("sentinel")` before
  a failed parse)
- **Negative cases**: Every type has both "works correctly" and "rejects wrong type"
  tests
- **Container partial failure**: Tests verify that if an element deep inside a nested
  container fails, the outer container is unchanged
- **Config seeding**: Contract tests verify the auto-copy from Project/Engine to Saved,
  including the case where Saved already exists (should not overwrite)

---

## Part II: Comparison with Modern Game Engines

### 9. Intermediate Representation Strategy

| Engine | IR Type | Format coupling |
|--------|---------|----------------|
| **RTRLab** | `PropertyTree` (custom `std::variant` tree) | Fully decoupled — domain types never see json.hpp |
| **Unreal Engine** | `FArchive` (binary stream), `FJsonObject`/`FJsonValue` (JSON DOM) | Two separate systems: `FArchive` for binary, JSON wrappers for config. Domain types implement `Serialize(FArchive&)` which is format-coupled |
| **Unity** | No explicit IR — `JsonUtility`, `YAMLDotNet`, `BinaryFormatter` each work directly on C# objects via reflection | Format-specific; swapping format requires different API |
| **Godot** | `Variant` (tagged union of 38 types) | `Variant` acts as universal IR for properties, scripting, and serialization. Very broad but tightly coupled to engine |
| **id Tech / Source** | No IR — key-value string pairs in `.cfg` files, binary lumps in map formats | Format is the representation |

RTRLab's approach is closest to the "property system" pattern used by Godot's Variant,
but much narrower in scope — `PropertyTree` handles only serialization, not scripting
or UI binding. The variant has 7 alternatives vs. Godot's 38, keeping it focused and
lightweight.

Compared to Unreal's dual-system approach (`FArchive` for binary, JSON wrappers for
text), RTRLab uses a single IR for all formats. This is simpler but means binary
serialization will still pass through the tree representation — potentially slower for
high-throughput scenarios (scene snapshots, network replication).

### 10. Trait / Registration Model

| Engine | Type registration | Boilerplate per type |
|--------|------------------|---------------------|
| **RTRLab** | C++20 concept + ADL free functions | ~10-20 lines of Serialize/Deserialize functions |
| **Unreal** | `UPROPERTY()` macro + reflection codegen | Zero for marked properties; macro on every serialized field |
| **Unity** | `[SerializeField]` C# attribute + runtime reflection | Zero for marked fields; attribute per field |
| **Godot** | `BIND_PROPERTY()` macro → Variant | One macro per property |
| **CryEngine** | `Schematyc` reflection + `Serialize(Serialization::IArchive&)` | Method override per type |

RTRLab's ADL-based approach has a moderate boilerplate cost (explicit Serialize/
Deserialize function pairs) but maximal flexibility — each type controls exactly how
its fields map to the tree. There are no macros, no code generation, and no inheritance
requirements.

The tradeoff is that adding a new serializable field requires manually updating the
trait function, whereas macro/attribute-based systems (Unreal, Unity, Godot) add
serialization with a single annotation. At the current project scale (~5 serializable
types), explicit traits are preferable. At 50+ types, the cost/benefit shifts toward
macro or reflection-based approaches, which is acknowledged in the design document.

### 11. Format Backend Architecture

| Engine | Backend model | Supported formats |
|--------|-------------|-------------------|
| **RTRLab** | `IFormatBackend` interface, backend selected by file extension | JSON (Phase 1). YAML, binary, MessagePack planned |
| **Unreal** | `FArchive` subclasses (`FMemoryReader`, `FFileReaderGeneric`, `FJsonArchiveOutputFormatter`) | Binary (UAsset), JSON (config), XML (project files), custom text (INI) |
| **Unity** | Separate APIs per format (`JsonUtility`, `BinaryFormatter`, `AssetBundle`) | Binary (scenes, prefabs), YAML (text mode scenes), JSON (configs) |
| **Godot** | `ResourceFormatSaver`/`ResourceFormatLoader` interface | tres (text), res (binary), custom via plugins |

RTRLab's backend model is closest to Godot's — a clean interface with pluggable
implementations. Unreal's `FArchive` is conceptually similar but couples the
serialization direction (read vs. write) and format into a single class hierarchy, which
is more monolithic.

The current single-backend state (JSON only) is appropriate for the project's needs
(config files, input mappings). The interface is ready for expansion without changes to
existing code.

### 12. Enum Serialization Strategy

| Engine | Enum on-disk format | Token stability | Runtime cost |
|--------|-------------------|----------------|-------------|
| **RTRLab** | String token via `magic_enum` (default) or custom overload | Default: tied to C++ spelling; custom: stable | Compile-time reflection + `enum_cast` at load |
| **Unreal** | Integer by default; `FName` string via `UMETA(DisplayName)` in editor | Integer: stable if not reordered; FName: stable | Integer: zero overhead; FName: hash lookup |
| **Unity** | Integer by default; string via `[EnumMember]` attribute + custom serializer | Integer: fragile on reorder; string: stable | Integer: zero; string: Enum.Parse at load |
| **Godot** | String via `Variant::get_type_name()` or integer | Depends on usage | Variant-level lookup |

RTRLab's string-first approach (magic_enum) prioritizes human readability of config files
over compactness. The dual strategy (magic_enum for simple enums, custom overloads for
stable tokens) is a pragmatic balance. The main risk is that magic_enum tokens silently
change on rename — mitigated by reserving custom overloads for stability-critical enums
like input codes.

### 13. Atomic Load Semantics

| Engine | Atomic load guarantee | Mechanism |
|--------|---------------------|-----------|
| **RTRLab** | Yes — `LoadFromFile` deserializes into temporary, moves on success | `T temp{}; Deserialize(tree, temp); value = std::move(temp);` |
| **Unreal** | No general guarantee — `Serialize(FArchive&)` modifies fields in-place | Some systems (e.g., package loading) have transactional wrappers |
| **Unity** | No — `JsonUtility.FromJsonOverwrite()` modifies target directly | Caller responsible for backup/restore |
| **Godot** | Partial — `ResourceLoader` returns a new `Ref<Resource>` (new allocation) | New object created on load; old ref unchanged until swap |

RTRLab's atomic load is a genuine strength. Most engines do not provide this guarantee
at the serialization framework level — callers must implement their own backup/restore
logic if they want rollback on failure. Godot's `ResourceLoader` achieves something
similar by returning new objects rather than modifying existing ones.

### 14. Schema and Versioning

| Engine | Schema validation | Versioning mechanism |
|--------|------------------|---------------------|
| **RTRLab** | Planned (Phase 3) — whitelist + warn, unknown keys allowed | Not yet implemented |
| **Unreal** | Full: `FCustomVersionRegistration`, `FArchive::UsingCustomVersion()` | Per-type custom version numbers, upgrade functions |
| **Unity** | None built-in (JSON); YAML scenes use `fileFormatVersion` field | Scene format version; no per-type versioning |
| **Godot** | `format_version` in `.tres`/`.res` files | Single format version; migration via `_set_property_list` |

This is where mature engines have the most significant advantage. Unreal's version
system allows individual types to register version numbers and provide upgrade functions
that transform old data during deserialization. RTRLab currently has no versioning — a
renamed field silently becomes a missing optional field (uses default) or causes a
deserialization failure.

The planned Phase 3 schema validation (whitelist + warn, unknown keys allowed) is a
reasonable first step. It provides forward compatibility (newer config format readable
by older engine) but does not address backward compatibility (older config format
needing migration to new schema).

### 15. Config Fallback and Layering

| Engine | Config layering | Auto-seeding |
|--------|----------------|-------------|
| **RTRLab** | Three-tier: Saved → Project → Engine | Yes — auto-copies Project/Engine defaults to Saved on first access |
| **Unreal** | Multi-tier: `DefaultEngine.ini` → `Base<Platform>.ini` → `<Platform>/...ini` → per-project → saved | Complex merge system, not simple copy |
| **Unity** | No built-in layering — `PlayerPrefs` or custom | None |
| **Godot** | `ProjectSettings` with overrides per platform/feature | Export presets, not runtime layering |
| **CryEngine** | `sys_spec` quality levels with cascading console variable overrides | CVars cascade by spec level |

RTRLab's three-tier config system is notably clean compared to Unreal's INI merge
complexity. The auto-seeding behavior (copy default to Saved on first read) is a
practical pattern that gives users editable config files without requiring a manual
"create config" step. Unreal's INI system achieves similar goals but with a much more
complex merge strategy (config sections can add, remove, or clear entries from parent
tiers).

### 16. Mechanisms Present in Mature Engines but Absent in RTRLab

| Mechanism | What it does | Which engines have it |
|-----------|-------------|---------------------|
| **Versioning / migration** | Transform old serialized data to match new schema | Unreal (custom versions), Godot (format versions) |
| **Binary serialization** | Compact, fast serialization for large data (scenes, assets) | Unreal (UAsset), Unity (scene binary), Godot (.res) |
| **Reflection-driven serialization** | Auto-serialize all marked properties without explicit trait code | Unreal (UPROPERTY), Unity ([SerializeField]), Godot (BIND_PROPERTY) |
| **Async / streaming deserialization** | Load data without blocking the main thread | Unreal (async package loading), Unity (AssetBundle.LoadAsync) |
| **Diff / merge tooling** | Structured diff of serialized files for version control | Unreal (UAsset diff), Unity (YAML merge) |
| **Object references** | Serialize references between objects (not just self-contained values) | Unreal (hard/soft object references), Unity (GUID refs) |
| **Network serialization** | Serialize data for replication / RPC | Unreal (FArchive + NetDriver), Unity (NetworkVariable) |

Most of these are premature for RTRLab's current scope. The most likely near-term
additions are:

1. **Binary backend** — needed when scene save/load performance matters
2. **Versioning** — needed when config formats start evolving
3. **Object references** — needed when scene serialization (Phase 2) handles
   cross-referencing objects like meshes and materials

### 17. Overall Positioning

RTRLab's serialization system is a well-designed "right-sized" framework for its
current stage:

**Strengths relative to industry**:
- Cleaner format isolation than any mainstream engine (true single-point coupling)
- Atomic load semantics provided at the framework level (most engines leave this to callers)
- ADL-based traits avoid the macro tax of Unreal/Godot without requiring runtime reflection
- Config fallback chain with auto-seeding is a pragmatic, user-friendly pattern
- Strong test coverage at every layer, including negative cases and atomicity guarantees

**Appropriate simplifications**:
- No versioning yet — the system has few serialized types and their schemas are still evolving
- JSON-only — sufficient for config files and input mappings
- No async loading — not needed until scene data is large enough to cause frame drops
- No object references — not needed until scene serialization (Phase 2)

**Key design decisions that will age well**:
- `PropertyTree` as intermediate representation: already proven by Godot's Variant model
  (though narrower and cleaner). Adding binary/YAML backends requires zero domain changes.
- ADL trait discovery: forward-compatible with C++26 reflection. When compile-time
  reflection arrives, traits can be auto-generated while keeping the same concept interface.
- Separated config layering: the Saved → Project → Engine fallback is a pattern used by
  shipping engines, implemented cleanly.

**Architectural risks**:
- `BuiltinTraits.h` is a compilation bottleneck — pulling in math traits, magic_enum, and all
  container headers into every consumer. Splitting into focused headers before the type
  count grows would prevent build time regression.
- magic_enum token instability — currently mitigated by custom overloads for
  stability-critical types, but requires discipline to identify which enums need stable
  tokens as new types are serialized.
- No versioning means any schema change to serialized types is a breaking change for
  existing save files. Acceptable during active development; will need addressing before
  any form of release.
