# Resource System Internals

This document describes how the resource system actually works at the code level, and
compares RTRLab's approach with the resource/asset systems found in modern game engines.

It is intended as a companion to the main
[ResourceSystem.md](ResourceSystem.md) design document, which focuses on the public
contract and design rationale. This document focuses on implementation mechanics and
industry context.

> **Snapshot date**: 2026-04-11

---

## Table of Contents

- [Resource System Internals](#resource-system-internals)
  - [Table of Contents](#table-of-contents)
  - [Part I: How the System Works](#part-i-how-the-system-works)
    - [1. End-to-End Flow Overview](#1-end-to-end-flow-overview)
    - [2. Offline Toolchain](#2-offline-toolchain)
      - [2.1 Source Catalog Construction](#21-source-catalog-construction)
      - [2.2 Cooking (rtr\_asset\_cook)](#22-cooking-rtr_asset_cook)
      - [2.3 Packaging (rtr\_asset\_pack)](#23-packaging-rtr_asset_pack)
    - [3. Runtime Resolution](#3-runtime-resolution)
      - [3.1 Initialization](#31-initialization)
      - [3.2 Path Parsing and Domain Dispatch](#32-path-parsing-and-domain-dispatch)
      - [3.3 Unified Project--Engine Read Resolution](#33-unified-project--engine-read-resolution)
      - [3.4 Writable Domain Resolution](#34-writable-domain-resolution)
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

The system operates in two phases: an **offline toolchain** (source scan, cook, pack,
stage) and a
**runtime resolution** layer (FileSystem API).

```
Offline phase (build time)                Runtime phase
+---------------+                        +----------------------+
| rtr_asset_    |  decode images->.rtrtex | Which domain?        |
|   cook        |-> generate cooked cat   | Project/Engine       |
+---------------+                        +----------+-----------+
       |                                            |
       v                                            v
+---------------+                +----------------------+  +----------------------+
| rtr_asset_    |  bundle into   | CatalogRegistry      |  | Writable mounts      |
|   pack        |  .rtrpak       | ::ResolvePath        |  | Saved / Cache roots  |
+---------------+                +----------------------+  +----------------------+
```

---

### 2. Offline Toolchain

#### 2.1 Source Catalog Construction

- Implementation: `src/Core/Resource/Catalog/SourceCatalog.cpp`

**What it does**: recursively scans source mount roots and builds source catalog
entries in memory.

**Steps** (`SourceCatalog.cpp:304-319`):

1. **Discover mounts**: scan for `Project/` and `Engine/` and collect each existing
   directory as a source mount.

2. **Scan files**: for each mount root, run a `recursive_directory_iterator`. Skip the
   `.rtr/` metadata directory.

3. **Classify content role and derive logical paths**:
   - files under `Config/` become **document entries**
   - plain-text support files such as `.ini`, `.txt`, `.xml`, and `.toml` may also
     become document entries
   - asset-oriented directories such as `Textures/`, `Shaders/`, `Materials/`,
     `Scenes/`, and `Defaults/` remain **asset entries** even when the source file is
     text-based such as `.json`
   - document entries keep the logical filename and extension:
     `Project/Config/Graphics.json` -> `/Project/Config/Graphics.json`
   - asset entries strip the source extension:
     `Project/Textures/Grassy_Square.jpg` -> `/Project/Textures/Grassy_Square`

4. **Enforce uniqueness**: if two source files generate the same logical path (e.g.
   `foo.jpg` and `foo.png` in the same directory), source catalog construction rejects
   the catalog with an error.

5. **Return entries in memory**: each entry contains `logicalPath`,
   `sourceRelativePath`, and an `artifacts` array (default tags:
   `profileTag="dev"`, `backendTag="any"`, `platformTag="any"`).

**Example entry shape**:

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
   (e.g. `build/Packaging/<Config>/Cooked/Project/`).

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
24      4     pixelFormat (1 = RGBA8_UNORM)
28      4     mipLevelCount (1)
32      4     rowPitch (width * 4)
36      4     dataOffset (44)
40      4     dataSize
44      ...   raw RGBA8 pixel data
```

#### 2.3 Packaging (`rtr_asset_pack`)

- Entry: `src/Tools/AssetPackMain.cpp` -> `Resource::PackageCookedRepositoryCatalogs()`
- Implementation: `src/Core/Resource/Package/PakArchive.cpp`

**What it does**: merges cooked `Project/` and `Engine/` content into a single
`Game.rtrpak`, including one merged cooked catalog at `.rtr/catalog.json`.

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

**Output**: `build/Stage/<Config>/Game.rtrpak` (with cooked intermediates staged under
`build/Packaging/<Config>/Cooked/`).

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
  - CLI `--root`, if present
  - `RTRL_ROOT` environment variable, but only if that directory contains a
    `.rtrproject` marker
  - walk up from the executable location (max 5 levels) looking for `.rtrproject`
  - `GLAB_ROOT_DIR` compile-time fallback, again requiring `.rtrproject`
  - current working directory, if it contains `.rtrproject`
  - otherwise log loudly and fall back to `current_path()`
- **Release/shipping builds** (`RTRLAB_CONFIG_RELEASE`):
  - treat the executable directory as the install root by default
  - ignore `RTRL_ROOT`
  - honor `--root` only when `--dev-mode` is present, still requiring a
    `.rtrproject` marker at that override path

Writable directories (`s_SavedDir`, `s_CacheDir`) are **lazily initialized** on first
access via `GetSavedDir()` / `GetCacheDir()`. In development mode (`GLAB_ROOT_DIR`
defined), they resolve to `rootPath/Saved` and `rootPath/Saved/Cache`. Otherwise they
use platform-specific user data paths:
- Windows: `%LOCALAPPDATA%/RTRLab/Saved` and `%LOCALAPPDATA%/RTRLab/Cache`
- macOS: `~/Library/Application Support/RTRLab/Saved` and `~/Library/Caches/RTRLab`
- Linux: `$XDG_DATA_HOME/RTRLab/Saved` and `$XDG_CACHE_HOME/RTRLab`

#### 3.2 Path Parsing and Domain Dispatch

When `ReadBinary("/Project/Textures/Grassy_Square")` is called:

**Step 1: Parse the virtual path**
- Validate: must start with `/`, must not contain `\`, must not contain `.` or `..`
  segments.
- Split into segments, collapse consecutive `/`.
- Determine domain from the first segment: `Project` / `Engine` / `DLC` / `Mod` /
  `Saved` / `Cache`.

Result: `VirtualPath { domain=Project, mountName=nullopt, relativePath="Textures/Grassy_Square" }`

**Step 2: Dispatch by domain**
- `Project` / `Engine` / `DLC` / `Mod`: always resolve through
  `CatalogRegistry::ResolveArtifact()`
- `Saved` / `Cache`: resolve through writable mounts directly

The runtime no longer branches on "has extension" versus "no extension". Document vs
asset classification now exists only inside the source catalog builder.

#### 3.3 Unified Project / Engine Read Resolution

`FileSystem.cpp` routes every Project / Engine / DLC / Mod read through
`CatalogRegistry::ResolveArtifact()`.

This includes both kinds of logical path:

```
/Project/Textures/Grassy_Square  -> catalog entry -> selected artifact
/Project/Config/Graphics.json    -> document catalog entry -> selected artifact
/Engine/Defaults/Materials/ErrorMaterial -> asset catalog entry -> selected artifact
/DLC/Expansion1/Weapons/LaserRifle -> namespaced DLC entry -> selected artifact
/Mod/CoolMod/Weapons/Hammer -> namespaced mod entry -> selected artifact
```

The resolution path is the same for both asset and document entries. The difference is
how the source catalog entry was synthesized:

- asset entries are extensionless logical paths and may later grow multiple artifacts
- document entries keep their explicit filename/extension and currently carry a single
  artifact with `format="document"`

Once the entry is in the merged catalog table, runtime resolution does not care which
builder-time rule produced it.

**Step A: Lazily build the global table** (first call only)

1. Call `DiscoverReadableMountBackends()` to discover all readable mounts. This function
   now has only two runtime behaviors:

   **Profile decision logic**:
   - `"dev"` profile (default): register `Project/` and `Engine/` source mounts only.
   - `"shipping"` profile: register pak-backed mounts only.

   For each catalog-backed domain, register mounts according to the profile strategy,
   assigning priorities:

   ```
   Mod      (500)  >  Patch    (400)  >  DLC      (300)
   > Packaged (200) > Source   (0)
   ```

   In shipping installs, the runtime scans:

   - `<install>/Game.rtrpak` for the base packaged Project / Engine content
   - `<install>/DLC/*.rtrpak` for DLC overlays and `/DLC/<Name>/...` namespace mounts
   - `<install>/Patches/*.rtrpak` for transparent patch overlays
   - `<install>/Mods/*.rtrpak` for mod overlays and `/Mod/<Name>/...` namespace mounts

2. For each discovered mount, acquire its catalog:
   - Dev source directory backend (`Project/` or `Engine/`, source priority): build the
     source catalog in memory from the source tree.
   - PakArchive backend: extract `.rtr/catalog.json` from inside the `.rtrpak` file.

3. Parse the catalog JSON, validate version (v1 = source, v2 = cooked), and filter the
   merged entry set by mount domain. In shipping, both the Project and Engine
   mounts may read from the same shared `Game.rtrpak` catalog, but each mount only
   keeps entries for its own logical domain. DLC and Mod namespace mounts perform the
   same filtering, but also require the catalog path's `mountName` to match the
   discovered pak stem. Both extensionless asset paths and extension-preserving
   document paths are valid for Project / Engine / DLC / Mod mounts.

4. **Merge into global table** (`ResourceCatalog.cpp:334-388`):
   - A higher-priority mount's entry replaces a lower-priority one
     (mod > patch > DLC > packaged > source).
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
- `profileTag`: development builds use the default `dev` profile. Release/shipping
  builds default to `shipping`, which maps artifact selection to `"cooked"`. The
  only remaining dev-mode override is `--dev-mode --root <path>`, which changes root
  discovery but does not reintroduce extra runtime profiles.

**Step D: Artifact descriptor resolution** (`MountBackend.cpp`)

- Directory backend: return a descriptor with `backend=Directory`,
  `mountRoot`, and `relativePath`.
- PakArchive backend: return a descriptor with `backend=PakArchive`,
  `mountRoot`, and `relativePath`.
- `FileSystem::ReadText`, `ReadBinary`, and `OpenReadStream` consume that
  descriptor directly. Directory-backed reads use normal file I/O; pak-backed
  reads use `ReadPakEntry()` or an in-memory stream wrapper.

#### 3.4 Writable Domain Resolution

`/Saved/...` and `/Cache/...` reads do not go through the catalog. They resolve by
joining the parsed relative path onto the selected writable root:

```
/Saved/Config/imgui.ini  ->  {savedDir}/Config/imgui.ini
/Cache/Shaders/foo.bin   ->  {cacheDir}/Shaders/foo.bin
```

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
  +- Domain dispatch -> Project => catalog path
  |
  +- CatalogRegistry::ResolveArtifact
       |
       +- [first call] DiscoverReadableMountBackends
       |    +- dev profile:
       |    |    +- Project/ exists      -> register Source:Project    (priority=0)
       |    |    +- Engine/ exists       -> register Source:Engine     (priority=0)
       |    +- shipping profile:
       |         +- Game.rtrpak          -> register Packaged:Project  (priority=200)
       |         +- Game.rtrpak          -> register Packaged:Engine   (priority=200)
       |         +- DLC/Expansion1.rtrpak -> register DLC:Expansion1:Project (priority=300)
       |         +- DLC/Expansion1.rtrpak -> register DLC:Expansion1         (priority=300)
       |         +- Patches/Patch_001.rtrpak -> register Patch:Patch_001:Project (priority=400)
       |         +- Mods/CoolMod.rtrpak  -> register Mod:CoolMod:Project    (priority=500)
       |         +- Mods/CoolMod.rtrpak  -> register Mod:CoolMod            (priority=500)
       |
       +- [first call] Load catalog for each mount -> parse JSON -> merge into global table
       |
       +- Lookup: m_GlobalEntries["/Project/Textures/Grassy_Square"]
       |    -> found entry from the highest-priority mount
       |
       +- ChooseArtifact -> select best artifact
       |    e.g. dev -> {relativePath: "Textures/Grassy_Square.jpg", profileTag: "dev"}
       |         shipping -> {relativePath: "Textures/Grassy_Square.rtrtex", profileTag: "cooked"}
       |
       +- ResolveReadableMountArtifact
            +- Directory backend -> {mountRoot, relativePath}
            +- PakArchive backend -> {pak archive path, relativePath}

  ReadBinary consumes the descriptor:
    +- Directory backend -> Resource::ReadBinaryFile(mountRoot / relativePath)
    +- PakArchive backend -> Resource::ReadPakEntry(pakPath, relativePath)
  -> returns bytes.
```

---

### 5. Key Implementation Details

| Mechanism | Implementation Location | Key Point |
|-----------|------------------------|-----------|
| Project / Engine / DLC / Mod reads are always catalog-backed | `FileSystem::ReadText` / `ReadBinary` / `OpenReadStream` | Runtime dispatch is by domain, not by filename extension |
| Document vs asset classification is builder-time | `SourceCatalog.cpp` | Source catalog synthesis decides whether a logical path keeps its extension |
| Shipping overlay precedence is explicit | `MountPriority::{DLC,Patch,Mod}` | Mods override patches, patches override DLC, DLC overrides packaged base content |
| Equal-priority conflict = removal | `MergeMountEntriesIntoGlobalTable` | No implicit choice; the path becomes unavailable |
| Catalog is lazily built | `m_GlobalTableBuilt` flag | All mounts are scanned on the first Project / Engine read |
| Pak entries are read directly | `ReadPakEntry` / `OpenReadableArtifactStream` | No extraction cache or temp-file materialization remains in the normal read path |
| Profile determines mount registration order | `DiscoverReadableMountBackends` | `dev` registers source only; `shipping` registers pak mounts only |
| Shipping CLI surface is explicit | `Core/Util/CommandLine` + `main.cpp` | Release help shows `--help`, `--language`, `--windowed`, `--fullscreen`, and `--dev-mode`; dev-only resource overrides are dropped unless `--dev-mode` is present |
| Writable dirs are lazily resolved | `FileSystem::ResolveWritableDirs` | First call to `GetSavedDir()` / `GetCacheDir()` triggers resolution |
| Dev root discovery walks up from executable | `FindRootFromExecutable` | Up to 5 parent directories, looking for `.rtrproject` |
| Shipping root discovery uses install dir | `DiscoverRootPath` under `RTRLAB_CONFIG_RELEASE` | Executable directory is treated as the root unless `--dev-mode --root <path>` is explicitly provided |

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
| Import trigger | Manual cook/pack staging step | Automatic on editor launch / file change | Automatic on file drop into Assets/ |
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
| Override mechanism | 5-level priority (Mod > Patch > DLC > Packaged > Source) | Pak priority + patch pak | AssetBundle variants | Later-mounted pak overrides earlier |
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
| Streaming | Direct stream from resolved artifact | Direct stream from pak | Direct stream | Direct stream |

**Analysis**:

RTRLab's pak format is a minimum viable archive. Key gaps relative to Unreal's `.pak`:

**a) No compression**. Unreal's pak supports per-entry or per-block compression; Unity
does too. RTRLab's RGBA8 textures are uncompressed raw data packed directly, leading to
large storage sizes. This is acceptable with only a few textures but grows linearly.

**b) Index not resident in memory**. Unreal reads the pak index into memory at startup
and keeps it resident; subsequent reads require only a single seek+read. RTRLab reopens
the file and reads the full header+index on every `FindPakEntry` call.

**c) Data-returning reads vs streaming**. RTRLab now resolves readable artifacts into a
small descriptor and lets the `FileSystem` facade serve bytes or streams directly from
directory-backed files or pak-backed entries. Unreal and Unity also support direct
streaming from archives; RTRLab now follows the same broad direction, though its higher-
level runtime consumers are still largely synchronous.

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
