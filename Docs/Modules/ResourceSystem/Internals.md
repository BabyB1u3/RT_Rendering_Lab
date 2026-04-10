# Resource System Internals

This document describes how the resource system actually works at the code level, and
compares RTRLab's approach with the resource/asset systems found in modern game engines.

It is intended as a companion to the main
[ResourceSystem.md](ResourceSystem.md) design document, which focuses on the public
contract and design rationale. This document focuses on implementation mechanics and
industry context.

> **Snapshot date**: 2026-04-10

---

## Table of Contents

- [Resource System Internals](#resource-system-internals)
  - [Table of Contents](#table-of-contents)
  - [Part I: How the System Works](#part-i-how-the-system-works)
    - [1. End-to-End Flow Overview](#1-end-to-end-flow-overview)
    - [2. Offline Toolchain](#2-offline-toolchain)
      - [2.1 Source Indexing (rtr\_asset\_index)](#21-source-indexing-rtr_asset_index)
      - [2.2 Cooking (rtr\_asset\_cook)](#22-cooking-rtr_asset_cook)
      - [2.3 Packaging (rtr\_asset\_pack)](#23-packaging-rtr_asset_pack)
    - [3. Runtime Resolution](#3-runtime-resolution)
      - [3.1 Initialization](#31-initialization)
      - [3.2 Path Classification](#32-path-classification)
      - [3.3 Document Path Resolution](#33-document-path-resolution)
      - [3.4 Catalog-Backed Path Resolution](#34-catalog-backed-path-resolution)
      - [3.5 Write Path Resolution](#35-write-path-resolution)
    - [4. Runtime Data Flow Walkthrough](#4-runtime-data-flow-walkthrough)
    - [5. Key Implementation Details](#5-key-implementation-details)
  - [Part II: Comparison with Modern Game Engines](#part-ii-comparison-with-modern-game-engines)
    - [6. Asset Identity Model](#6-asset-identity-model)
    - [7. Import / Cook Pipeline](#7-import--cook-pipeline)
    - [8. Catalog / Registry Architecture](#8-catalog--registry-architecture)
    - [9. Mount / Virtual File System](#9-mount--virtual-file-system)
    - [10. Archive / Packaging Format](#10-archive--packaging-format)
    - [11. Mechanisms Present in Mature Engines but Absent in RTRLab](#11-mechanisms-present-in-mature-engines-but-absent-in-rtrlab)
    - [12. Overall Positioning](#12-overall-positioning)
    - [13. Recommended Priorities](#13-recommended-priorities)

---

## Part I: How the System Works

### 1. End-to-End Flow Overview

The system operates in two phases: an **offline toolchain** (index, cook, pack) and a
**runtime resolution** layer (FileSystem API).

```
Offline phase (build time)                Runtime phase
+---------------+                        +----------------------+
| rtr_asset_    |  scan source files     | FileSystem::         |
|   index       |-> generate catalog.json| ResolveReadPath      |
+---------------+                        +----------+-----------+
       |                                            |
       v                                            v
+---------------+                        +----------------------+
| rtr_asset_    |  decode images->.rtrtex | Is path document     |-> direct physical join
|   cook        |-> generate cooked cat   | or catalog-backed?   |
+---------------+                        +----------+-----------+
       |                                            | catalog-backed
       v                                            v
+---------------+                        +----------------------+
| rtr_asset_    |  bundle into .rtrpak   | CatalogRegistry      |-> global table lookup
|   pack        |-> binary archive       | ::ResolvePath         |   + artifact selection
+---------------+                        +----------------------+
```

---

### 2. Offline Toolchain

#### 2.1 Source Indexing (`rtr_asset_index`)

- Entry: `src/Tools/AssetIndexMain.cpp` -> `Resource::IndexRepositorySourceCatalogs()`
- Implementation: `src/Core/Resource/Catalog/SourceCatalog.cpp`

**What it does**: recursively scans mount roots and can generate a `.rtr/catalog.json`
per mount.

**Steps** (`SourceCatalog.cpp:304-319`):

1. **Discover mounts**: scan for `Project/` and `Engine/` and collect each existing
   directory as a source mount.

2. **Scan files** (`SourceCatalog.cpp:167-191`): for each mount root, run a
   `recursive_directory_iterator`. Skip the `.rtr/` metadata directory and any top-level
   `Config/` subtree (config files are documents, not catalog-backed assets).

3. **Derive logical paths** (`SourceCatalog.cpp:44-89`):
   - take each file's relative path, strip the extension
   - normalize known directory names to canonical casing (`textures` -> `Textures`,
     `shaders` -> `Shaders`, etc.)
   - prepend the domain prefix:
     `Project/Textures/Grassy_Square.jpg` -> `/Project/Textures/Grassy_Square`

4. **Enforce uniqueness**: if two source files generate the same logical path (e.g.
   `foo.jpg` and `foo.png` in the same directory), the indexer rejects the catalog with
   an error.

5. **Write JSON** (`SourceCatalog.cpp:238-302`): produce a version-1 `catalog.json` with
   one entry per file. Each entry contains `logicalPath`, `sourceRelativePath`, and an
   `artifacts` array (default tags: `profileTag="dev"`, `backendTag="any"`,
   `platformTag="any"`).

**Example output** (`Project/.rtr/catalog.json`):

```json
{
  "version": 1,
  "entries": [
    {
      "logicalPath": "/Project/Textures/Grassy_Square",
      "sourceRelativePath": "Textures/Grassy_Square.jpg",
      "artifacts": [{
        "relativePath": "Textures/Grassy_Square.jpg",
        "format": "jpg",
        "profileTag": "dev",
        "backendTag": "any",
        "platformTag": "any"
      }]
    }
  ]
}
```

#### 2.2 Cooking (`rtr_asset_cook`)

- Entry: `src/Tools/AssetCookMain.cpp` -> `Resource::CookRepositoryCatalogs()`
- Implementation: `src/Core/Resource/Cook/CookedCatalog.cpp`

**What it does**: scans each source mount in memory, converts source files to
runtime-ready artifacts, and writes them to a cooked output directory.

**Steps** (`CookedCatalog.cpp:644-703`):

1. Discover all source mounts and determine the corresponding cooked output directory
   (e.g. `Saved/Cache/Cooked/Project/`).

2. Build source catalog entries in memory for each mount.

3. For each source artifact, call `CopySourceArtifact`:
   - **Image files** (logical path starts with `Textures/`, format is jpg/png): decode
     with `stbi_load` to RGBA8 pixels, write as a `.rtrtex` file (V2 header + raw pixel
     data).
   - **Other files**: straight `copy_file`.

4. Write a version-2 cooked catalog (`kind: "cooked"`). Artifact `profileTag` becomes
   `"cooked"`, and `relativePath` points to the cooked product.

**`.rtrtex` V2 binary layout** (`CookedCatalog.cpp:39-53`):

```
Offset  Size  Field
0       8     magic "RTRTEX01"
8       4     version (2)
12      4     width
16      4     height
20      4     channelCount (4)
24      4     pixelFormat (1 = RGBA8_UNorm)
28      4     mipLevelCount (1)
32      4     rowPitch (width * 4)
36      4     dataOffset (44)
40      4     dataSize
44      ...   raw RGBA8 pixel data
```

#### 2.3 Packaging (`rtr_asset_pack`)

- Entry: `src/Tools/AssetPackMain.cpp` -> `Resource::PackageCookedRepositoryCatalogs()`
- Implementation: `src/Core/Resource/Package/PakArchive.cpp`

**What it does**: bundles all files under each cooked mount (including its catalog) into a
single `.rtrpak` binary archive.

**`.rtrpak` binary layout** (`PakArchive.cpp:14-21`):

```
+------------------------+
| PakHeader (32 bytes)   |  magic "RTRPAK01", version, entryCount, indexOffset, indexSize
+------------------------+
| Entry 0 data           |  raw file bytes, back-to-back
| Entry 1 data           |
| ...                    |
+------------------------+  <- indexOffset
| Index Entry 0          |  pathLength(u32) + dataOffset(u64) + dataSize(u64) + path(bytes)
| Index Entry 1          |
| ...                    |
+------------------------+
```

**Output**: `Saved/Cache/Packaged/Project.rtrpak`, `Engine.rtrpak`.

---

### 3. Runtime Resolution

The public entry point is always a static method on the `FileSystem` class.

#### 3.1 Initialization

`FileSystem::Init()` (`FileSystem.cpp:26-32`):

1. Calls `Resource::DiscoverRootPath()` to locate the project root.
2. Sets `s_EngineDir = rootPath / "Engine"`.
3. Resets the catalog registry.

`DiscoverRootPath()` now has two compile-time strategies:

- **Development builds**:
  - `RTRL_ROOT` environment variable, but only if that directory contains a
    `.rtrproject` marker
  - walk up from the executable location (max 5 levels) looking for `.rtrproject`
  - `GLAB_ROOT_DIR` compile-time fallback, again requiring `.rtrproject`
  - current working directory, if it contains `.rtrproject`
  - otherwise log loudly and fall back to `current_path()`
- **Shipping builds** (`RTRL_SHIPPING`):
  - treat the executable directory as the install root
  - skip marker search and path override logic entirely

Writable directories (`s_SavedDir`, `s_CacheDir`) are **lazily initialized** on first
access via `GetSavedDir()` / `GetCacheDir()`. In development mode (`GLAB_ROOT_DIR`
defined), they resolve to `rootPath/Saved` and `rootPath/Saved/Cache`. Otherwise they
use platform-specific user data paths:
- Windows: `%LOCALAPPDATA%/RTRLab/Saved` and `%LOCALAPPDATA%/RTRLab/Cache`
- macOS: `~/Library/Application Support/RTRLab/Saved` and `~/Library/Caches/RTRLab`
- Linux: `$XDG_DATA_HOME/RTRLab/Saved` and `$XDG_CACHE_HOME/RTRLab`

#### 3.2 Path Classification

When `ResolveReadPath("/Project/Textures/Grassy_Square")` is called:

**Step 1: Parse the virtual path** (`PathParser.cpp:66-140`)
- Validate: must start with `/`, must not contain `\`, must not contain `.` or `..`
  segments.
- Split into segments, collapse consecutive `/`.
- Determine domain from the first segment: `Project` / `Engine` / `Saved` / `Cache`.

Result: `VirtualPath { domain=Project, mountName=nullopt, relativePath="Textures/Grassy_Square" }`

**Step 2: Classify as document or catalog-backed** (`PathParser.cpp:142-167`)
- `Saved` / `Cache` domain: never catalog-backed.
- `Project` / `Engine` domain: check whether the relative path has a file extension.
  - Has extension (e.g. `.json`): document path, resolved via direct physical join.
  - No extension: catalog-backed, resolved through catalog lookup.

This implements the design contract: **extensionless = asset, has extension = document**.

#### 3.3 Document Path Resolution

`FileSystem.cpp:76-78` -> `Resource::ResolvePhysicalPath()`

Direct domain-to-directory mapping:

```
/Project/Config/input/Foo.json  ->  {root}/Project/Config/input/Foo.json
/Engine/Config/Foo.json         ->  {root}/Engine/Config/Foo.json
/Saved/Config/imgui.ini         ->  {savedDir}/Config/imgui.ini
```

#### 3.4 Catalog-Backed Path Resolution

`FileSystem.cpp:66-73` -> `CatalogRegistry::ResolvePath()`

This is the most complex path, involving mount discovery, catalog loading, global table
merging, and artifact selection.

**Step A: Lazily build the global table** (first call only,
`ResourceCatalog.cpp:412-445`)

1. Call `DiscoverReadableMountBackends()` to discover all readable mounts. This function
   decides which backends to register based on the current profile:

   **Profile decision logic** (`MountBackend.cpp:39-40`):
   - `"dev"` profile (default): register source mount only; fall back to
     packaged/cooked only if source does not exist.
   - `"cooked"` profile: prefer cooked, then source.
   - `"packaged"` / `"shipping"` profile: prefer packaged, then cooked, then source.

   For each domain (Project / Engine), register mounts according to the profile
   strategy, assigning priorities:

   ```
   Overlay  (300)  >  Packaged (200)  >  Cooked (100)  >  Source (0)
   ```

   Overlay mounts are discovered from `Saved/Overrides/` or the `RTRLAB_OVERLAY_ROOT`
   environment variable.

2. For each discovered mount, acquire its catalog:
   - Dev source directory backend (`Project/` or `Engine/`, source priority): build the
     source catalog in memory from the source tree.
   - Other directory backends: read `{mountRoot}/.rtr/catalog.json` from disk.
   - PakArchive backend: extract `.rtr/catalog.json` from inside the `.rtrpak` file.

3. Parse the catalog JSON, validate version (v1 = source, v2 = cooked), and verify that
   every entry's `logicalPath` belongs to the mount's domain.

4. **Merge into global table** (`ResourceCatalog.cpp:334-388`):
   - A higher-priority mount's entry replaces a lower-priority one
     (overlay > packaged > cooked > source).
   - Two equal-priority mounts providing the same `logicalPath` are treated as a
     **conflict**: the path is removed from the global table and recorded in
     `m_ConflictedLogicalPaths`. Future lookups for that path return `nullopt`.
   - A lower-priority mount cannot replace a higher-priority one.

**Step B: Global table lookup** (`ResourceCatalog.cpp:448-453`)

Look up the logical path string in `m_GlobalEntries` (`unordered_map`). First check
whether it appears in the conflict set.

**Step C: Artifact selection** (`ResourceCatalog.cpp:305-332`)

A catalog entry can have multiple artifacts (e.g. variants for different
platforms/backends/profiles). The best match is chosen by scoring:

```
Scoring rules:
  Each tag is scored independently:
    - exact match to the current runtime tag  -> 2 points
    - "any" (wildcard)                        -> 1 point
    - explicit mismatch                       -> -1 (rejected)

  Total score = profileScore * 100 + backendScore * 10 + platformScore
  The artifact with the highest total score wins.
```

Runtime tag sources:
- `platformTag`: determined at compile time (`windows` / `macos` / `linux`).
- `backendTag`: determined at compile time (`metal` or `opengl`).
- `profileTag`: from the `RTRLAB_RESOURCE_PROFILE` environment variable, or
  compile-time default (`dev` / `shipping`). For packaged/shipping profiles, the
  artifact profile tag is mapped to `"cooked"`.

**Step D: Artifact physical path resolution** (`MountBackend.cpp:395-408`)

- Directory backend: return `mountRoot / artifact.relativePath`.
- PakArchive backend: extract the entry from the pak to
  `{cacheDir}/PackagedExtracted/{sanitizedKey}/{relativePath}`, and return the extracted
  physical path.

#### 3.5 Write Path Resolution

`FileSystem.cpp:80-96`:

Only `/Saved/` and `/Cache/` domains are allowed. `ResolveWritableMount` returns
`nullopt` for all other domains. Parent directories are created automatically before
returning the resolved path.

---

### 4. Runtime Data Flow Walkthrough

```
User calls: FileSystem::ReadBinary("/Project/Textures/Grassy_Square")
  |
  +- ParseVirtualPath -> {Project, "Textures/Grassy_Square"}
  +- IsCatalogBackedPath -> true (no extension + not Saved/Cache)
  |
  +- CatalogRegistry::ResolvePath
       |
       +- [first call] DiscoverReadableMountBackends
       |    +- Project/ exists           -> register Source:Project    (priority=0)
       |    +- Cooked/Project/ has cat   -> register Cooked:Project   (priority=100)
       |    +- Packaged/Project.rtrpak   -> register Packaged:Project (priority=200)
       |    +- Overrides/Project/ has cat-> register Overlay:Project  (priority=300)
       |
       +- [first call] Load catalog for each mount -> parse JSON -> merge into global table
       |
       +- Lookup: m_GlobalEntries["/Project/Textures/Grassy_Square"]
       |    -> found entry from the highest-priority mount
       |
       +- ChooseArtifact -> select best artifact
       |    e.g. {relativePath: "Textures/Grassy_Square.rtrtex", profileTag: "cooked"}
       |
       +- ResolveReadableMountArtifact
            +- Directory backend -> return mountRoot/Textures/Grassy_Square.rtrtex
            +- PakArchive backend -> extract from .rtrpak to cache -> return extracted path

  ReadBinary receives the physical path, calls Resource::ReadBinaryFile, returns bytes.
```

---

### 5. Key Implementation Details

| Mechanism | Implementation Location | Key Point |
|-----------|------------------------|-----------|
| Extension = document, no extension = asset | `PathParser::HasDocumentExtension` | The sole criterion that routes to document vs catalog resolution |
| Overlay overrides everything | `MountPriority::Overlay = 300` | Development-time `Saved/Overrides/` can replace any resource |
| Equal-priority conflict = removal | `MergeMountEntriesIntoGlobalTable` | No implicit choice; the path becomes unavailable |
| Catalog is lazily built | `m_GlobalTableBuilt` flag | All mounts are scanned on the first `ResolvePath` call |
| Pak entries materialize to cache | `MaterializePakEntry` | Runtime still works with physical file paths, avoiding consumer changes |
| Profile determines mount registration order | `DiscoverReadableMountBackends` | `dev` prefers source; `shipping` prefers packaged |
| Writable dirs are lazily resolved | `FileSystem::ResolveWritableDirs` | First call to `GetSavedDir()` / `GetCacheDir()` triggers resolution |
| Dev root discovery walks up from executable | `FindRootFromExecutable` | Up to 5 parent directories, looking for `.rtrproject` |
| Shipping root discovery uses install dir | `DiscoverRootPath` under `RTRL_SHIPPING` | Executable directory is treated as the root |

---

## Part II: Comparison with Modern Game Engines

### 6. Asset Identity Model

| Dimension | RTRLab | Unreal Engine | Unity | Godot |
|-----------|--------|---------------|-------|-------|
| Primary identity | Logical path `/Project/Textures/Grassy_Square` | Package path `/Game/Textures/Grassy_Square` | GUID (128-bit) | `res://textures/grassy_square.png` |
| Extension | Intentionally stripped | Intentionally stripped | Not exposed in references | Source extension retained |
| Case sensitivity | Case-sensitive on all platforms | Case-insensitive (`FName`) | N/A (GUID) | Platform-dependent |
| Rename/move | **References break** | Redirector auto-repair | GUID unchanged, references survive | References break (`.import` mitigates) |

**Analysis**:

RTRLab uses a **logical-path-as-identity** model closest to Unreal, but lacks two
mechanisms that Unreal and Unity both have:

1. **GUID / Stable ID**: Unity's core design gives every asset a path-independent GUID
   (stored in `.meta` files). Moving or renaming an asset does not change its GUID, so
   references survive. Unreal has `FPrimaryAssetId` and `FSoftObjectPath`, and while it
   primarily uses paths, it has a Redirector system that creates forwarding stubs on
   rename. RTRLab relies purely on paths; if an asset is moved, all serialized references
   and catalogs are invalidated. This is not a problem at the current content scale, but
   will become a pain point as the content library grows.

2. **Case-sensitive strategy**: RTRLab chose case-sensitive on all platforms, which is
   stricter than Unreal's case-insensitive approach. The benefit is more predictable
   cross-platform consistency (the design document explains the rationale). The cost is
   that on Windows, a file can exist on disk but fail to match a logical path due to
   casing differences, producing a silent failure. Unreal avoids this by forcing
   lowercase.

### 7. Import / Cook Pipeline

| Dimension | RTRLab | Unreal Engine | Unity |
|-----------|--------|---------------|-------|
| Import trigger | Manual `rtr_asset_index` | Automatic on editor launch / file change | Automatic on file drop into Assets/ |
| Cook trigger | Manual `rtr_asset_cook` | Automatic (`Cook Content` or on package) | Automatic on build |
| Incremental cook | **None** -- full rebuild every time | Yes -- DDC (Derived Data Cache) keyed by content hash | Yes -- Library cache keyed by hash |
| Dependency tracking | **None** | Yes -- Asset Registry tracks dependency graph | Yes -- `.meta` + dependency graph |
| Cooked artifact format | `.rtrtex` (textures only) | `.uasset` / `.ubulk` (unified container) | Platform bundle |
| Supported asset types | Only jpg/png to RGBA8 textures; everything else copied verbatim | Every asset type has a dedicated cooker | Every asset type |

**Analysis**:

RTRLab's cook pipeline is a minimal bootstrap implementation. Three structural gaps
relative to mature engines:

**a) No incremental build**. `CookRepositoryCatalogs` iterates all source entries every
time, re-decoding and re-writing each one. Unreal's DDC uses content hashes as cache
keys and only re-cooks changed content. RTRLab's `ArtifactRecord` already has a
`contentHash` field (currently always 0), which is reserved for incremental cooking but
not yet wired up. Full-rebuild cook will become slow once asset counts reach the hundreds.

**b) No dependency tracking**. If a material references a texture, and the texture is
re-cooked, does the material need rebuilding? The current system cannot answer this
question because there is no dependency graph. Unreal's Asset Registry maintains a
complete reference graph (referencers / dependencies); Unity uses `.meta` files and
`AssetDatabase` for the same purpose. This is not a problem while only textures are
cooked, but will be needed once materials, shaders, and scenes enter the cook pipeline.

**c) No automatic trigger**. Source file changes do not trigger re-indexing or re-cooking.
This is an intentional design choice (tools and runtime are separate executables), but it
means a developer must manually run two commands after modifying a texture. Unreal and
Unity handle this transparently in editor mode.

### 8. Catalog / Registry Architecture

| Dimension | RTRLab | Unreal Engine | Unity |
|-----------|--------|---------------|-------|
| Registry format | JSON catalog per mount | `AssetRegistry.bin` (binary, global) | Library/metadata (SQLite-like) |
| Runtime loading | Lazy, full build on first `ResolvePath` | Loaded at startup | Loaded at startup |
| Lookup complexity | O(1) `unordered_map` | O(1) `TMap` | O(1) |
| Runtime writable | No | No (writable in editor mode) | No |
| Global uniqueness | Within-mount uniqueness at index time; cross-mount conflict removal at merge time | Globally guaranteed | GUID globally unique |

**Analysis**:

RTRLab's catalog architecture is clean: each mount has an independent catalog, and they
are merged into a global table at runtime. This is similar to Unreal's chunk-based
AssetRegistry (Unreal stores a local registry inside each pak and merges them at
startup).

Two differences worth noting:

**a) JSON vs binary**. RTRLab uses JSON throughout. Unreal's `AssetRegistry.bin` is a
compact custom binary format that loads extremely fast. Unity's Library is also binary.
When catalogs contain thousands of entries, JSON parsing overhead (memory allocations and
CPU) will become a startup bottleneck. The design document (section 12.1) explicitly
acknowledges and defers this.

**b) Conflict handling strategy**. RTRLab makes the most conservative choice for
equal-priority conflicts: remove the path entirely. Unreal has finer-grained handling for
same-name assets (by package priority, load order), allowing controlled overrides in
DLC/mod scenarios. RTRLab's "conflict = removal" is safer for correctness, but may need a
more flexible conflict policy as overlay/mod scenarios grow more complex.

### 9. Mount / Virtual File System

| Dimension | RTRLab | Unreal Engine | Unity | id Tech / Source |
|-----------|--------|---------------|-------|------------------|
| Mount hierarchy | Project > Engine > Saved > Cache | /Game, /Engine, /Plugin, /Temp | No hierarchy concept | Search-path-based pak stack |
| Override mechanism | 4-level priority (Overlay > Packaged > Cooked > Source) | Pak priority + patch pak | AssetBundle variants | Later-mounted pak overrides earlier |
| Runtime dynamic mount | **Not supported** | Supported (`MountPak` / `UnmountPak`) | Supported (`LoadAssetBundle`) | Supported |
| Write domain isolation | Hard-coded: only Saved/Cache writable | Similar (Saved/ writable) | `Application.persistentDataPath` | Dedicated write path |

**Analysis**:

RTRLab's mount model is very similar to Unreal's: a fixed set of domains
(Project/Engine plus writable Saved/Cache), strict separation of read-only content and
writable user data. The overlay mechanism is equivalent to Unreal's patch pak.

**Key gap: no runtime dynamic mounting.** `CatalogRegistry`'s global table is built once
on the first `ResolvePath` call and does not change afterwards (`RefreshCatalogs` can
rebuild it, but is not incremental). Unreal supports runtime `FCoreDelegates::MountPak`
to dynamically mount new paks (for DLC / hot updates) and incrementally update the
registry. This is not needed at RTRLab's current scale, but would be required for mod
support or runtime content downloads.

### 10. Archive / Packaging Format

| Dimension | RTRLab `.rtrpak` | Unreal `.pak` | Unity AssetBundle | Godot `.pck` |
|-----------|------------------|---------------|-------------------|--------------|
| Compression | **None** | Optional (zlib/oodle/lz4 per chunk) | LZMA/LZ4 | Optional |
| Encryption | **None** | AES-256 per pak | Optional | Optional |
| Alignment | **None** | Configurable (sector-aligned for HDD/SSD) | Yes | None |
| Index location | End of file | End of file | Header | End of file |
| Single-entry read | Re-opens file + re-reads full index every time | Index resident in memory, direct seek+read | Resident | Index resident |
| Streaming | Materialize to cache, then read file | Direct stream from pak | Direct stream | Direct stream |

**Analysis**:

RTRLab's pak format is a minimum viable archive. Key gaps relative to Unreal's `.pak`:

**a) No compression**. Unreal's pak supports per-entry or per-block compression; Unity
does too. RTRLab's RGBA8 textures are uncompressed raw data packed directly, leading to
large storage sizes. This is acceptable with only a few textures but grows linearly.

**b) Index not resident in memory**. Unreal reads the pak index into memory at startup
and keeps it resident; subsequent reads require only a single seek+read. RTRLab reopens
the file and reads the full header+index on every `FindPakEntry` call.

**c) Materialization vs streaming**. RTRLab extracts pak entries to cache-backed physical
files and then lets consumers read those files. Unreal and Unity support streaming
directly from inside the pak without materialization. The design document (section 12.5)
explicitly acknowledges this as an intentional trade-off: current runtime consumers
expect physical file paths, and materialization avoids rewriting all consumers.

### 11. Mechanisms Present in Mature Engines but Absent in RTRLab

| Missing Mechanism | Unreal / Unity Approach | Impact Assessment for RTRLab |
|-------------------|-------------------------|------------------------------|
| Async loading / streaming | Unreal `FStreamableManager`, Unity `Addressables.LoadAssetAsync` | Current synchronous I/O will block the main thread on large assets |
| Reference counting / lifecycle | Unreal `FSoftObjectPtr` + GC, Unity `Resources.UnloadUnusedAssets` | No asset unloading concept currently |
| Memory budgeting and priority | Unreal Streaming Pool, Unity Memory Profiler | All assets are fully loaded |
| GPU-compressed texture cook (BC/ASTC/ETC) | Unreal Texture Compressor, Unity platform texture import | Only RGBA8 uncompressed currently |
| Shader cook / offline compilation | Unreal ShaderCompileWorker, Unity ShaderCompiler | Shaders are currently copied verbatim |
| Asset hot-reload | Unreal Editor live reimport, Unity auto-reimport | Requires manual re-index + re-cook + restart |
| Asset browser / editor integration | Unreal Content Browser, Unity Project Window | CLI tools only |

### 12. Overall Positioning

RTRLab's resource system makes architecturally sound choices:

- **Logical path decoupled from physical storage** -- aligned with Unreal / id Tech's
  core philosophy.
- **Document vs asset classification** -- cleaner than Unreal's "everything is a UAsset"
  approach; config files do not need to go through the cook pipeline.
- **Explicit priority mount hierarchy** -- more predictable than id Tech's implicit
  search-path ordering.
- **Toolchain separated from runtime** -- cook/pack are standalone executables rather
  than editor-embedded operations, making them CI/CD-friendly.

The system is currently at the **"architectural skeleton in place, awaiting flesh"**
stage. As a rough positioning:

```
Godot 3.x early  --  RTRLab current  ----------  Unreal 5.x
   |                    |                            |
   Simple direct        Logical paths + multi-layer  Complete asset registry
     mapping              mount + pak + overlay        + DDC + async streaming
   No cook pipeline     Bootstrap cook                 + compression + encryption
   Single backend       Source/Cooked/Packaged         + chunking + hot-reload
```

### 13. Recommended Priorities

The following improvements offer the highest return on investment. None of them require
changes to the logical path model or mount architecture; they extend capabilities on the
existing skeleton.

1. **Incremental cook** (wire up `contentHash`) -- the single largest developer-
   experience bottleneck. Full-rebuild cook time grows linearly with asset count.

2. **Pak index caching** -- a straightforward change that directly improves packaged-
   profile runtime performance. Read the index once, keep it resident.

3. **GPU-compressed texture formats** (BC7/ASTC) -- current RGBA8 uncompressed textures
   waste significant GPU bandwidth and VRAM.

4. **Async loading interface** -- prepares the path for future streaming and prevents
   main-thread stalls on large assets.
