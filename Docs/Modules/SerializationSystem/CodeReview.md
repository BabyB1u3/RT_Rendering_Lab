# Serialization System Code Review

Code review of the `src/Core/Serialization/` module and its primary consumer
`src/Core/Input/Action/InputActionSerialization.h`, based on the implementation as of
2026-04-09.

> **Overall assessment**: code quality is high. The four-layer separation
> (`PropertyTree` -> traits -> format backend -> file I/O) is clean, test coverage is
> strong, and the implementation generally matches the design document well. The main
> gaps are concentrated around a few edge-case contract violations, malformed-input
> robustness, and one future-extensibility mismatch in the file I/O layer.

---

## Table of Contents

- [Serialization System Code Review](#serialization-system-code-review)
  - [Table of Contents](#table-of-contents)
  - [Summary](#summary)
  - [1. Bug Risk: GLM Deserialization Is Not Atomic](#1-bug-risk-glm-deserialization-is-not-atomic)
  - [2. Interface Contract: ReadFromString Does Not Guarantee tree Unchanged on Failure](#2-interface-contract-readfromstring-does-not-guarantee-tree-unchanged-on-failure)
  - [3. Compile Performance: BuiltinTraits.h Is a Heavyweight Header](#3-compile-performance-builtintraitsh-is-a-heavyweight-header)
  - [4. Interface Contract: LoadFromFile Requires Default-Constructible T](#4-interface-contract-loadfromfile-requires-default-constructible-t)
  - [5. DRY: SaveToVirtualPath / LoadFromVirtualPath Duplicate Resolution Logic](#5-dry-savetovirtualpath--loadfromvirtualpath-duplicate-resolution-logic)
  - [6. Robustness: Config Seed-to-Saved Correctness Depends on an Implicit Invariant](#6-robustness-config-seed-to-saved-correctness-depends-on-an-implicit-invariant)
  - [7. Data Loss Risk: Enum Serialize Silently Produces Empty String for Out-of-Range Values](#7-data-loss-risk-enum-serialize-silently-produces-empty-string-for-out-of-range-values)
  - [8. Style: GetBackendForExtension Ignores Extension Parameter](#8-style-getbackendforextension-ignores-extension-parameter)
  - [9. Correctness: int Deserialization Silently Narrows Without Range Checks](#9-correctness-int-deserialization-silently-narrows-without-range-checks)
  - [10. Design Drift: SaveToFile / LoadFromFile Are Text-Mode Only, Blocking Future Binary Backends](#10-design-drift-savetofile--loadfromfile-are-text-mode-only-blocking-future-binary-backends)
  - [11. Robustness: InputAction Warning Path Can Assert on Malformed Non-String Fields](#11-robustness-inputaction-warning-path-can-assert-on-malformed-non-string-fields)
  - [12. Robustness: InputAction Device Index Wraps on Invalid Input](#12-robustness-inputaction-device-index-wraps-on-invalid-input)
  - [13. Strengths](#13-strengths)

---

## Summary

| Category | Count |
|----------|-------|
| Bug risk / correctness | 3 |
| Interface contract unclear | 2 |
| Compile performance | 1 |
| DRY / consistency | 1 |
| Robustness | 3 |
| Design drift / future extensibility | 2 |
| Minor issues | 1 |

The core architecture remains strong: `PropertyTree` is a clean IR, trait discovery is
simple, JSON is well isolated, and `LoadFromFile` preserves atomic commit semantics at
the top level. The most important new findings are #9, #10, #11, and #12 because they
were missing from the original review and expose either silent invalid-data acceptance or
design/document mismatches.

---

## 1. Bug Risk: GLM Deserialization Is Not Atomic

**File**: `src/Core/Serialization/BuiltinTraits.h:172-227`

```cpp
inline bool Deserialize(const PropertyTree &tree, glm::vec2 &v)
{
    if (!tree.IsArray() || tree.Size() != 2)
        return false;
    v.x = static_cast<float>(tree[size_t(0)].AsFloat());
    v.y = static_cast<float>(tree[size_t(1)].AsFloat());
    return true;
}
```

The design document promises that partial failures do not commit, and most container
traits follow that rule by deserializing into temporaries. However, `glm::vec2`, `vec3`,
`vec4`, and `mat4` write directly into the destination object.

In practice the risk is low because the shape checks are strong and `AsFloat()` accepts
integer values, but this is still inconsistent with the atomicity story the rest of the
framework tells.

**Recommendation**: deserialize into local temporaries first, then assign on success. If
that is intentionally out of scope, document GLM traits as in-place exceptions.

---

## 2. Interface Contract: ReadFromString Does Not Guarantee tree Unchanged on Failure

**File**: `src/Core/Serialization/JsonBackend.cpp:107-130`

`ReadFromString` takes `PropertyTree &tree` and returns `false` on parse/conversion
failure, but the function does not clearly guarantee whether `tree` is preserved on
failure. If conversion fails partway through a nested structure, a caller could observe a
partially modified tree.

Current callers do not rely on the output after failure, so this is harmless today.
Still, the contract is unclear and weaker than the rest of the serialization stack.

**Recommendation**: parse into a local `PropertyTree temp` and move-assign to `tree` only
on success.

---

## 3. Compile Performance: BuiltinTraits.h Is a Heavyweight Header

**File**: `src/Core/Serialization/BuiltinTraits.h`

This header pulls in GLM, `magic_enum`, and standard container traits in one place. Any
consumer that needs even a small subset of built-in traits transitively pays for all of
them.

That is manageable at current scale, but `magic_enum` and GLM are both meaningful compile
time contributors, so this header is likely to become a build-time hotspot as adoption
widens.

**Recommendation**: split into focused headers such as `PrimitiveTraits.h`,
`ContainerTraits.h`, `EnumTraits.h`, and `GlmTraits.h`, while keeping
`BuiltinTraits.h` as a convenience umbrella include.

---

## 4. Interface Contract: LoadFromFile Requires Default-Constructible T

**File**: `src/Core/Serialization/Serialization.h:186`

```cpp
T temp{};
```

`LoadFromFile` requires `T` to be default-constructible, but that constraint is not part
of the `Serializable` concept and is not called out in the function contract.

All current serializable types happen to satisfy it, so there is no present bug, but the
missing constraint will produce a confusing template error if a future type does not.

**Recommendation**: either add `std::default_initializable<T>` to the relevant concept /
function template, or document the requirement explicitly.

---

## 5. DRY: SaveToVirtualPath / LoadFromVirtualPath Duplicate Resolution Logic

**File**: `src/Core/Serialization/Serialization.h:133-162`, `:206-236`

Both the explicit-backend and auto-detect overloads of `SaveToVirtualPath` perform the
same resolve-and-log sequence. The same duplication exists on the load side.

This is not a bug, but it does create two maintenance sites for the same behavior.

**Recommendation**: resolve once in the more specific overloads and have the convenience
overloads delegate through them.

---

## 6. Robustness: Config Seed-to-Saved Correctness Depends on an Implicit Invariant

**File**: `src/Core/Serialization/Serialization.h:43-67`

The original review flagged a possible missing `create_directories()` call. On closer
inspection, the current code is correct because `ResolveWritePath()` already creates the
parent directory before `copy_file()` runs.

So this is not a current bug, but the correctness depends on a non-local invariant:
future refactors of `ResolveWritePath()` could quietly break config seeding without
touching this function.

**Recommendation**: add an explicit `create_directories(savedPath->parent_path())` here
or at least a short comment documenting the dependency.

---

## 7. Data Loss Risk: Enum Serialize Silently Produces Empty String for Out-of-Range Values

**File**: `src/Core/Serialization/BuiltinTraits.h:137-143`

```cpp
auto name = magic_enum::enum_name(value);
tree = PropertyTree(std::string(name));
```

If an enum value is outside the reflected range, `magic_enum::enum_name()` returns an
empty string view. The serialized result becomes `""`, and the original numeric value is
lost.

This is an edge case, but the failure mode is silent.

**Recommendation**: guard `name.empty()`. On failure, log and either serialize the
underlying integer or reject serialization explicitly.

---

## 8. Style: GetBackendForExtension Ignores Extension Parameter

**File**: `src/Core/Serialization/Serialization.h:81-86`

```cpp
inline const IFormatBackend &GetBackendForExtension(std::string_view /*ext*/)
{
    static const JsonBackend s_JsonBackend;
    return s_JsonBackend;
}
```

This is intentional in Phase 1 because only JSON exists. The issue is not the current
behavior; the issue is that adding a second backend later could silently fail if this
function is forgotten.

**Recommendation**: when a second backend is introduced, add explicit extension dispatch
and a warning or assert for unknown extensions.

---

## 9. Correctness: int Deserialization Silently Narrows Without Range Checks

**File**: `src/Core/Serialization/BuiltinTraits.h:43-49`

```cpp
inline bool Deserialize(const PropertyTree &tree, int &v)
{
    if (!tree.IsInt())
        return false;
    v = static_cast<int>(tree.AsInt());
    return true;
}
```

`PropertyTree` stores integers as `int64_t`, but the built-in `int` trait simply casts to
`int` without validating range. On out-of-range input, this will silently narrow and may
wrap or truncate depending on platform behavior.

This is inconsistent with the nearby `uint8_t` and `uint16_t` traits, which already do
strict range checks before committing.

It also conflicts with the system's general contract from the design doc: malformed data
should fail validation, not deserialize into a corrupted value.

**Recommendation**: mirror the unsigned traits:

```cpp
const auto raw = tree.AsInt();
if (raw < std::numeric_limits<int>::min() ||
    raw > std::numeric_limits<int>::max())
    return false;
v = static_cast<int>(raw);
```

---

## 10. Design Drift: SaveToFile / LoadFromFile Are Text-Mode Only, Blocking Future Binary Backends

**File**: `src/Core/Serialization/Serialization.h:112-120`, `:170-176`

```cpp
std::ofstream file(path);
...
file << data;
```

```cpp
std::ifstream file(path);
...
ss << file.rdbuf();
```

The design document explicitly positions the system as format-independent and calls out
future `BinaryBackend` / `MessagePackBackend` support without domain-type changes.
`IFormatBackend` already uses `std::string`, so binary payloads are plausible at the API
level. However, the file layer currently opens streams in text mode.

On Windows, text mode performs newline translation and has different treatment for some
byte sequences, which makes it unsuitable as a transparent transport for arbitrary binary
payloads. So the backend abstraction says "binary-capable later", while the actual file
I/O contract still says "text only".

This is not a JSON bug today, but it is a genuine design/document mismatch.

**Recommendation**: either:

- change `SaveToFile` / `LoadFromFile` to open streams with `std::ios::binary` now, which
  remains safe for JSON text, or
- narrow the design doc wording until binary-safe file transport actually exists.

The first option is cleaner and preserves the intended architecture.

---

## 11. Robustness: InputAction Warning Path Can Assert on Malformed Non-String Fields

**Files**:
- `src/Core/Input/Action/InputActionSerialization.h:230-239`
- `src/Core/Serialization/PropertyTree.cpp:55-59`

When an action binding fails to deserialize, the code tries to extract `"code"`,
`"type"`, and `"kind"` for warning logs:

```cpp
if (srcTree.IsObject() && srcTree.Contains("code"))
    codeName = srcTree["code"].AsString();
```

That looks harmless, but `AsString()` asserts if the stored value is not actually a
string. So malformed user data such as:

```json
{ "type": "Key", "code": 42 }
```

can turn a graceful "warn and skip" path into an assert/crash in debug builds.

This conflicts with the design document's error-handling promise that type mismatches
should warn and skip rather than destabilize the caller.

**Recommendation**: only call `AsString()` after `IsString()`, or better, reuse
`Deserialize(field, std::stringValue)` and treat failure as "no diagnostic token
available".

---

## 12. Robustness: InputAction Device Index Wraps on Invalid Input

**File**: `src/Core/Input/Action/InputActionSerialization.h:99`, `:310`

```cpp
src.DeviceIndex = static_cast<uint8_t>(tree.GetOr<int>("device", 0));
```

```cpp
temp.BindAxis(name, axisCode, static_cast<uint8_t>(axisObj.GetOr<int>("device", 0)));
```

The serialization framework already has a checked `Deserialize(PropertyTree, uint8_t&)`
trait, but `InputActionSerialization` bypasses it and casts an arbitrary `int` directly
to `uint8_t`.

That means malformed values like `-1` or `300` silently wrap to `255` or `44` instead of
being rejected or defaulted.

This is subtle because valid content works fine, but it weakens the malformed-data story
right at a user-facing config boundary.

**Recommendation**: deserialize into a temporary `uint8_t deviceIndex` using the built-in
trait, or manually range-check the `int` before assignment.

---

## 13. Strengths

| Aspect | Assessment |
|--------|------------|
| JSON isolation | Fully achieved. `nlohmann/json` is confined to `JsonBackend.cpp`. |
| Atomic top-level load | `LoadFromFile` deserializes into a temporary and only commits on success. |
| Test coverage | `PropertyTree`, `JsonBackend`, `BuiltinTraits`, and file I/O all have dedicated test coverage, including negative cases. |
| ADL trait discovery | Clean C++20 concept-based discovery without macros or inheritance scaffolding. |
| Consumer integration | `InputActionSerialization` demonstrates that non-trivial domain schemas can sit on top of the generic framework without leaking backend details. |

---

## Verification Notes

The review was cross-checked against the current implementation and test suite. Relevant
test slices pass:

- `rtrlab_unit_tests.exe --gtest_filter=*BuiltinTraits*:*JsonBackend*:*PropertyTree*:*InputActionMapTest.SerializeAndDeserializeChordBindingsRemainCompatibleWithSingleSources`
- `rtrlab_contract_tests.exe --gtest_filter=*SerializationFileIOContractTests*`
