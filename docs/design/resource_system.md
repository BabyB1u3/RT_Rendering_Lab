# Resource System

The resource system gives RTRLab a stable, engine-owned way to talk about content.
Runtime code refers to resources through logical paths such as `/Project/...`,
`/Engine/...`, and `/Plugins/<Name>/...` rather than by concatenating repository or
install-relative filesystem paths.

> **Design Philosophy**: Public resource identity is logical, not physical. The same
> logical path should work across loose source content, loose cooked output, packaged
> archives, and higher-priority override layers. Config and other document-style files
> remain first-class mounted resources, but they are intentionally distinct from
> catalog-backed imported assets.

---

## Table of Contents

- [Resource System](#resource-system)
  - [Table of Contents](#table-of-contents)
  - [1. Motivation](#1-motivation)
  - [2. Architecture Overview](#2-architecture-overview)
  - [3. Logical Path Model](#3-logical-path-model)
    - [3.1 Public Mount Roots](#31-public-mount-roots)
    - [3.2 Path Categories](#32-path-categories)
    - [3.3 Path Syntax Contract](#33-path-syntax-contract)
  - [4. Resolution Model](#4-resolution-model)
    - [4.1 Read Domains and Write Domains](#41-read-domains-and-write-domains)
    - [4.2 Document-Style Resolution](#42-document-style-resolution)
    - [4.3 Catalog-Backed Resolution](#43-catalog-backed-resolution)
    - [4.4 Mount Precedence](#44-mount-precedence)
  - [5. Catalog System](#5-catalog-system)
    - [5.1 Source Catalogs](#51-source-catalogs)
    - [5.2 Cooked Catalogs](#52-cooked-catalogs)
    - [5.3 Artifact Selection](#53-artifact-selection)
    - [5.4 Conflict Handling](#54-conflict-handling)
  - [6. Mount Backends](#6-mount-backends)
  - [7. FileSystem Facade](#7-filesystem-facade)
  - [8. Config Integration](#8-config-integration)
  - [9. Serialization Integration](#9-serialization-integration)
  - [10. Cooking and Packaging](#10-cooking-and-packaging)
    - [10.1 Source Indexing](#101-source-indexing)
    - [10.2 Loose Cooked Output](#102-loose-cooked-output)
    - [10.3 Packaged Output](#103-packaged-output)
    - [10.4 Current Cooked Texture Format](#104-current-cooked-texture-format)
  - [11. Current Implementation Status](#11-current-implementation-status)
  - [12. Current Gaps and Near-Term Plan](#12-current-gaps-and-near-term-plan)
  - [13. File Layout](#13-file-layout)
  - [14. Key Design Decisions](#14-key-design-decisions)
    - [Why logical paths are case-sensitive on every platform](#why-logical-paths-are-case-sensitive-on-every-platform)
    - [Why config files are mounted documents, not extensionless assets](#why-config-files-are-mounted-documents-not-extensionless-assets)
    - [Why source and cooked catalogs are separate schemas](#why-source-and-cooked-catalogs-are-separate-schemas)
    - [Why packaged archives materialize into cache-backed files today](#why-packaged-archives-materialize-into-cache-backed-files-today)

---

## 1. Motivation

The original project treated content lookup mostly as filesystem string-joining:
`assets/` for shipped data, `saved/` for writable data, and a handful of callsites that
each knew their own relative directory rules. That approach worked while the codebase
was small, but it had three structural problems:

| Problem | Impact |
|---------|--------|
| Public references depended on physical layout | Serialized data and runtime code would break when directories changed |
| Source, cooked, and packaged content were different code paths | Runtime systems could not treat them as interchangeable storage backends |
| Read-only content and writable user data were mixed conceptually | Config, logs, cache data, and assets all wanted different rules but shared ad hoc helpers |

The current resource system fixes that by separating **public identity** from
**physical storage**:

- public identity is a logical path such as `/Project/Textures/Grassy_Square`
- physical storage may be a loose file, cooked artifact, packaged archive entry, or
  override layer
- writable data lives in explicit `/Saved/` and `/Cache/` domains instead of pretending
  to be project content

That gives RTRLab the same key property that larger engines rely on: a resource path can
stay stable while the storage backend evolves.

---

## 2. Architecture Overview

```
                      Public API
                /-------------------\
                |    FileSystem     |
                | logical path I/O  |
                \---------+---------/
                          |
             +------------+------------+
             |                         |
             v                         v
      Document-style paths       Catalog-backed paths
      (/Saved/Config/...)        (/Project/Textures/...)
             |                         |
             v                         v
     Mounted directory roots    CatalogRegistry
                                + artifact selection
                                + mount precedence
                                + backend resolution
                                          |
                                +---------+---------+
                                |                   |
                                v                   v
                        Readable mount backends   Tooling
                        directory / pak /         source index / cook / pack
                        overlay layers
```

At a high level, the system has four cooperating pieces:

1. **Logical path parsing**
   Validates and classifies public paths such as `/Project/...` and `/Saved/...`.

2. **Mounted filesystem resolution**
   Handles document-style resources that map directly to files, especially config,
   logs, save data, and cache data.

3. **Catalog-backed asset resolution**
   Handles extensionless asset paths by loading catalogs from active readable mounts and
   selecting the best artifact for the current runtime.

4. **Tooling**
   Generates source catalogs, loose cooked output, and packaged `.rtrpak` archives so
   runtime code does not have to guess how an asset is stored.

---

## 3. Logical Path Model

### 3.1 Public Mount Roots

The resource system defines five public logical roots:

| Mount | Purpose | Writable | Typical Examples |
|------|---------|----------|------------------|
| `/Project/` | Project-authored runtime content | No | textures, materials, shaders, scenes, config defaults |
| `/Engine/` | Engine-shipped built-in content | No | fallback materials, shared defaults, editor/support assets |
| `/Plugins/<Name>/` | Plugin-owned content | No | plugin materials, plugin defaults, plugin-specific assets |
| `/Saved/` | User and runtime-generated data | Yes | config overrides, logs, save data |
| `/Cache/` | Disposable derived data | Yes | shader cache, cooked intermediates, extracted packaged artifacts |

`/Project/`, `/Engine/`, and `/Plugins/<Name>/` are read domains. `/Saved/` and
`/Cache/` are write domains.

### 3.2 Path Categories

The system distinguishes two kinds of public resource paths.

**Catalog-backed asset paths** identify imported/runtime assets and intentionally omit
physical source extensions:

```text
/Project/Textures/Grassy_Square
/Engine/Defaults/Materials/ErrorMaterial
/Plugins/ExamplePlugin/Materials/Checker
```

These resolve through a resource catalog rather than by probing the filesystem.

**Document-style resource paths** identify directly readable files and retain their
filenames and extensions:

```text
/Project/Config/input/DebugCameraControl.json
/Engine/Config/input/DefaultBindings.json
/Saved/Config/imgui.ini
/Saved/logs/RTRLab.log
```

These resolve through mounted backends directly and do not participate in extensionless
asset lookup.

### 3.3 Path Syntax Contract

Logical paths are part of RTRLab's serialized contract and follow strict rules:

- a path must begin with `/`
- path segments use forward slashes only
- repeated `/` segments are normalized
- `.` and `..` segments are rejected
- public logical paths are case-sensitive on all platforms
- `/Plugins/<Name>/...` mount names must be valid identifiers:
  non-empty, ASCII letter first, then ASCII letters, digits, or `_`

The parser accepts no raw OS path syntax. Strings like
`C:\Project\Content\Textures\X.png` are not resource paths.

---

## 4. Resolution Model

### 4.1 Read Domains and Write Domains

Read APIs may accept any logical domain, but write APIs only accept `/Saved/` and
`/Cache/`.

That means:

- `FileSystem::ResolveReadPath("/Project/Textures/Grassy_Square")` is valid
- `FileSystem::ResolveWritePath("/Saved/Config/input/Foo.json")` is valid
- `FileSystem::ResolveWritePath("/Project/Config/Foo.json")` is rejected

This rule is enforced centrally by the resource system rather than by each caller.

### 4.2 Document-Style Resolution

Document-style paths resolve through mounted backends directly.

In the current repository layout:

| Logical Root | Development Backing |
|-------------|---------------------|
| `/Project/` | `Content/` |
| `/Engine/` | `EngineContent/` |
| `/Plugins/<Name>/` | `Plugins/<Name>/Content/` |
| `/Saved/` | `Saved/` in debug/development, platform user-data in shipping-style builds |
| `/Cache/` | `Saved/Cache/` in debug/development, platform cache/user-data in shipping-style builds |

The current config chain is layered on top of those mounts:

1. `/Saved/Config/...`
2. `/Project/Config/...`
3. `/Engine/Config/...`

Missing saved overrides may be auto-seeded from project or engine defaults, but shipped
defaults are never modified in place.

### 4.3 Catalog-Backed Resolution

Catalog-backed logical paths resolve in two stages:

1. **Catalog lookup**
   Find a `ResourceCatalogEntry` for the requested logical path in the merged global
   table.

2. **Artifact resolution**
   Choose the best matching artifact for the current runtime and resolve that artifact
   through the owning mount backend.

The caller never sees whether the chosen artifact came from:

- a loose source directory
- a loose cooked directory
- a packaged `.rtrpak` archive
- a higher-priority overlay directory

The public path stays the same.

### 4.4 Mount Precedence

Readable mounts are merged with explicit precedence. For the current implementation,
the order is:

1. overlay mounts
2. packaged mounts
3. loose cooked mounts
4. loose source mounts

This precedence applies within the same logical namespace. Plugin content does not
implicitly override `/Project/...`; it only affects `/Plugins/<Name>/...`.

If an overlay supplies `/Project/Textures/Grassy_Square`, it overrides lower-priority
project mounts for that logical path. Unrelated project entries still fall back to the
next available lower layer.

---

## 5. Catalog System

### 5.1 Source Catalogs

Source catalogs describe loose authoring-time content. They are generated under each
mount root as `.rtr/catalog.json`.

Current source catalogs:

- use `version = 1`
- include a `logicalPath`
- include a required `sourceRelativePath`
- include one or more candidate `artifacts`

Example shape:

```json
{
  "version": 1,
  "entries": [
    {
      "logicalPath": "/Project/Textures/Grassy_Square",
      "sourceRelativePath": "textures/Grassy_Square.jpg",
      "artifacts": [
        {
          "relativePath": "textures/Grassy_Square.jpg",
          "format": "jpg",
          "profileTag": "dev",
          "backendTag": "any",
          "platformTag": "any"
        }
      ]
    }
  ]
}
```

### 5.2 Cooked Catalogs

Cooked catalogs describe runtime-ready artifacts rather than authoring-time source
files. They currently use a separate loose JSON schema:

- `version = 2`
- `kind = "cooked"`
- entries omit `sourceRelativePath`
- artifacts point at cooked payloads such as `.rtrtex`

This split matters because runtime code should not assume cooked output still has a
meaningful source-relative path.

### 5.3 Artifact Selection

Each catalog entry can contain multiple artifacts. Runtime chooses the best one using:

- `profileTag`
- `backendTag`
- `platformTag`

The current scoring rule is:

- exact match beats `any`
- explicit mismatch rejects the artifact
- profile dominates backend, which dominates platform

Packaged and shipping profiles currently select cooked artifacts rather than a distinct
third artifact family. In other words, packaged storage changes the mount backend, not
the logical asset identity.

### 5.4 Conflict Handling

Conflict handling happens at two levels.

**Within a single catalog**
Duplicate `logicalPath` entries are invalid and the catalog is rejected.

**Across active mounts**
Equal-precedence duplicates are treated as a conflict and the logical path is removed
from the merged table. Higher-precedence mounts are allowed to replace lower-precedence
entries.

This is what allows overlay layers to replace packaged or source content without making
equal-priority duplicates ambiguous.

---

## 6. Mount Backends

The current runtime treats storage as a backend detail behind a common mount layer.

| Backend | Current Role |
|--------|---------------|
| Directory mount | Loose source content, loose cooked output, overlay roots |
| Writable user-directory mount | `/Saved/` and `/Cache/` roots |
| Pak archive mount | Packaged cooked content in `.rtrpak` archives |

The backend layer is responsible for:

- discovering readable mounts for the current profile
- checking whether a mount exposes a catalog
- reading mount-local catalog bytes
- resolving a selected artifact to a physical file path
- materializing pak entries when the runtime still expects a file path
- resolving writable mount roots for `/Saved/` and `/Cache/`

This keeps `ResourceCatalog` focused on catalog semantics and keeps `FileSystem`
focused on public API behavior.

---

## 7. FileSystem Facade

`FileSystem` remains the public facade for the resource system.

Key public responsibilities:

- initialize root discovery and writable roots
- parse and classify logical paths
- resolve read and write paths
- expose logical-path-based `Exists`, `ReadText`, `ReadBinary`, `WriteText`,
  and `WriteBinary`
- refresh the catalog registry when mount state changes

The facade is now fully logical-path-first. Runtime systems are expected to use:

- `ResolveReadPath()` / `ResolveWritePath()`
- `Exists()`
- `ReadText()` / `ReadBinary()`
- `WriteText()` / `WriteBinary()`

Low-level physical I/O still exists in `Resource::ReadTextFile()` and
`Resource::ReadBinaryFile()`, but those helpers live in the `IO` layer rather than on
the public `FileSystem` facade.

---

## 8. Config Integration

Config files are treated as mounted documents, not imported assets.

That means:

- they keep their explicit filenames and extensions
- they resolve directly through mounted filesystems
- they participate in override order, not extensionless catalog lookup

Current config locations:

| Logical Path | Physical Development Backing |
|-------------|------------------------------|
| `/Project/Config/...` | `Content/Config/...` |
| `/Engine/Config/...` | `EngineContent/Config/...` |
| `/Saved/Config/...` | `Saved/Config/...` |

`Serialization::LoadFromConfigPath()` and `SaveToConfigPath()` sit on top of that
logical model and are the intended high-level entry points for config consumers.

---

## 9. Serialization Integration

The serialization system distinguishes between:

- **asset references** that should serialize as logical resource paths
- **document/config I/O** that should resolve through logical mounted files

Current integration points:

- `Serialization::SaveToVirtualPath()` / `LoadFromVirtualPath()`
- `Serialization::SaveToConfigPath()` / `LoadFromConfigPath()`
- validated `Resource::AssetPath` for serialized asset references

Serialized asset references are expected to look like this:

```json
{
  "albedo": "/Project/Textures/Grassy_Square"
}
```

Not like this:

```json
{
  "albedo": "Content/textures/Grassy_Square.jpg"
}
```

or this:

```json
{
  "albedo": "C:/Users/name/dev/RTRLab/Content/textures/Grassy_Square.jpg"
}
```

---

## 10. Cooking and Packaging

### 10.1 Source Indexing

`rtr_asset_index` scans readable loose content roots and generates source catalogs for:

- `Content/`
- `EngineContent/`
- `Plugins/<Name>/Content/`

It skips document-style config subtrees for extensionless asset indexing and enforces
logical-path uniqueness.

### 10.2 Loose Cooked Output

`rtr_asset_cook` transforms source catalogs into loose cooked output.

Current supported loose cooked layouts:

- `Saved/Cache/Cooked/`
- `build/Cooked/`

Runtime can also be pointed at an explicit cooked root through `RTRLAB_COOKED_ROOT`.

Loose cooked output preserves the same public logical paths as source content. Only the
artifact representation changes.

### 10.3 Packaged Output

`rtr_asset_pack` packages cooked mounts into `.rtrpak` archives.

Current packaged layouts:

- `Saved/Cache/Packaged/`
- `build/Packaged/`

Runtime can also be pointed at an explicit packaged root through `RTRLAB_PACKAGE_ROOT`.

The current packaged convention is:

- `Project.rtrpak`
- `Engine.rtrpak`
- `Plugins/<Name>.rtrpak`

Each archive contains the cooked catalog for that mount plus the cooked artifacts it
references.

### 10.4 Current Cooked Texture Format

Cooked textures currently use an engine-private `.rtrtex` format.

The current format is intentionally modest:

- versioned header
- explicit width, height, channel count
- explicit pixel format
- explicit `rowPitch`
- explicit `mipLevelCount`
- payload bytes following the header

Runtime can read metadata only or load the full payload:

- `Resource::ReadCookedTextureMetadata()`
- `Resource::LoadCookedTexture()`

This is sufficient for the current loose cooked and packaged contracts, even though it
is not yet a fully evolved long-term texture container.

---

## 11. Current Implementation Status

As of 2026-04-08, the resource system is no longer a speculative design. It is an
active engine subsystem with the following implemented behavior:

- `src/Core/Resource/` is the home of the module
- `FileSystem` is the public facade
- logical path parsing and classification are implemented
- read/write domain enforcement is implemented
- `/Project/`, `/Engine/`, `/Plugins/<Name>/`, `/Saved/`, and `/Cache/` are active
- config fallback resolution is implemented through serialization-facing helpers
- logical-path-based file I/O is implemented
- source catalogs are generated and consumed
- loose cooked catalogs are generated and consumed
- packaged `.rtrpak` archives are generated and consumed
- overlay precedence over packaged/cooked/source mounts is implemented
- mount handling now goes through a backend layer rather than being inlined inside the
  catalog resolver

The core path and mount architecture is in place, but the system is not "fully done" in
the sense of having every long-term artifact format, mount policy, and runtime consumer
already built. The remaining work is now mostly about formalizing the current bootstrap
formats and connecting more engine systems to the logical resource model.

---

## 12. Current Gaps and Near-Term Plan

The resource system is intentionally past the "just a proposal" phase, but it still has
some known unfinished areas. Those gaps are not in the public logical path contract;
they are mostly in how far the current tooling and runtime consumers have been pushed.

### 12.1 Cooked catalog formalization

Cooked catalogs currently use a loose JSON schema that is good enough for development,
testing, and the current packaged MVP. What it is not yet is a finalized long-term
runtime catalog container.

Near-term plan:

- keep the current versioned JSON cooked schema as the working contract
- defer a binary cooked catalog format until the runtime has stronger reasons to need
  it, such as startup cost or package size pressure
- preserve the rule that cooked and packaged artifacts do not change public logical
  paths

### 12.2 Cooked texture evolution

`.rtrtex` is now a real engine-owned cooked texture artifact, but it is still closer to
a bootstrap runtime container than to a finished production texture format.

Near-term plan:

- extend the current format beyond the single-layer RGBA8 baseline
- add richer metadata where needed, such as mip-chain layout and clearer long-term
  pixel-format semantics
- only do that work once there is a real runtime texture consumer path that benefits
  from it

### 12.3 Runtime consumer integration

The resource system itself is now much further along than the current renderer/runtime
consumer layer. In practice, that means the path/cook/package infrastructure exists
before there are many high-level systems that fully exercise it.

Near-term plan:

- keep the logical path contract stable
- integrate actual runtime resource consumers when the rendering/material path is ready
- prefer connecting future loaders to `/Project/...`, `/Engine/...`, and
  `/Plugins/<Name>/...` directly rather than introducing new physical-path shortcuts

### 12.4 Overlay and packaging policy depth

The current packaged/overlay model is intentionally small and explicit:

- packaged content is mounted through `.rtrpak`
- one loose override layer can sit above packaged/cooked/source mounts
- precedence is defined, but multi-layer patching and chunk policy are still minimal

Near-term plan:

- keep the current override model as the baseline behavior
- defer more complex mod/DLC/chunk policies until there is a concrete consumer need
- avoid inventing a broader overlay stack before the runtime actually needs one

### 12.5 Streaming versus materialized archive entries

Packaged archive entries currently materialize into cache-backed files so the rest of
the runtime can keep consuming physical file paths.

Near-term plan:

- keep materialization because it minimizes churn for current systems
- revisit direct stream-style resource access only when higher-level runtime loaders are
  in place and the cost of temporary extraction becomes meaningful

---

## 13. File Layout

```
src/Core/Resource/
    FileSystem.h / .cpp         - Public facade and logical-path I/O entry points
    Path/
        PathTypes.h             - Path domain and parsed virtual-path types
        PathParser.h / .cpp     - Logical path validation and normalization
    Mount/
        MountResolver.h / .cpp  - Writable-root policy and direct mounted path joins
        MountBackend.h / .cpp   - Readable/writable backend abstraction layer
        RootDiscovery.h / .cpp  - Repository/install root discovery
    Catalog/
        AssetPath.h             - Validated serialized asset reference type
        ResourceCatalog.h / .cpp - Catalog registry, merge logic, artifact selection
        SourceCatalog.h / .cpp  - Source catalog indexing and generation
    Cook/
        CookedCatalog.h / .cpp  - Loose cook output + `.rtrtex` helpers
    Package/
        PakArchive.h / .cpp     - `.rtrpak` archive read/write helpers
    IO/
        PhysicalIO.h / .cpp     - Low-level text/binary filesystem I/O

src/Tools/
    AssetIndexMain.cpp          - `rtr_asset_index`
    AssetCookMain.cpp           - `rtr_asset_cook`
    AssetPackMain.cpp           - `rtr_asset_pack`

Content/
    .rtr/catalog.json           - Generated project source catalog

EngineContent/
    .rtr/catalog.json           - Generated engine source catalog

Plugins/<Name>/Content/
    .rtr/catalog.json           - Generated plugin source catalog

Saved/Cache/Cooked/
    ...                         - Loose cooked output

Saved/Cache/Packaged/
    ...                         - Packaged `.rtrpak` output

Saved/Overrides/
    ...                         - Development-time loose overlay root
```

---

## 14. Key Design Decisions

### Why logical paths are case-sensitive on every platform

RTRLab wants one serialized contract across Windows, macOS, and Linux. Treating logical
paths as case-sensitive even on case-insensitive filesystems prevents subtle
platform-specific ambiguity and makes mismatched casing an authoring error instead of a
runtime lottery.

### Why config files are mounted documents, not extensionless assets

Config files are consumed as documents, not imported as opaque runtime artifacts. Their
format is part of their contract, so keeping explicit filenames and extensions is more
useful than pretending they are catalog-backed assets.

### Why source and cooked catalogs are separate schemas

Source catalogs need provenance like `sourceRelativePath`; cooked catalogs do not.
Keeping the schemas separate prevents runtime code from accidentally depending on
authoring-only details and makes the transition to richer cooked metadata cleaner.

### Why packaged archives materialize into cache-backed files today

Most of the current runtime still wants a physical file path after resource resolution.
Materializing pak entries into cache-backed files lets packaged mounts participate in
the same path-based consumers without forcing a larger streaming/IO API redesign first.
