# Resource System

The resource system gives RTRLab a stable, engine-owned way to talk about content.
Runtime code refers to resources through logical paths such as `/Project/...` and
`/Engine/...` rather than by concatenating repository or install-relative filesystem
paths.

> **Design Philosophy**: Public resource identity is logical, not physical. The same
> logical path should work across loose source content, loose cooked output, packaged
> archives, and higher-priority override layers. Config and other document-style files
> remain first-class mounted resources, but in the Project / Engine domains they are
> now represented as catalog entries rather than bypassing the catalog at runtime.

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
    - [4.2 Unified Project / Engine Read Resolution](#42-unified-project--engine-read-resolution)
    - [4.3 Writable Domain Resolution](#43-writable-domain-resolution)
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
    - [10.1 Source Catalog Construction](#101-source-catalog-construction)
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
    - [Why packaged archives return data and streams instead of temporary files](#why-packaged-archives-return-data-and-streams-instead-of-temporary-files)

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
      Project / Engine reads      Saved / Cache reads
      (catalog-backed)            (direct writable mounts)
             |                         |
             v                         v
                         CatalogRegistry         Mounted directory roots
                         + artifact selection
                         + mount precedence
                         + backend resolution
                                          |
                                +---------+---------+
                                |                   |
                                v                   v
                        Readable mount backends   Tooling
                        directory / pak /         source scan / cook / pack / stage
                        overlay layers
```

At a high level, the system has four cooperating pieces:

1. **Logical path parsing**
   Validates public paths such as `/Project/...` and `/Saved/...` and extracts the
   logical domain.

2. **Unified catalog-backed read resolution**
   Handles every Project / Engine read, including both extensionless asset paths and
   extension-preserving document paths such as `/Project/Config/*.json`.

3. **Mounted writable-domain resolution**
   Handles `/Saved/...` and `/Cache/...` reads/writes by mapping them directly to
   writable roots.

4. **Tooling**
   Builds source catalogs in memory, generates loose cooked output, and stages packaged
   `.rtrpak` archives so runtime code does not have to guess how an asset is stored.

---

## 3. Logical Path Model

### 3.1 Public Mount Roots

The resource system defines six public logical roots:

| Mount | Purpose | Writable | Typical Examples |
|------|---------|----------|------------------|
| `/Project/` | Project-authored runtime content | No | textures, materials, shaders, scenes, config defaults |
| `/Engine/` | Engine-shipped built-in content | No | fallback materials, shared defaults, editor/support assets |
| `/DLC/<Name>/` | First-party DLC content namespace | No | expansion-specific weapons, maps, config defaults |
| `/Mod/<Name>/` | User-installed mod content namespace | No | mod-local assets and documents |
| `/Saved/` | User and runtime-generated data | Yes | config overrides, logs, save data |
| `/Cache/` | Disposable derived data | Yes | shader cache, cooked intermediates |

`/Project/`, `/Engine/`, `/DLC/<Name>/`, and `/Mod/<Name>/` are read domains.
`/Saved/` and `/Cache/` are write domains. `Patch` is intentionally not a public
path root: patch paks override existing `/Project/...` and `/Engine/...` logical paths
transparently.

### 3.2 Path Categories

The system distinguishes two kinds of public resource paths.

**Catalog-backed asset paths** identify imported/runtime assets and intentionally omit
physical source extensions:

```text
/Project/Textures/Grassy_Square
/Engine/Defaults/Materials/ErrorMaterial
/DLC/Expansion1/Weapons/LaserRifle
/Mod/CoolMod/Weapons/Hammer
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

These retain explicit filenames and extensions. In the Project / Engine domains, they
are represented as document entries in the source/cooked catalog. DLC and Mod mounts
use the same catalog-backed model, but with a required mount-name segment. In the
Saved / Cache domains, they resolve through writable mounts directly.

### 3.3 Path Syntax Contract

Logical paths are part of RTRLab's serialized contract and follow strict rules:

- a path must begin with `/`
- path segments use forward slashes only
- repeated `/` segments are normalized
- `.` and `..` segments are rejected
- `/DLC/` and `/Mod/` require a non-empty mount name segment immediately after the root
- public logical paths are case-sensitive on all platforms

The parser accepts no raw OS path syntax. Strings like
`C:\RTRLab\Project\Textures\X.png` are not resource paths.

---

## 4. Resolution Model

### 4.1 Read Domains and Write Domains

Read APIs may accept any logical domain, but write APIs only accept `/Saved/` and
`/Cache/`.

That means:

- `FileSystem::ReadBinary("/Project/Textures/Grassy_Square")` is valid
- `FileSystem::ResolveWritePath("/Saved/Config/input/Foo.json")` is valid
- `FileSystem::ResolveWritePath("/Project/Config/Foo.json")` is rejected

This rule is enforced centrally by the resource system rather than by each caller.

### 4.2 Unified Catalog-Backed Read Resolution

`/Project/...`, `/Engine/...`, `/DLC/<Name>/...`, and `/Mod/<Name>/...` reads are
resolved uniformly through the catalog system, regardless of whether the logical path
is an extensionless asset path or an extension-preserving document path.

That means:

- `/Project/Textures/Grassy_Square` resolves through a catalog entry whose selected
  artifact may be a source file, cooked file, packaged archive entry, or higher-priority
  DLC / Patch / Mod pak
- `/Project/Config/Graphics.json` also resolves through a catalog entry, but keeps its
  explicit filename and extension
- `/DLC/Expansion1/Weapons/LaserRifle` resolves through the named DLC pak namespace
- `/Mod/CoolMod/Weapons/Hammer` resolves through the named mod pak namespace
- the runtime no longer branches on "has extension" versus "no extension" when deciding
  whether to use the catalog

Patch mounts remain invisible to callers: they override `/Project/...` or `/Engine/...`
paths directly and do not introduce a `/Patch/...` namespace.

The catalog entry type is decided at **source catalog build time**:

- **asset entries** strip the source extension from the logical path
- **document entries** preserve the logical filename and extension and use a single
  artifact with `format="document"`

This builder-time classification is based primarily on content role:

- `Config/` files are documents
- plain-text support files such as `.ini`, `.txt`, `.xml`, and `.toml` may also be
  treated as documents
- content directories such as `Textures/`, `Shaders/`, `Materials/`, `Scenes/`, and
  `Defaults/` remain assets even if the source representation is text-based, such as
  `.json`

### 4.3 Writable Domain Resolution

`/Saved/...` and `/Cache/...` are not catalog-backed. They resolve through mounted
writable roots directly.

In the current repository layout:

| Logical Root | Development Backing |
|-------------|---------------------|
| `/Project/` | `Project/` |
| `/Engine/` | `Engine/` |
| `/Saved/` | `Saved/` in debug/development, platform user-data in shipping-style builds |
| `/Cache/` | `Saved/Cache/` in debug/development, platform cache/user-data in shipping-style builds |

The current config chain is layered on top of those mounts:

1. `/Saved/Config/...`
2. `/Project/Config/...`
3. `/Engine/Config/...`

Missing saved overrides may be auto-seeded from project or engine defaults, but shipped
defaults are never modified in place.

### 4.4 Mount Precedence

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
- a higher-priority DLC / Patch / Mod pak

The public path stays the same.

Readable mounts are merged with explicit precedence. For the current implementation,
the order is:

1. mod mounts
2. patch mounts
3. DLC mounts
4. packaged mounts
5. loose source mounts

If a mod pak supplies `/Project/Textures/Grassy_Square`, it overrides lower-priority
patch, DLC, packaged, and source mounts for that logical path. Unrelated
entries still fall back to the next available lower layer. DLC and Mod paks may also
publish additive content under their own `/DLC/<Name>/...` and `/Mod/<Name>/...`
namespaces.

---

## 5. Catalog System

### 5.1 Source Catalogs

Source catalogs describe loose authoring-time content. At runtime in development builds,
and during cooking, they are built in memory from the source tree rather than being
persisted as a standalone on-disk source-catalog artifact.

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

Shipping currently selects cooked artifacts rather than a distinct third artifact
family. In other words, packaged storage changes the mount backend, not the logical
asset identity.

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
- resolving a selected artifact to a readable descriptor
- serving bytes or streams for directory-backed and pak-backed artifacts
- resolving writable mount roots for `/Saved/` and `/Cache/`

This keeps `ResourceCatalog` focused on catalog semantics and keeps `FileSystem`
focused on public API behavior.

---

## 7. FileSystem Facade

`FileSystem` remains the public facade for the resource system.

Key public responsibilities:

- initialize root discovery and writable roots
- parse logical paths and dispatch by domain
- resolve writable paths
- expose logical-path-based `Exists`, `ReadText`, `ReadBinary`,
  `OpenReadStream`, `WriteText`, and `WriteBinary`
- refresh the catalog registry when mount state changes

Current root discovery behavior is now:

- development builds locate the repo root through a `.rtrproject` marker file,
  with CLI `--root` taking precedence over `RTRL_ROOT`
- release/shipping builds treat the executable directory as the install root by
  default and ignore `RTRL_ROOT`
- `--dev-mode --root <path>` re-enables an explicit development override in
  release/shipping builds, still requiring a `.rtrproject` marker at that path
- `GLAB_ROOT_DIR` remains a development-only compile-time fallback

The facade is now fully logical-path-first. Runtime systems are expected to use:

- `ResolveWritePath()` for writable-domain path creation
- `Exists()`
- `ReadText()` / `ReadBinary()` / `OpenReadStream()`
- `WriteText()` / `WriteBinary()`

Low-level physical I/O still exists in `Resource::ReadTextFile()` and
`Resource::ReadBinaryFile()`, but those helpers live in the `IO` layer rather than on
the public `FileSystem` facade.

---

## 8. Config Integration

Config files are treated as mounted documents, not extensionless imported assets.

That means:

- they keep their explicit filenames and extensions
- Project / Engine defaults are represented as document entries in the catalog
- Saved overrides resolve directly through writable mounts
- they participate in override order, not extensionless asset lookup

Current config locations:

| Logical Path | Physical Development Backing |
|-------------|------------------------------|
| `/Project/Config/...` | `Project/Config/...` |
| `/Engine/Config/...` | `Engine/Config/...` |
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
  "albedo": "Project/textures/Grassy_Square.jpg"
}
```

or this:

```json
{
  "albedo": "C:/Users/name/dev/RTRLab/Project/textures/Grassy_Square.jpg"
}
```

---

## 10. Cooking and Packaging

### 10.1 Source Catalog Construction

Source catalog construction is now an in-memory step shared by dev runtime and cook.

- Dev runtime scans `Project/` and `Engine/` and builds source entries in memory.
- `rtr_asset_cook` performs the same source scan in memory before producing cooked
  artifacts and cooked catalogs.
- The standalone source-index export tool was removed; source catalogs are no longer
  written back to `Project/.rtr/catalog.json` or `Engine/.rtr/catalog.json`.

### 10.2 Loose Cooked Output

`rtr_asset_cook` transforms source catalogs into loose cooked output.

Current staging-oriented cooked layout:

- `build/Packaging/<Config>/Cooked/`

Loose cooked output preserves the same public logical paths as source content. Only the
artifact representation changes.

### 10.3 Packaged Output

`rtr_asset_pack` packages cooked mounts into `.rtrpak` archives.

Current staged packaged layout:

- `build/Stage/<Config>/Game.rtrpak`

The current packaged convention is:

- `Game.rtrpak`

The archive contains a merged cooked catalog plus both `Project/...` and `Engine/...`
payloads under a single pak.

At runtime, packaged Project and Engine reads are still resolved as separate logical
domains, but in shipping they both mount the same shared `Game.rtrpak` and filter its
merged catalog by domain.

Shipping installs may also provide optional overlay pak directories next to the base
archive:

- `DLC/*.rtrpak`
- `Patches/*.rtrpak`
- `Mods/*.rtrpak`

Patch paks override existing `/Project/...` and `/Engine/...` entries transparently.
DLC and Mod paks can do that as well, and may additionally expose additive content
through `/DLC/<Name>/...` and `/Mod/<Name>/...`.

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

As of 2026-04-11, the resource system is no longer a speculative design. It is an
active engine subsystem with the following implemented behavior:

- `src/Core/Resource/` is the home of the module
- `FileSystem` is the public facade
- logical path parsing and domain dispatch are implemented
- read/write domain enforcement is implemented
- `/Project/`, `/Engine/`, `/DLC/<Name>/`, `/Mod/<Name>/`, `/Saved/`, and `/Cache/`
  are active
- config fallback resolution is implemented through serialization-facing helpers
- logical-path-based file I/O is implemented
- dev runtime and cook both build source catalogs in memory
- Project / Engine reads now share a single catalog-driven path for both assets and
  document entries
- loose cooked catalogs are generated and consumed
- packaged `.rtrpak` archives are generated and consumed
- shipping overlay precedence over packaged/source mounts is implemented through
  `DLC`, `Patch`, and `Mod` pak priorities
- mount handling now goes through a backend layer rather than being inlined inside the
  catalog resolver
- `Src/Core/Util/CommandLine.{h,cpp}` now provides shared CLI parsing for `RTRLab`
  and the asset tools
- development root discovery uses a repo-root `.rtrproject` marker instead of
  searching for `Project/`
- release/shipping builds use the executable directory as the install root via the
  existing `RTRLAB_CONFIG_RELEASE` configuration, while `--dev-mode` re-enables
  the remaining dev-only CLI resource override `--root`
- shipping installs scan optional `DLC/`, `Patches/`, and `Mods/` directories for
  additional pak mounts

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
- prefer connecting future loaders to `/Project/...` and `/Engine/...` directly rather
  than introducing new physical-path shortcuts

### 12.4 Shipping Overlay Policy Depth

The current packaged overlay model is intentionally small and explicit:

- packaged content is mounted through `.rtrpak`
- the base game ships as one `Game.rtrpak`
- shipping can add `DLC/`, `Patches/`, and `Mods/` pak directories above the base game
- precedence is defined, but chunk policy and runtime hot-mount behavior are still
  minimal

Near-term plan:

- keep the current shipping mount stack as the baseline behavior
- defer more complex mod/DLC/chunk policies until there is a concrete consumer need
- avoid inventing a broader runtime mount manager before the engine actually needs one

### 12.5 Streaming versus materialized archive entries

Packaged archive entries no longer materialize into cache-backed files as part of the
normal read path. The resource system now resolves a readable artifact descriptor and
serves text, bytes, or streams directly from either a directory-backed file or a
pak-backed entry.

Near-term plan:

- keep `ReadText()` / `ReadBinary()` / `OpenReadStream()` as the primary read surface
- migrate more high-level runtime consumers toward stream-friendly loading where it
  meaningfully reduces memory spikes for large payloads

---

## 13. File Layout

```
src/Core/Util/
    Base.h                    - Core-wide foundational aliases/helpers
    Time.h / .cpp             - Global frame-timing helpers
    CommandLine.h / .cpp      - Shared CLI option registration and parsing

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
        SourceCatalog.h / .cpp  - Source catalog indexing, generation, and document/asset classification
    Cook/
        CookedCatalog.h / .cpp  - Loose cook output + `.rtrtex` helpers
    Package/
        PakArchive.h / .cpp     - `.rtrpak` archive read/write helpers
    IO/
        PhysicalIO.h / .cpp     - Low-level text/binary filesystem I/O

src/Tools/
    AssetCookMain.cpp           - `rtr_asset_cook`
    AssetPackMain.cpp           - `rtr_asset_pack`

.rtrproject                    - Development-time project root marker

Project/
    ...                         - Project source files (catalog built in memory in dev)

Engine/
    ...                         - Engine source files (catalog built in memory in dev)

build/Packaging/<Config>/Cooked/
    ...                         - Staged loose cooked output

build/Stage/<Config>/
    RTRLab(.exe)                - Staged shipping executable
    Game.rtrpak                 - Staged packaged output

<install>/
    RTRLab(.exe)                - Shipping executable
    Game.rtrpak                 - Base packaged content
    DLC/                        - Optional DLC pak directory
    Patches/                    - Optional patch pak directory
    Mods/                       - Optional user mod pak directory
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
useful than pretending they are catalog-backed extensionless assets. In the current
architecture they still participate in the Project / Engine catalog path, but they do
so as explicit document entries rather than as extensionless assets.

### Why source and cooked catalogs are separate schemas

Source catalogs need provenance like `sourceRelativePath`; cooked catalogs do not.
Keeping the schemas separate prevents runtime code from accidentally depending on
authoring-only details and makes the transition to richer cooked metadata cleaner.

### Why packaged archives return data and streams instead of temporary files

The current resource API is data-first: callers are expected to consume text, binary
buffers, or `istream` objects instead of asking the resource system for a physical file
path. That lets packaged mounts read directly from `.rtrpak` entries without extracting
temporary files into a cache directory first, and it keeps directory-backed and
pak-backed reads behind the same public contract.
