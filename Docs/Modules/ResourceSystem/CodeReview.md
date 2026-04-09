# Resource System Code Review

Code review of the `src/Core/Resource/` module and associated tooling, based on the
implementation as of 2026-04-09.

> **Overall assessment**: code quality is high -- clean layering, consistent error
> handling, and good alignment between design document and implementation. The findings
> below are organized by severity.

---

## Table of Contents

- [Resource System Code Review](#resource-system-code-review)
  - [Table of Contents](#table-of-contents)
  - [Summary](#summary)
  - [1. Bug: Tautological Check (Dead Code)](#1-bug-tautological-check-dead-code)
  - [2. Performance: Double Path Parsing in ResolveReadPath](#2-performance-double-path-parsing-in-resolvereadpath)
  - [3. Performance: PakArchive Index Re-Read on Every Access](#3-performance-pakarchive-index-re-read-on-every-access)
  - [4. Performance: MaterializePakEntry Does Not Cache](#4-performance-materializepakentry-does-not-cache)
  - [5. Thread Safety: All-Static State Without Synchronization](#5-thread-safety-all-static-state-without-synchronization)
  - [6. DRY Violation: Mount Discovery Logic Duplicated Three Times](#6-dry-violation-mount-discovery-logic-duplicated-three-times)
  - [7. DRY Violation: SanitizeKeyForPath Duplicated](#7-dry-violation-sanitizekeyforpath-duplicated)
  - [8. Robustness: getenv Return Value Held as string\_view](#8-robustness-getenv-return-value-held-as-string_view)
  - [9. Redundancy: Unused GetMountRoot Call in ResolvePath](#9-redundancy-unused-getmountroot-call-in-resolvepath)
  - [10. DRY: Catalog JSON Parsing Duplicated Between Runtime and Cook](#10-dry-catalog-json-parsing-duplicated-between-runtime-and-cook)

---

## Summary

| Category | Count |
|----------|-------|
| Bug (dead code) | 1 |
| Performance (optimizable, non-blocking) | 3 |
| Thread safety (constraint needs documenting) | 1 |
| DRY / maintainability | 3 |
| Style / defensive redundancy | 2 |

The core architecture (logical path model, mount precedence, catalog merge, artifact
selection scoring) is solid and matches the design document faithfully.

---

## 1. Bug: Tautological Check (Dead Code)

**File**: `src/Core/Resource/Cook/CookedCatalog.cpp:675-681`

```cpp
ResourceCatalogEntry cookedEntry;
cookedEntry.logicalPath = sourceEntry.logicalPath;
if (cookedEntry.logicalPath.empty() || cookedEntry.logicalPath != sourceEntry.logicalPath)
{
    // ...
    return false;
}
```

`cookedEntry.logicalPath` was just assigned from `sourceEntry.logicalPath` on the
previous line. The `!= sourceEntry.logicalPath` sub-expression is always `false`. The
only way this branch can trigger is if `sourceEntry.logicalPath` itself is empty, which
makes the second condition dead code.

**Recommendation**: either remove the tautological sub-expression, or replace it with
the actual invariant that was intended to be checked.

---

## 2. Performance: Double Path Parsing in ResolveReadPath

**File**: `src/Core/Resource/FileSystem.cpp:59-65`

```cpp
const auto virtualPath = Resource::ParseVirtualPath(virtualPathString);
// ...
if (Resource::IsCatalogBackedPath(virtualPathString))  // internally calls ParseVirtualPath again
```

`IsCatalogBackedPath` internally calls `ParseVirtualPath` a second time on the same
input string. The already-parsed `VirtualPath` result contains all the information
needed to determine catalog-backed status (domain + whether the relative path has an
extension).

The same double-parse also occurs indirectly through `Exists`, `ReadText`, `ReadBinary`,
`WriteText`, and `WriteBinary`, since they all delegate to `ResolveReadPath` or
`ResolveWritePath`.

**Recommendation**: add an overload or internal helper that accepts the already-parsed
`VirtualPath` and checks domain + extension directly, avoiding the redundant parse.

---

## 3. Performance: PakArchive Index Re-Read on Every Access

**File**: `src/Core/Resource/Package/PakArchive.cpp:141-157`

`FindPakEntry` opens the pak file, reads the full header, seeks to the index, and parses
every index entry on every call. `ReadPakEntry` then opens the same file a second time to
read the actual entry data. `MaterializePakEntry` calls `ReadPakEntry`, repeating the
entire process again.

The current `MountCatalogCache` in `CatalogRegistry` only caches the parsed catalog JSON
results. The pak's internal index (entry offset/size table) is never cached.

**Recommendation**: cache the parsed pak index (the `vector<PakIndexEntry>`) on first
access, keyed by pak file path. This avoids re-reading the index on subsequent lookups.

---

## 4. Performance: MaterializePakEntry Does Not Cache

**File**: `src/Core/Resource/Package/PakArchive.cpp:354-390`

Every call re-reads the entry from the pak and re-writes it to disk, even if the same
entry was already extracted to the same output path.

**Recommendation**: before extracting, check whether the target file already exists and
has the expected size. This avoids redundant I/O on repeated accesses to the same
packaged asset.

---

## 5. Thread Safety: All-Static State Without Synchronization

**File**: `src/Core/Resource/FileSystem.h` (entire class)

All state in `FileSystem` is static with no mutex or atomic protection.
`CatalogRegistry::ResolvePath` lazily builds the global table on its first call
(`m_GlobalTableBuilt` flag + populating `m_GlobalEntries`). If multiple threads issue
their first `ResolveReadPath` concurrently, they will race on the flag and on the global
table contents.

This is not a bug if the system is only used from a single thread, but that constraint
is not documented anywhere.

**Recommendation**: either add a `// Not thread-safe` comment and document the
single-threaded usage constraint, or protect the lazy initialization with a
`std::call_once` / mutex.

---

## 6. DRY Violation: Mount Discovery Logic Duplicated Three Times

The pattern "scan Project/Engine/Plugins directories, sort by name, validate with
`IsValidPluginMountName`" is implemented independently in three places:

1. `src/Core/Resource/Catalog/SourceCatalog.cpp:99-144`
   (`DiscoverReadableSourceMounts`)
2. `src/Core/Resource/Cook/CookedCatalog.cpp:227-286`
   (`DiscoverReadableSourceMounts`, same name, different anonymous namespace)
3. `src/Core/Resource/Mount/MountBackend.cpp:244-342`
   (`DiscoverReadableMountBackends`, the Project/Engine/Plugins portion)

All three have the same iteration structure, the same sort-by-filename, and the same
`IsValidPluginMountName` check. If mount discovery rules change in the future, all three
must be updated in sync.

**Recommendation**: extract a shared helper that returns a list of
`(VirtualPath, filesystem::path)` pairs for all discovered source mounts. The three
callsites can then consume this list and add their own context (cooked output roots,
backend tags, etc.).

---

## 7. DRY Violation: SanitizeKeyForPath Duplicated

`SanitizeMountKeyForPath` in `src/Core/Resource/Mount/MountBackend.cpp:15-27` and
`SanitizeKeyForPath` in `src/Core/Resource/Package/PakArchive.cpp:159-171` are
functionally identical (replace non-alphanumeric/underscore/hyphen characters with `_`).

The copy in `PakArchive.cpp` appears to be unused.

**Recommendation**: remove the unused copy in `PakArchive.cpp`. If both are actually
needed, move the function to a shared utility.

---

## 8. Robustness: getenv Return Value Held as string_view

**File**: `src/Core/Resource/Catalog/ResourceCatalog.cpp:263-266`

```cpp
if (const char *overrideValue = std::getenv("RTRLAB_RESOURCE_PROFILE"))
{
    const std::string_view value = overrideValue;
    if (!value.empty()) return value;
}
```

The returned `string_view` points to memory owned by the `getenv` implementation. In the
current usage pattern (the value is consumed immediately within `ChooseArtifact`), this
is safe. However, if any code between this return and the final consumption calls
`putenv` / `setenv`, the `string_view` would dangle.

**Recommendation**: either add a comment documenting the lifetime constraint, or change
the return type to `std::string` to own the value.

---

## 9. Redundancy: Unused GetMountRoot Call in ResolvePath

**File**: `src/Core/Resource/Catalog/ResourceCatalog.cpp:408-409`

```cpp
const auto mountRoot = GetMountRoot(rootPath, engineDir, virtualPath, projectContentDirName);
if (mountRoot.empty()) return std::nullopt;
```

This computation is only used as an empty check to early-return for `Saved` / `Cache`
domains. But those domains should never reach `ResolvePath` in the first place, because
`FileSystem::ResolveReadPath` already filters them out via `IsCatalogBackedPath`.

This is defensive redundancy, not a bug, but it adds cognitive overhead for readers
trying to understand the flow.

**Recommendation**: replace with an explicit domain check or add a comment explaining
that this is a defense-in-depth guard.

---

## 10. DRY: Catalog JSON Parsing Duplicated Between Runtime and Cook

`LoadCatalogEntries` in `src/Core/Resource/Cook/CookedCatalog.cpp:288-392` implements a
complete catalog JSON parser (version check, entries iteration, artifact parsing) that
substantially overlaps with `ParseCatalogFromJson` in
`src/Core/Resource/Catalog/ResourceCatalog.cpp:82-206`.

The cook-side parser is simpler because it only handles version-1 source catalogs, but
the JSON field names and parsing rules must be kept in sync across both implementations.

**Recommendation**: consider extracting the shared parsing logic into a common helper.
The cook-side could call the shared parser with a version filter, or the shared parser
could accept a mode flag to skip cooked-only validation.
