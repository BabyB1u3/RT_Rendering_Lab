# Resource System Refactor Plan

Plan for refactoring the resource system based on the design review discussion on
2026-04-10. This document records the decisions reached during that discussion and
lays out the phased implementation work required to land them.

> **Goal**: Reshape the resource system around a single catalog-driven read path,
> a Godot-style flat shipping layout, and a unified mount-priority mechanism for
> DLC / Patch / Mod overlays. The end state is simpler, has fewer special cases,
> and makes dev-mode behavior representative of shipping-mode behavior.

---

## Table of Contents

- [Resource System Refactor Plan](#resource-system-refactor-plan)
  - [Table of Contents](#table-of-contents)
  - [1. Motivation](#1-motivation)
  - [2. Decisions](#2-decisions)
    - [2.1 Root Discovery](#21-root-discovery)
    - [2.2 Architecture Positioning](#22-architecture-positioning)
    - [2.3 Unified Catalog Read Path](#23-unified-catalog-read-path)
    - [2.4 Naming and Mount Types](#24-naming-and-mount-types)
    - [2.5 DLC / Patch / Mod as First-Class Mounts](#25-dlc--patch--mod-as-first-class-mounts)
    - [2.6 Directory Layout](#26-directory-layout)
    - [2.7 API Contract: Data, Not Paths](#27-api-contract-data-not-paths)
    - [2.8 Env Vars and CLI Overrides](#28-env-vars-and-cli-overrides)
  - [3. Before / After](#3-before--after)
    - [3.1 Dev Source Layout](#31-dev-source-layout)
    - [3.2 Shipping Install Layout](#32-shipping-install-layout)
    - [3.3 Writable User Data Layout](#33-writable-user-data-layout)
    - [3.4 Virtual Path Namespace](#34-virtual-path-namespace)
  - [4. Implementation Phases](#4-implementation-phases)
    - [Phase 1: Root Discovery and Marker File](#phase-1-root-discovery-and-marker-file)
    - [Phase 2: Naming Migration](#phase-2-naming-migration)
    - [Phase 3: In-Memory Source Catalog](#phase-3-in-memory-source-catalog)
    - [Phase 4: Unified Read Path](#phase-4-unified-read-path)
    - [Phase 5: Data-Returning API](#phase-5-data-returning-api)
    - [Phase 6: Flat Shipping Layout](#phase-6-flat-shipping-layout)
    - [Phase 7: DLC / Patch / Mod Mounts](#phase-7-dlc--patch--mod-mounts)
    - [Phase 8: Core/Util Reorganization, CLI Parser, and Override Hardening](#phase-8-coreutil-reorganization-cli-parser-and-override-hardening)
  - [5. Out of Scope](#5-out-of-scope)
  - [6. Resolved Questions](#6-resolved-questions)
    - [Q1. `.rtrproject` contents — what goes inside the marker file?](#q1-rtrproject-contents--what-goes-inside-the-marker-file)
    - [Q2. Single merged `Game.rtrpak` vs split Engine/Project paks](#q2-single-merged-gamertrpak-vs-split-engineproject-paks)
    - [Q3. Config file migration through the unified catalog](#q3-config-file-migration-through-the-unified-catalog)
    - [Q4. `ExtractToTempPath` lifetime](#q4-extracttotemppath-lifetime)
    - [Q5. CLI parser choice and Core/ directory cleanup](#q5-cli-parser-choice-and-core-directory-cleanup)

---

## 1. Motivation

The current resource system has several structural issues identified during review:

1. **Root discovery is coupled to source asset layout** — it searches for a `Content/`
   directory to locate the project root, which forces shipping builds to ship an empty
   `Content/` directory as a hack.
2. **Dev and shipping walk different code paths** — dev mode uses
   `Resource::ResolvePhysicalPath` (direct virtual-to-physical mapping), while shipping
   mode uses the catalog system. Bugs in catalog parsing, artifact selection, or pak
   reading are invisible in dev.
3. **`Plugin` as a resource mount type is redundant** with the DLC / Patch / Mod model —
   it predates that model and can be removed.
4. **Shipping directory structure is awkward** — pak files live under `build/Packaged/`
   even in a released build, which implies "build artifact" semantics that do not belong
   in an install tree.
5. **Pak materialization leaks abstraction** — the resource API returns
   `std::filesystem::path`, which forces pak entries to be extracted to a
   `PackagedExtracted/` cache directory before they can be consumed. Returning data
   directly removes the need for the extraction cache entirely.

The decisions below address each of these points.

---

## 2. Decisions

### 2.1 Root Discovery

- **Dev mode**: search upward from the executable for a marker file named
  `.rtrproject`. Depth limit retained (currently 5). The marker is an **empty file**
  (or a single comment line); it carries no content today. If a future tool genuinely
  needs project metadata, the file can evolve into JSON in a backward-compatible way,
  but there is no justification to add content preemptively.
- **Shipping mode**: the executable's directory *is* the root. No search performed.
  No marker file is required in the install tree. Selected via a compile-time macro
  (`RTRL_SHIPPING` or equivalent) flipping the discovery strategy.
- **Fallbacks**: if dev-mode search fails, fall back to the current working directory
  with a loud warning. Never silently succeed against an unrelated directory.
- **`Content/`-based discovery is removed entirely.**

### 2.2 Architecture Positioning

RTRLab is a **tool-type engine** (Godot-style), not a framework-type engine
(Unreal-style). Engine code is statically linked into the game executable; there is
no scenario where the engine is deployed independently of a specific game. This
justifies:

- Flat shipping layout — one `Game.rtrpak` merging Engine and Project content.
- No separate "engine install" versus "game install" concept.
- The Engine/Project split exists only at *source* level, as organizational scaffolding
  for developers.

### 2.3 Unified Catalog Read Path

**Dev and shipping both go through the catalog system** for everything in the
Project and Engine domains. `Saved/` and `Cache/` remain writable direct-mount
domains — they never go through the catalog because they are per-user mutable
state, not content.

- Introduce a `SourceCatalogBuilder` that walks a source tree and produces a
  `ResourceCatalogEntry` per file. The builder applies a classification rule to
  decide what kind of entry to emit:
  - **Asset files** (textures, meshes, shaders, etc.) → logical path has the
    extension stripped; the entry can hold multiple artifacts with different
    `profileTag` / `backendTag` / `platformTag` values for artifact selection to
    choose between source and cooked variants.
  - **Document files** (config, text, JSON) → logical path keeps the extension;
    the entry holds a single artifact with `format="document"`. Artifact selection
    is trivial (single artifact always wins).
  The classification rule is a **builder-time convention** (e.g. files under
  `Config/` or with extensions in `{.json, .ini, .txt, .xml, .toml}` are
  documents), not a runtime branch.
- On `FileSystem::Init`, dev builds catalogs **in memory** for Engine and Project
  source trees and installs them into `CatalogRegistry` alongside any on-disk
  cooked / packaged catalogs that exist.
- `FileSystem::ResolveReadPath` always goes through `CatalogRegistry::ResolvePath`
  for Project / Engine domains, and through `ResolveWritableMount` for Saved /
  Cache domains. No document-vs-catalog bifurcation inside either branch.
- `Resource::ResolvePhysicalPath`, `IsCatalogBackedPath`, and `IsDocumentPath`
  functions are removed. The "two code paths for reads in the Project/Engine
  domain" bifurcation is gone. The *fact* that documents and assets are
  classified differently survives, but only as a rule inside the source catalog
  builder — runtime resolution sees a uniform `ResourceCatalogEntry`.
- `RefreshCatalogs()` stays as the manual trigger to re-scan source trees after
  files change on disk. Optional file watcher integration is deferred.

Result: catalog parsing, artifact selection, mount merging, and priority resolution
are exercised on every dev-mode read. Bugs surface during development, not during
the first packaged build.

**Layered config model (downstream consumer)**: default config files live at
`/Project/Config/*.json` or `/Engine/Config/*.json` and are shipped in the pak
(via the source → cooked → packaged pipeline). User overrides live at
`/Saved/Config/*.json`, which bypasses the catalog and maps directly to the
platform user directory. The config subsystem merges layers in this order:
hardcoded defaults → Layer 2 (pak defaults via catalog) → Layer 3 (user overrides
via writable mount) → Layer 4 (command-line overrides). Writes only ever target
Layer 3. This layering is a concern of the config subsystem, not the resource
system — but it is noted here because it is why Saved/Cache stays a separate
domain and why document files remain human-editable plain text rather than being
serialized into an opaque format.

### 2.4 Naming and Mount Types

| Old | New | Notes |
|-----|-----|-------|
| `EngineContent/` | `Engine/` | Drop redundant `Content` suffix |
| `Content/` | `Project/` | Symmetric with `Engine/`, clearer semantics |
| `PathDomain::Plugin` | **removed** | Replaced by Mod/DLC overlay mounts |
| `Plugins/<Name>/Content/` | **removed** | Ditto |

Virtual path prefixes become:

- `/Engine/...` — engine-authored content
- `/Project/...` — game-authored content
- `/DLC/<Name>/...` — first-party DLC
- `/Mod/<Name>/...` — user-installed mods
- `/Saved/...` — writable user data (config, saves, screenshots)
- `/Cache/...` — writable derived data (shader cache, etc.)

`Patch` does not get its own virtual path prefix — patches override existing logical
paths from the base game and are invisible to callers (they see the patched content
under the original `/Engine/` or `/Project/` path).

### 2.5 DLC / Patch / Mod as First-Class Mounts

DLC, Patch, and Mod all use the same underlying mechanism: **additional pak mounts
with higher priority than the base game**. The existing `MountPriority` enum extends
to cover them:

```
Source    = 0     (dev-only: in-memory source catalog)
Cooked    = 100   (dev-only: cook output)
Packaged  = 200   (base game pak)
DLC       = 300   (first-party DLC paks)
Patch     = 400   (patch paks — override base and DLC)
Mod       = 500   (user mods — override everything)
```

Mount discovery scans additional directories in shipping mode:

- `<install>/DLC/*.rtrpak` — each file becomes a DLC mount
- `<install>/Patches/*.rtrpak` — each file becomes a Patch mount
- `<install>/Mods/*.rtrpak` — each file becomes a Mod mount

When multiple mounts provide the same logical path, the highest-priority mount wins
(`CatalogRegistry::MergeMountEntriesIntoGlobalTable` already implements this).

### 2.6 Directory Layout

See [section 3](#3-before--after) for the before/after diagrams. Key points:

- **`build/` exists only in the dev tree**, as a CMake-style build artifact directory.
  It contains `build/Cooked/` and `build/Packaged/`, which are the intermediate outputs
  of the cook and package steps.
- **Shipping install is flat**. The package step copies `build/Packaged/Game.rtrpak`
  (and any DLC paks) next to the exe. The `build/` directory is never part of a
  shipping distribution.
- **Writable data always lives in a platform user directory**, not under the install
  directory. Shipping builds never assume the install directory is writable.

### 2.7 API Contract: Data, Not Paths

The `FileSystem` facade returns **data**, not file paths, as the primary API:

- `ReadText(virtualPath) -> optional<string>`
- `ReadBinary(virtualPath) -> optional<vector<uint8_t>>`
- `OpenReadStream(virtualPath) -> unique_ptr<istream>` (new, for large files where
  loading the whole thing into memory is wasteful — e.g. streaming mesh / audio)
- `WriteText` / `WriteBinary` unchanged — they still return a bool and only apply to
  writable domains.

`ResolveReadPath` is removed from the public surface. It may survive as a private
diagnostic helper used by logging, but no non-diagnostic call site in the codebase is
allowed to hold a `filesystem::path` obtained from a virtual path.

**No path-returning escape hatch is provided.** Earlier drafts of this plan included
an `ExtractToTempPath` helper for third-party libraries that only accept file paths,
but on review this is unnecessary: modern C++ asset libraries (`stb_image`,
`tinygltf`, `assimp`, `miniz`, etc.) almost universally accept memory buffers or
streams. If a specific case arises that genuinely requires a path, a small helper
can be added at that time as isolated code — there is no reason to design for it now
(YAGNI).

**Removed**:
- `Resource::MaterializePakEntry` — replaced by direct in-memory `ReadPakEntry`
- `PackagedExtracted/` cache directory — no longer exists
- The `materializedRoot` field on `ReadableMount` — no longer needed

### 2.8 Env Vars and CLI Overrides

- **Dev mode**: env vars (`RTRL_ROOT`, `RTRLAB_COOKED_ROOT`, `RTRLAB_PACKAGE_ROOT`,
  `RTRLAB_OVERLAY_ROOT`, `RTRLAB_RESOURCE_PROFILE`) and CLI args remain supported.
- **Shipping mode**: path-related overrides (`RTRL_ROOT`, cooked/package/overlay roots)
  are **compiled out** or return `nullptr` unconditionally. This prevents an attacker
  from redirecting resource loads via a malicious shortcut.
- **Shipping whitelist**: only a small set of player-facing overrides survives
  (`--language`, `--windowed`, etc.). `--content-root` and friends require
  `--dev-mode` (or are absent entirely).
- **Precedence**: CLI > env var > marker file search > compile-time default.

---

## 3. Before / After

### 3.1 Dev Source Layout

**Before**:

```
<repo root>/
├── Content/                          # project source assets
│   └── .rtr/catalog.json
├── EngineContent/                    # engine source assets
│   └── .rtr/catalog.json
├── Plugins/
│   └── <PluginName>/
│       └── Content/
│           └── .rtr/catalog.json
├── Saved/
│   └── Overrides/                    # dev-time override mounts
└── build/
    ├── Cooked/
    └── Packaged/
```

**After**:

```
<repo root>/
├── .rtrproject                       # root marker
├── Engine/                           # engine source assets
├── Project/                          # project source assets
└── build/
    ├── Cooked/
    │   ├── Engine/
    │   └── Project/
    └── Packaged/
        └── Game.rtrpak               # merged Engine + Project pak
```

- `.rtr/catalog.json` files under source directories are **gone**. Dev builds catalogs
  in memory from the source tree at startup.
- `Plugins/` directory is gone. The DLC / Mod concept replaces it.
- `Saved/Overrides/` is gone. The dev workflow for testing overrides is to drop a
  `.rtrpak` into `build/Packaged/` with a Mod-priority manifest, or to use a
  dedicated dev-only mount configured via env var.

### 3.2 Shipping Install Layout

**Before** (awkward — `build/` and empty `Content/`):

```
<install>/
├── RTRLab.exe
├── Content/                          # empty, just to satisfy root discovery
└── build/
    └── Packaged/
        ├── Project.rtrpak
        ├── Engine.rtrpak
        └── Plugins/
            └── MyPlugin.rtrpak
```

**After** (flat, Godot-style):

```
<install>/
├── RTRLab.exe
├── .rtrproject                       # optional marker, may be omitted in shipping
├── Game.rtrpak                       # merged Engine + Project content
├── DLC/                              # optional
│   ├── Expansion1.rtrpak
│   └── Expansion2.rtrpak
├── Patches/                          # optional, populated by auto-updater
│   └── Patch_001.rtrpak
└── Mods/                             # optional, user-installed
    └── CoolMod.rtrpak
```

- No `build/` directory.
- No empty `Content/` marker directory.
- Root discovery in shipping mode simply uses the exe directory; the `.rtrproject`
  file is optional in shipping (harmless if present, unnecessary if absent).

### 3.3 Writable User Data Layout

Unchanged in spirit, but the `PackagedExtracted/` cache directory goes away:

```
<platform user dir>/RTRLab/
├── Saved/
│   └── Config/                       # user config, saves, screenshots
└── Cache/
    └── Shaders/                      # shader cache, etc.
    # (PackagedExtracted/ removed)
```

Platform-specific location:

- **Windows**: `%LOCALAPPDATA%/RTRLab/`
- **macOS**: `~/Library/Application Support/RTRLab/` (Saved) and
  `~/Library/Caches/RTRLab/` (Cache)
- **Linux**: `$XDG_DATA_HOME/RTRLab/` and `$XDG_CACHE_HOME/RTRLab/`

### 3.4 Virtual Path Namespace

| Prefix | Read | Write | Source in Dev | Source in Shipping |
|--------|------|-------|---------------|--------------------|
| `/Engine/` | yes | no | `Engine/` source dir | `Game.rtrpak` |
| `/Project/` | yes | no | `Project/` source dir | `Game.rtrpak` |
| `/DLC/<Name>/` | yes | no | (n/a in dev) | `DLC/<Name>.rtrpak` |
| `/Mod/<Name>/` | yes | no | (n/a in dev) | `Mods/<Name>.rtrpak` |
| `/Saved/` | yes | yes | `<user>/Saved/` | `<user>/Saved/` |
| `/Cache/` | yes | yes | `<user>/Cache/` | `<user>/Cache/` |

---

## 4. Implementation Phases

Phases are ordered so that each step leaves the codebase in a working, testable state.
A phase should be landable as its own PR.

### Phase 1: Root Discovery and Marker File

**Scope**: replace `Content/`-based discovery with `.rtrproject` marker.

- Add `.rtrproject` file at repo root (empty or minimal JSON with project name).
- Update `RootDiscovery::DiscoverRootPath` to search for `.rtrproject` instead of
  `assetDirName`.
- Remove the `assetDirName` parameter plumbing from `FileSystem::Init` — it is no
  longer needed.
- Add a `RTRL_SHIPPING` compile-time switch. In shipping mode, `DiscoverRootPath`
  returns `executable_dir()` unconditionally.
- Update tests that construct temp project trees to drop a `.rtrproject` marker.

**Not yet**: naming changes, layout changes, catalog unification.

### Phase 2: Naming Migration

**Scope**: rename `EngineContent/` → `Engine/`, `Content/` → `Project/`, remove
`Plugin` concept.

- Rename directories on disk: `EngineContent/` → `Engine/`, `Content/` → `Project/`.
  Update any references in build scripts, CMake, docs.
- Update `FileSystem::Init` and `MountBackend` to use new directory names.
- Remove `PathDomain::Plugin` from `PathTypes.h` and all call sites.
- Remove `Plugins/` directory scanning in `MountBackend::DiscoverReadableMountBackends`.
- Remove `IsValidPluginMountName` and related helpers.
- Update `PathParser` to reject `/Plugins/...` paths (or delete that code path).
- Update all `.rtr/catalog.json` files that reference the old prefixes.
- Update `Docs/Modules/ResourceSystem/Design.md` and `Internals.md`.

**Not yet**: catalog unification, DLC/Mod directories (this phase only deletes the
old Plugin concept; Mod replaces it in a later phase).

### Phase 3: In-Memory Source Catalog

**Scope**: build source catalogs in memory at startup from source trees.

- Add `SourceCatalogBuilder` in `Src/Core/Resource/Catalog/`. It takes a source root
  path and a mount prefix (e.g. `/Engine/`, `/Project/`), walks the tree, and emits a
  `unordered_map<string, ResourceCatalogEntry>` with one entry per file.
- Each synthesized entry has one `ArtifactRecord` pointing at the source file,
  `format="source"`, tags all `"any"`.
- Modify `CatalogRegistry::ResolvePath` so that on first use in dev mode, it calls
  `SourceCatalogBuilder` for each source mount and inserts the results into the same
  internal map used by disk-loaded catalogs.
- Delete any on-disk `.rtr/catalog.json` files under source trees (they are no longer
  needed — dev builds the catalog in memory).
- Keep on-disk catalogs for cooked/packaged mounts unchanged.

**Not yet**: removing the `Resource::ResolvePhysicalPath` fallback. Both code paths
still exist in this phase; phase 4 removes the old one.

### Phase 4: Unified Read Path

**Scope**: delete the non-catalog resolution path for the Project / Engine
domains. Saved / Cache continue to use the writable mount resolver (they are
never catalog-backed).

- In `FileSystem::ResolveReadPath`, remove the `IsCatalogBackedPath` branch. For
  Project / Engine domains, always route through `CatalogRegistry::ResolvePath`.
  For Saved / Cache domains, continue to route through `ResolveWritableMount`.
- Delete `Resource::ResolvePhysicalPath`, `Resource::IsCatalogBackedPath`, and
  `Resource::IsDocumentPath`. Delete `HasDocumentExtension` from `PathParser.cpp`.
- Update `PathParser` to no longer distinguish document vs catalog paths — parsing
  a virtual path returns a uniform `VirtualPath` that downstream code dispatches on
  domain only.
- In `SourceCatalogBuilder`, implement the document-vs-asset classification rule
  (documented in section 2.3) — this is where the "document" concept survives,
  but only at catalog-build time, not at runtime. The rule decides whether a file
  becomes an extensionless asset entry (multiple artifacts possible) or an
  extension-preserving document entry (single artifact, `format="document"`).
- Config files migrate implicitly: `Project/Config/Graphics.json` gets registered
  as a document entry by the source catalog builder, and
  `ReadText("/Project/Config/Graphics.json")` works unchanged. The only call-site
  migration needed is for code that previously assumed config files took a
  different code path inside `ResolveReadPath` — audit and fix.
- User config overrides at `/Saved/Config/*.json` are unaffected. They were
  already in the Saved domain, which does not go through the catalog. No changes
  needed at the resource-system level; the layered merge logic lives in the
  config subsystem.
- Update tests that relied on document-style paths being a separate code path.
  Tests that only verified read/write semantics for config files should continue
  to pass after the path parser change.

After this phase, **dev and shipping walk identical code paths** for every
Project / Engine read, and the only remaining branch in `FileSystem::ResolveReadPath`
is the domain dispatch between catalog-backed and writable-mount-backed domains,
which is a legitimate and permanent distinction (one is content, the other is
per-user state).

### Phase 5: Data-Returning API

**Scope**: change the primary `FileSystem` API to return data, remove pak materialization.

- Audit all callers of `FileSystem::ResolveReadPath` and `FileSystem::GetRootPath`.
  Categorize each into:
  1. Reads a file — migrate to `ReadBinary` / `ReadText`.
  2. Streams a large file — migrate to `OpenReadStream`.
  3. Genuinely needs a path for debugging/logging — allow via a private diagnostic
     helper that is clearly marked as such.
- Migrate category 1 first — this is the vast majority of call sites.
- Add `OpenReadStream(virtualPath) -> unique_ptr<istream>` for the streaming cases.
  For pak-backed reads, the returned stream wraps an in-memory buffer (no temp
  file). For directory-backed reads, it wraps a plain `ifstream`. Call sites don't
  need to know which.
- Change `CatalogRegistry::ResolvePath`'s return type from `filesystem::path` to a
  small descriptor that can carry either a concrete file path (directory-backed
  mount) or a pak-relative entry (pak-backed mount). The `FileSystem::Read*`
  functions consume this descriptor and dispatch accordingly.
- Delete `Resource::MaterializePakEntry`.
- Delete the `PackagedExtracted/` cache directory logic and the `materializedRoot`
  field on `ReadableMount`.
- **No `ExtractToTempPath` helper is added.** The plan considered one and rejected
  it — see section 2.7. If a real need arises later, it can be added in isolation
  without affecting the rest of the system.

### Phase 6: Flat Shipping Layout

**Scope**: merge Engine + Project into a single `Game.rtrpak`; place it next to the
exe in shipping.

- Update the cook/package step to produce one `Game.rtrpak` that contains both the
  Engine and Project catalogs merged into a single `.rtr/catalog.json`.
- Update the install/stage step (or add one if missing) to copy `Game.rtrpak` next to
  the staged executable, not under `build/Packaged/`.
- Update shipping-mode `MountBackend::DiscoverReadableMountBackends` to look for
  `<install>/Game.rtrpak` instead of `<install>/build/Packaged/*.rtrpak`.
- Delete the `build/Packaged/` search path from shipping mode.
- Remove the empty-`Content/`-directory workaround if any still lingers after phase 1.
- Verify a shipping build runs from a directory containing only the exe, `.rtrproject`
  (optional), and `Game.rtrpak`.

### Phase 7: DLC / Patch / Mod Mounts

**Scope**: add mount discovery for DLC, Patch, and Mod overlay directories.

- Extend `MountPriority` enum with `DLC = 300`, `Patch = 400`, `Mod = 500`.
- Extend `DiscoverReadableMountBackends` to scan `<install>/DLC/`,
  `<install>/Patches/`, `<install>/Mods/` for `.rtrpak` files in shipping mode.
- Each discovered pak becomes a `ReadableMount` with the corresponding priority.
- Verify `CatalogRegistry::MergeMountEntriesIntoGlobalTable` correctly resolves
  priority conflicts. It already does this — mostly a matter of adding tests.
- Add virtual path prefixes `/DLC/<Name>/` and `/Mod/<Name>/` to `PathParser` so that
  DLC and Mod content can be addressed directly in addition to transparently
  overriding base content.
- Add tests:
  1. Base-only: logical path resolves to base pak.
  2. Base + DLC: DLC additive content is visible under `/DLC/<Name>/`.
  3. Base + Patch: logical paths override base pak transparently.
  4. Base + Mod: mod overrides patch, DLC, and base.

### Phase 8: Core/Util Reorganization, CLI Parser, and Override Hardening

**Scope**: tidy up `Src/Core/` by introducing a `Util/` subdirectory for
cross-cutting helpers, add a small in-house CLI parser, and lock down path
overrides in shipping builds. This phase bundles three related small cleanups.

**8a. Core/Util reorganization**:

Currently `Src/Core/` has three loose files at its root (`Base.h`, `Time.h`,
`Time.cpp`) alongside its subdirectories (`App/`, `Diagnostics/`, `Event/`,
`Input/`, `Resource/`, `Serialization/`). This phase creates `Src/Core/Util/` and
moves the loose files into it, so that `Src/Core/` contains only subdirectories.

- Create `Src/Core/Util/`.
- Move `Src/Core/Base.h` → `Src/Core/Util/Base.h`.
- Move `Src/Core/Time.h` and `Src/Core/Time.cpp` → `Src/Core/Util/Time.{h,cpp}`.
- Global include replacement:
  - `#include "Core/Base.h"` → `#include "Core/Util/Base.h"`
  - `#include "Core/Time.h"` → `#include "Core/Util/Time.h"`
- Update `CMakeLists.txt` to pick up files under `Src/Core/Util/`.

**8b. In-house CLI parser**:

No CLI parser currently exists in the codebase. `Src/main.cpp` does not parse
`argc`/`argv` at all, and the three tool entry points (`AssetCookMain`,
`AssetIndexMain`, `AssetPackMain`) each do ad-hoc manual parsing. This phase adds
a small shared parser.

- Add `Src/Core/Util/CommandLine.{h,cpp}`, implementing a minimal parser for
  `--key=value`, `--key value`, and boolean `--flag` forms. No external
  dependency; expected size is ~150-300 LoC.
- Provide a simple `CommandLineOption` registration API so different subsystems
  can declare their flags independently.
- Migrate the three tool entry points under `Src/Tools/*Main.cpp` to use the new
  parser. Delete the ad-hoc parsing code.
- Wire `Src/main.cpp` to parse `argc`/`argv` at startup and expose the parsed
  result to downstream subsystems (e.g. the resource system consults it for
  `--content-root` overrides in dev builds).

**8c. Override hardening**:

- Gate `RTRL_ROOT`, `RTRLAB_COOKED_ROOT`, `RTRLAB_PACKAGE_ROOT`,
  `RTRLAB_OVERLAY_ROOT`, and `RTRLAB_RESOURCE_PROFILE` behind `#ifndef RTRL_SHIPPING`.
  In shipping builds, these environment variables are ignored entirely.
- Define a shipping-mode CLI whitelist inside the new parser: only
  player-facing options (`--language`, `--windowed`, `--fullscreen`, etc.) are
  accepted. Path-related overrides (`--content-root`, `--mod-dir`, etc.) are
  silently dropped in shipping builds.
- Add a `--dev-mode` flag that, when present in a shipping build, re-enables the
  dev whitelist. Useful for internal QA builds that need override capability
  while still being compiled as "shipping" profile.
- Document the whitelist in `Docs/Modules/ResourceSystem/Design.md`.

---

## 5. Out of Scope

The following were discussed but explicitly deferred:

- **Steam Workshop integration** — the Steamworks `ISteamUGC` path. Requires the
  Steamworks SDK integration, which is a separate workstream. The `Mod` mount
  infrastructure built in phase 7 is the foundation that Workshop will later plug into.
- **Mod code loading** — DLL / script loading for mods that ship executable code.
  Resource system only handles asset mounts; code loading is a separate subsystem.
- **File watcher for dev hot reload** — the in-memory source catalog supports manual
  `RefreshCatalogs()` but not automatic invalidation on file change. Can be added
  later without changing the catalog model.
- **Catalog format v3** — if source and cooked catalogs diverge further in the
  future, a unified v3 schema may be needed. For now, v1 (source) and v2 (cooked)
  continue to coexist.
- **Cross-platform pak format compatibility** — paks built on one platform are
  assumed to work on others because artifacts carry platform tags. No changes
  planned.
- **Compression and encryption inside paks** — possible future work once the
  data-returning API is in place (phase 5).

---

## 6. Resolved Questions

The questions below were raised during the design discussion and have been
resolved. They are retained as a record of *why* the plan decided what it did.

### Q1. `.rtrproject` contents — what goes inside the marker file?

**Decision**: empty file (or a single comment line). No JSON, no metadata.

**Reasoning**: The marker is dev-only — shipping builds don't search for it. Its
only current use is to serve as a filesystem landmark that `DiscoverRootPath` can
recognize. Other engines' marker files (Unreal's `.uproject`, Godot's
`project.godot`) carry metadata because they serve additional roles: Epic's
launcher lists projects by name, Unreal Build Tool reads enabled modules/plugins,
Godot's editor persists most project settings there. RTRLab has none of those
consumers today — no launcher, no module configuration, no editor. Adding fields
now would be speculative design. The file can evolve to JSON later in a
backward-compatible way if and when a real need arises.

### Q2. Single merged `Game.rtrpak` vs split Engine/Project paks

**Decision**: single merged `Game.rtrpak`.

**Reasoning**: The concern that splitting helps patching is nullified by the
Patch mount system — patches ship as additive `Patches/*.rtrpak` files that
override only the changed entries, so the base pak never needs to be
redistributed wholesale regardless of whether it's merged or split. Merging
simplifies distribution (one file instead of two), simplifies mount discovery
(one hard-coded name instead of two), and is consistent with the Godot-style
flat shipping layout. Splitting would be a special case without a corresponding
benefit.

### Q3. Config file migration through the unified catalog

**Decision**: No migration problem exists. Default config files become document
entries in the source catalog (extension preserved, single artifact). User
override files continue to live in the Saved domain, which bypasses the catalog
and maps directly to the platform user directory. The config subsystem layers
the two at read time.

**Reasoning**: The perceived problem was "what happens to configs when we delete
the document-style path?" The answer: the *runtime resolution* bifurcation goes
away, but the *classification* (some files preserve extensions and have a single
artifact; others strip extensions and have multiple artifacts for format
selection) survives as a rule inside `SourceCatalogBuilder`. Runtime code sees
both kinds of entries through the same `CatalogRegistry::ResolvePath` path. User
overrides are a completely separate concern — they live in the Saved domain,
which is unrelated to the document/asset distinction and is resolved by
`ResolveWritableMount` as it is today. The layered config model (hardcoded
defaults → pak defaults → user overrides → CLI overrides) is a concern of the
config subsystem, not the resource system.

### Q4. `ExtractToTempPath` lifetime

**Decision**: `ExtractToTempPath` is not added. Removed from the plan entirely.

**Reasoning**: The hypothetical use case was third-party libraries that only
accept `const char*` file paths. But modern C++ asset libraries used or likely
to be used in this project (`stb_image`, `tinygltf`, `assimp`, `miniz`, etc.) all
accept memory buffers or streams. `OpenReadStream` covers the remaining
large-file case without any temp-file extraction. Building an extraction helper
and its lifetime management (session temp dir, refcounted handles, or content-hash
cache) for a use case that may never arise is the exact kind of speculative
generality this refactor is trying to remove from the resource system. If a real
case appears later, a small isolated helper can be added at that time.

### Q5. CLI parser choice and Core/ directory cleanup

**Decision**: hand-write a small CLI parser and place it, along with the existing
`Time.{h,cpp}` and `Base.h`, under a new `Src/Core/Util/` directory. See Phase 8
for the mechanical details.

**Reasoning**: No CLI parser currently exists. `Src/main.cpp` does not even
consume `argc`/`argv` today, and the three tools (`AssetCookMain`,
`AssetIndexMain`, `AssetPackMain`) each do ad-hoc manual parsing. Requirements
are small: a handful of `--key=value` and `--flag` options, shipping whitelist
enforcement, and a registration mechanism for subsystems to declare their own
options. A ~200-LoC hand-written parser is cheaper than integrating a third-party
dependency (`cxxopts`, `CLI11`) for this scale. Meanwhile, the existing
`Src/Core/` directory has three loose files (`Base.h`, `Time.h`, `Time.cpp`)
sitting next to its subdirectories, which is untidy. Creating `Src/Core/Util/`
and collecting both the existing loose files and the new `CommandLine.{h,cpp}`
under it normalizes the structure: `Src/Core/` holds only subdirectories.
