# Resource System Code Review

Status update for the original `ResourceSystem` code review findings after the
resource-system refactor work that landed through 2026-04-11.

> **Overall assessment**: the refactor materially improved the module. The
> biggest architectural changes from the original review are real: source
> catalogs are now built in memory, the public read API is data-returning,
> pak-entry materialization has been removed, and the old Plugin mount model is
> gone. That said, a meaningful subset of the original findings still remains.

---

## Table of Contents

- [Resource System Code Review](#resource-system-code-review)
  - [Table of Contents](#table-of-contents)
  - [Summary](#summary)
  - [1. Bug: Tautological Check (Dead Code)](#1-bug-tautological-check-dead-code)
  - [2. Performance: Double Path Parsing in Read Resolution](#2-performance-double-path-parsing-in-read-resolution)
  - [3. Performance: PakArchive Index Re-Read on Every Access](#3-performance-pakarchive-index-re-read-on-every-access)
  - [4. Performance: MaterializePakEntry Does Not Cache](#4-performance-materializepakentry-does-not-cache)
  - [5. Thread Safety: All-Static State Without Synchronization](#5-thread-safety-all-static-state-without-synchronization)
  - [6. DRY Violation: Source Mount Discovery Logic Still Duplicated](#6-dry-violation-source-mount-discovery-logic-still-duplicated)
  - [7. DRY Violation: SanitizeKeyForPath Duplicated](#7-dry-violation-sanitizekeyforpath-duplicated)
  - [8. Robustness: getenv Return Value Held as string\_view](#8-robustness-getenv-return-value-held-as-string_view)
  - [9. Redundancy: Defensive GetMountRoot Check in ResolveArtifact](#9-redundancy-defensive-getmountroot-check-in-resolveartifact)
  - [10. DRY: Catalog JSON Parsing Duplication Between Runtime and Packaging](#10-dry-catalog-json-parsing-duplication-between-runtime-and-packaging)
  - [11. Security: Catalog Artifact Paths Are Not Validated as Safe Relative Paths](#11-security-catalog-artifact-paths-are-not-validated-as-safe-relative-paths)
  - [12. Robustness: PakArchive Does Not Validate Index/Data Ranges Against File Size](#12-robustness-pakarchive-does-not-validate-indexdata-ranges-against-file-size)
  - [13. Correctness: Source Catalog Physical Path Casing Can Drift from the Real Filesystem](#13-correctness-source-catalog-physical-path-casing-can-drift-from-the-real-filesystem)

---

## Summary

| Status | Count |
|--------|-------|
| Still present | 8 |
| Partially resolved | 3 |
| Resolved | 2 |

| # | Finding | Current status |
|---|---------|----------------|
| 1 | Tautological check in cooked catalog generation | Still present |
| 2 | Double path parsing in read resolution | Partially resolved |
| 3 | Pak index re-read on every access | Still present |
| 4 | `MaterializePakEntry` extraction cache inefficiency | Resolved |
| 5 | Static state without synchronization | Still present |
| 6 | Mount discovery logic duplicated | Partially resolved |
| 7 | `SanitizeKeyForPath` duplicated | Resolved |
| 8 | `getenv` value returned as `string_view` | Still present |
| 9 | Redundant `GetMountRoot` check in catalog resolver | Still present |
| 10 | Catalog JSON parsing duplicated | Partially resolved |
| 11 | Catalog artifact paths not validated as safe relative paths | Still present |
| 12 | Pak index/data ranges not validated against file size | Still present |
| 13 | Source catalog physical-path casing drift | Still present |

---

## 1. Bug: Tautological Check (Dead Code)

**Status**: still present

**File**: `Src/Core/Resource/Cook/CookedCatalog.cpp:529-536`

The original dead-code check remains unchanged:

```cpp
ResourceCatalogEntry cookedEntry;
cookedEntry.logicalPath = sourceEntry.logicalPath;
if (cookedEntry.logicalPath.empty() || cookedEntry.logicalPath != sourceEntry.logicalPath)
{
    // ...
    return false;
}
```

After the assignment on the previous line, the `!= sourceEntry.logicalPath`
sub-expression is still tautologically false. The branch still only checks
whether the logical path is empty.

**Recommendation**: remove the tautological comparison or replace it with the
actual invariant that was intended to be checked.

---

## 2. Performance: Double Path Parsing in Read Resolution

**Status**: partially resolved

**Files**:

- `Src/Core/Resource/FileSystem.cpp:51-99`
- `Src/Core/Resource/FileSystem.cpp:136-173`

The original `ResolveReadPath()` plus `IsCatalogBackedPath()` double-parse is
gone. The refactor removed the old document-vs-catalog branch entirely, and
Project / Engine / DLC / Mod reads now resolve through `ResolveCatalogArtifact()`
only once.

However, there is still residual duplicate parsing on writable-domain reads:

- `ReadText()`, `ReadBinary()`, `OpenReadStream()`, and `Exists()` first call
  `ResolveCatalogArtifact(virtualPath)`, which parses the path.
- If that fails, they call `ResolveWritableReadPath(virtualPath)`, which parses
  the same string again.

That means `/Saved/...` and `/Cache/...` reads still pay two parses even though
their domain could be dispatched once up front.

**Recommendation**: parse once at the `FileSystem` entry point, dispatch on the
already-parsed `VirtualPath`, and route to catalog-backed vs writable-backed
resolution without reparsing.

---

## 3. Performance: PakArchive Index Re-Read on Every Access

**Status**: still present

**Files**:

- `Src/Core/Resource/Package/PakArchive.cpp:307-403`
- `Src/Core/Resource/Package/PakArchive.cpp:548-589`

`FindPakEntry()` still calls `LoadPakIndex()` on every lookup. `ReadPakEntry()`
then reopens the pak and reads the payload. No parsed pak-index cache has been
introduced.

This means repeated access to the same pak still repeatedly:

1. opens the pak
2. rereads the header
3. reparses the full index
4. scans for the target entry
5. reopens the pak again to read the payload

**Recommendation**: cache parsed pak indices by pak path, ideally behind a small
shared runtime helper so repeated `PakEntryExists()` and `ReadPakEntry()` calls
reuse the same index data.

---

## 4. Performance: MaterializePakEntry Does Not Cache

**Status**: resolved

The original issue no longer applies. The refactor removed the materialization
model entirely:

- no public path-returning packaged read path remains
- no `MaterializePakEntry` helper remains
- no `PackagedExtracted/` cache remains in the normal read path

Pak-backed reads now return bytes or streams directly through
`ReadReadableArtifactBinary()`, `ReadReadableArtifactText()`, and
`OpenReadableArtifactStream()`.

**Recommendation**: none for the original finding.

---

## 5. Thread Safety: All-Static State Without Synchronization

**Status**: still present

**Files**:

- `Src/Core/Resource/FileSystem.h:20-56`
- `Src/Core/Resource/Catalog/ResourceCatalog.cpp:432-544`

`FileSystem` still owns static mutable state with no synchronization:

- `s_RootPath`
- `s_EngineDir`
- `s_SavedDir`
- `s_CacheDir`
- `s_CatalogRegistry`
- `s_Initialized`
- `s_WritableDirsResolved`

`CatalogRegistry::ResolveArtifact()` still lazily builds the global table using
`m_GlobalTableBuilt` with no mutex or `std::call_once`. Concurrent first-use
reads would still race on both the flag and the maps being populated.

**Recommendation**: either document the single-threaded initialization contract
explicitly, or protect lazy initialization and writable-dir resolution with
proper synchronization.

---

## 6. DRY Violation: Source Mount Discovery Logic Still Duplicated

**Status**: partially resolved

**Files**:

- `Src/Core/Resource/Mount/MountBackend.cpp:152-173`
- `Src/Core/Resource/Cook/CookedCatalog.cpp:227-254`

The old three-way duplication around Project / Engine / Plugins scanning has
improved significantly:

- the Plugin concept is gone
- the Plugin-specific validation helpers are gone
- source-catalog building no longer has its own mount-discovery copy

But the remaining Project / Engine source-mount discovery rules still exist in
more than one place:

- runtime dev mount discovery in `MountBackend.cpp`
- cook-time source mount discovery in `CookedCatalog.cpp`

Today the duplication is smaller and lower risk, but it still means directory
discovery rules must be kept in sync manually.

**Recommendation**: extract a shared helper that enumerates source mounts and
returns `(VirtualPath, filesystem::path)` pairs for Project / Engine roots.

---

## 7. DRY Violation: SanitizeKeyForPath Duplicated

**Status**: resolved

The duplicate sanitize helpers called out in the original review are no longer
present in the current codebase. The unused copy in `PakArchive.cpp` is gone,
and there is no longer an active duplicated-path-sanitization finding here.

**Recommendation**: none for the original finding.

---

## 8. Robustness: getenv Return Value Held as string_view

**Status**: still present

**File**: `Src/Core/Resource/Mount/RootDiscovery.cpp:36-40`

The original specific site moved, but the pattern still exists:

```cpp
if (const char *envRoot = std::getenv("RTRL_ROOT"))
{
    const std::string_view value = envRoot;
    if (!value.empty())
        return value;
}
```

This remains safe only as long as no environment mutation invalidates the
returned storage before consumption. The current usage is short-lived, but the
non-owning lifetime hazard still exists.

**Recommendation**: return `std::string` instead of `std::string_view`, or add
an explicit comment documenting the lifetime assumption.

---

## 9. Redundancy: Defensive GetMountRoot Check in ResolveArtifact

**Status**: still present

**File**: `Src/Core/Resource/Catalog/ResourceCatalog.cpp:440-449`

`CatalogRegistry::ResolveArtifact()` still computes:

```cpp
const auto mountRoot = GetMountRoot(rootPath, engineDir, virtualPath, projectContentDirName);
if (mountRoot.empty())
    return std::nullopt;
```

That is effectively a defensive domain filter for `Saved` / `Cache`, but the
public caller (`FileSystem`) already routes those domains through writable-mount
resolution instead of catalog resolution.

This is not incorrect, but it is still defensive redundancy that makes the flow
harder to read than an explicit domain assertion would.

**Recommendation**: replace it with an explicit domain precondition or document
that this is deliberate defense-in-depth.

---

## 10. DRY: Catalog JSON Parsing Duplication Between Runtime and Packaging

**Status**: partially resolved

**Files**:

- `Src/Core/Resource/Catalog/ResourceCatalog.cpp:102-237`
- `Src/Core/Resource/Package/PakArchive.cpp:101-139`

This finding has improved compared with the original review.

The old cook-side duplication of full source-catalog parsing is gone because
cook now builds source catalogs in memory through `BuildSourceCatalogEntries()`
instead of reparsing a source `catalog.json` on disk.

There is still some JSON-parsing duplication, though:

- runtime has the general `ParseCatalogFromJson()` path for source/cooked mounts
- packaging still has `LoadCookedCatalogEntries()` for cooked catalog validation
  and extraction before merging `Game.rtrpak`

The remaining duplication is smaller than before, but version checks and cooked
catalog field expectations are still split across two implementations.

**Recommendation**: if cooked-catalog validation grows further, extract a shared
parser or validator so cooked catalog rules live in one place.

---

## 11. Security: Catalog Artifact Paths Are Not Validated as Safe Relative Paths

**Status**: still present

**Files**:

- `Src/Core/Resource/Catalog/ResourceCatalog.cpp:76-99`
- `Src/Core/Resource/Catalog/ResourceCatalog.cpp:185-223`
- `Src/Core/Resource/Mount/MountBackend.cpp:258-289`
- `Src/Core/Resource/Cook/CookedCatalog.cpp:315-424`

`ParseArtifactRecord()` still accepts arbitrary `relativePath` strings from
catalog JSON and stores them without validating that they are safe mount-relative
paths. Source-catalog parsing likewise accepts `sourceRelativePath` as-is.

Those paths are later trusted and joined directly:

- runtime directory-backed reads use `artifact.mountRoot / artifact.relativePath`
- cook copies use `sourceMountRoot / sourceArtifact.relativePath`

So a malformed or hand-edited catalog can still attempt mount-escape reads via
absolute paths or `..` traversal.

**Recommendation**: add a shared safe-relative-path validator for catalog fields
and apply it to both `artifact.relativePath` and `sourceRelativePath` during
parsing/loading.

---

## 12. Robustness: PakArchive Does Not Validate Index/Data Ranges Against File Size

**Status**: still present

**Files**:

- `Src/Core/Resource/Package/PakArchive.cpp:307-386`
- `Src/Core/Resource/Package/PakArchive.cpp:548-582`

`LoadPakIndex()` still validates magic/version and safe entry paths, but it does
not validate structural offsets and sizes against the actual pak file size.

Unchecked values still include:

- `header.indexOffset`
- `header.indexSize`
- `header.entryCount`
- per-entry `dataOffset`
- per-entry `dataSize`
- per-entry `pathLength`

`ReadPakEntry()` still trusts `dataSize` enough to allocate a buffer before the
payload read has proved that the range is sane.

**Recommendation**: validate the full archive structure against the physical file
size during index load, including overflow-safe range checks for index and data
regions.

---

## 13. Correctness: Source Catalog Physical Path Casing Can Drift from the Real Filesystem

**Status**: still present

**File**: `Src/Core/Resource/Catalog/SourceCatalog.cpp:37-57, 202-207`

The underlying asymmetry remains:

- logical paths are canonicalized through `MakeLogicalPath()` for known segment
  names such as `textures -> Textures`
- physical stored paths (`sourceRelativePath` and artifact `relativePath`) keep
  the scanned filesystem spelling

That still allows a catalog entry such as:

- logical path: `/Project/Textures/Grassy_Square`
- physical path: `textures/Grassy_Square.jpg`

On case-insensitive filesystems this often works silently. On case-sensitive
filesystems or exact-path comparisons, the inconsistency remains observable.

**Recommendation**: either enforce exact physical-path casing, or define and
apply a consistent normalization rule for physical artifact paths during source
catalog construction.
