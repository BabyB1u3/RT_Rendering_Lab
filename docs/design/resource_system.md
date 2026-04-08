# Resource System Design

This document defines the long-term resource architecture for RTRLab.

The target is an Unreal-style system:

- application code refers to resources through **logical engine paths**
- runtime code does **not** depend on OS-specific physical paths
- project content, engine content, plugin content, user-writable data, and caches are
  separate concerns
- loose files, cooked output, and packaged archives are interchangeable **mount
  backends**, not different public APIs

This document intentionally does **not** treat the current shader pipeline as an
architectural dependency. Shader compilation, cooking, and archive packaging must fit
into the resource model defined here, not define it.

## 0. Implementation Status Snapshot

This document still describes the long-term architecture, but parts of the early
phases are now implemented in the codebase.

Current state as of 2026-04-08:

- the resource/path module now lives under `src/Core/Resource/`
- `FileSystem` remains the public facade, with a thin compatibility forwarding header
  at `src/Core/FileSystem.h`
- logical path parsing is implemented for `/Project/`, `/Engine/`,
  `/Plugins/<Name>/`, `/Saved/`, and `/Cache/`
- document-style path resolution is implemented for direct file-backed reads/writes
- logical-path-based `Exists()`, `ReadText()`, `ReadBinary()`, `WriteText()`, and
  `WriteBinary()` are implemented
- compatibility wrappers such as `GetAssetPath()`, `GetSavedPath()`,
  `GetSavedConfigPath()`, and `ResolveConfigPath()` still exist during migration
- the config resolution chain is implemented as
  `/Saved/Config -> /Project/Config -> /Engine/Config`
- the repository now uses `Content/` for project content, and debug/development
  writable roots resolve under `Saved/`
- shipped project config defaults now live under `Content/Config/`
- shipped engine config defaults now live under `EngineContent/Config/`
- runtime log, crash, and ImGui ini callsites now resolve through explicit logical
  `/Saved/...` paths rather than legacy saved-path wrappers
- diagnostics now default to `/Saved/logs/RTRLab.log`, and the JSON sink can resolve
  explicit logical write paths such as `/Saved/Logs/*.jsonl`
- serialization now provides logical-path wrappers for document-style file I/O through
  the resource system
- asset-reference serialization now has an explicit validated path type instead of
  relying on raw strings
- a minimal source-catalog JSON loader and merged loose-mount resolution table are
  now implemented for readable mounts
- extensionless project, engine, and plugin asset paths can now resolve through
  source catalogs in development builds
- the current runtime can explicitly refresh catalog state when readable mounts
  change after initial lookup
- a source catalog indexing tool now exists and can generate `.rtr/catalog.json`
  for project, engine, and plugin loose content mounts
- a minimal cooked catalog generation tool now exists for loose development-time
  cooked mounts under `Saved/Cache/Cooked/`
- the current runtime can load and validate the bootstrap cooked texture payload
  written at cooked `.rtrtex` artifact paths

---

## 1. Design Goals

### 1.1 Primary goals

1. Make resource references stable, readable, and independent of disk layout.
2. Support Unreal-style logical roots such as `/Project/...` and `/Engine/...`.
3. Separate read-only shipped content from user-writable data.
4. Allow the same logical path to resolve from loose files in development and from
   cooked packages in shipping builds.
5. Make future plugin, mod, DLC, and patch support possible without changing gameplay
   code or serialization formats.

### 1.2 Non-goals

- Reproducing Unreal's entire asset registry, `.uasset` format, or editor pipeline.
- Locking the engine to a specific shader, model, or texture pipeline.
- Making archive packaging mandatory in the first implementation.

---

## 2. Core Decision

RTRLab adopts **logical mount-point paths** as its public resource identity.

Examples:

```text
/Project/Textures/Grassy_Square
/Project/Shaders/ForwardLit
/Engine/Editor/Icons/Play
/Plugins/ExamplePlugin/Materials/Checker
```

These are **resource paths**, not OS filesystem paths.

Application code, serialized scene data, material definitions, config defaults, and
 future asset references should use these logical paths wherever the reference means
"load a resource from the engine's resource space."

Physical filesystem paths remain an implementation detail inside the resource system.

---

## 3. Path Taxonomy

RTRLab distinguishes three categories of paths:

### 3.1 Resource paths

Logical read-oriented paths that identify shipped or mountable content.

Examples:

```text
/Project/Textures/Grassy_Square
/Engine/Shaders/Common/Fullscreen
/Plugins/Foo/Materials/DebugGrid
```

Properties:

- stable across development, install, and packaging
- valid in serialized data
- resolved through the resource mount table
- never directly concatenated with OS paths by gameplay code

#### 3.1.1 Catalog-backed asset paths

Most runtime content assets use **catalog-backed logical paths**.

Examples:

```text
/Project/Textures/Grassy_Square
/Project/Shaders/ForwardLit
/Plugins/Foo/Materials/DebugGrid
```

Properties:

- do **not** encode physical source extensions
- do **not** change when content is cooked
- resolve through the resource catalog, not by probing the filesystem for possible
  extensions
- are the preferred form for serialized asset references

#### 3.1.2 Document-style resource paths

Some resource subtrees intentionally retain explicit file names and extensions.

Examples:

```text
/Project/Config/Input/DebugCameraControl.json
/Engine/Config/Input/DefaultBindings.json
/Saved/Config/Input/DebugCameraControl.json
```

Properties:

- represent directly readable documents rather than catalog-backed imported assets
- keep extensions because the document format itself is part of the contract
- resolve directly through mounted filesystems
- do **not** participate in extensionless asset lookup through the resource catalog

### 3.2 User-data paths

Logical write-oriented paths for runtime-generated or user-edited data.

Examples:

```text
/Saved/Config/Input/DebugCameraControl.json
/Saved/Logs/RTRLab.log
/Saved/Saves/Slot01/save.json
/Cache/Shaders/ForwardLit.metallib
```

Properties:

- not part of shipped read-only content
- writable by the application
- mapped to platform-specific writable locations in non-development builds
- generally should **not** appear as long-lived asset references inside scene/material
  data

### 3.3 Native filesystem paths

Concrete `std::filesystem::path` values used internally by low-level file I/O.

Examples:

```text
/Users/name/Projects/RTRLab/Content/Textures/Grassy_Square.jpg
C:\Users\name\AppData\Local\RTRLab\Saved\Logs\RTRLab.log
```

Properties:

- valid only inside the platform/filesystem layer
- not used as stable identifiers
- not serialized as asset references

---

## 4. Public Mount Points

RTRLab defines the following logical roots.

| Mount | Purpose | Writable | Serialized as asset reference | Notes |
|------|---------|----------|-------------------------------|-------|
| `/Project/` | Project-authored runtime content | No | Yes | Main game/demo content root |
| `/Engine/` | Engine-shipped built-in content | No | Yes | Shared default assets, editor icons, fallback materials |
| `/Plugins/<Name>/` | Plugin-provided content | No | Yes | Optional; mounted only when plugin is enabled |
| `/Saved/` | User/runtime-generated data | Yes | Rarely | Logs, per-user config overrides, save files |
| `/Cache/` | Disposable derived data | Yes | No | Shader caches, cooked intermediates, thumbnails |

### 4.1 Rules

- `/Project/`, `/Engine/`, and `/Plugins/...` are **read domains**.
- `/Saved/` and `/Cache/` are **write domains**.
- Read APIs may accept any mount.
- Write APIs must reject writes to read domains.
- Asset references stored in user-facing/runtime content should normally point only to
  `/Project/`, `/Engine/`, or `/Plugins/...`.

### 4.2 Why this model

This gives RTRLab the most important Unreal-like property:

the public identity of a resource is its **mounted logical path**, not where that data
physically lives today.

That means `/Project/Textures/Grassy_Square` can come from:

- a loose development directory
- a cooked output directory
- a packaged archive
- a hotfix overlay

without changing any serialized references or gameplay code.

For clarity, plugin physical `Content/` directories are **mount roots**, not visible
logical path segments.

Example:

- physical: `Plugins/Foo/Content/Materials/DebugGrid.json`
- logical: `/Plugins/Foo/Materials/DebugGrid`

---

## 5. Proposed Physical Layout

This document recommends a deliberate directory layout centered on `Content/` rather
than `assets/`.

### 5.1 Recommended development layout

```text
RTRLab/
  Content/                    ← mounted as /Project/
    Textures/
    Shaders/
    Materials/
    Scenes/
    Config/                   ← /Project/Config/
  EngineContent/              ← mounted as /Engine/ (optional at first)
    Editor/
    Defaults/
    Config/                   ← /Engine/Config/
  Plugins/
    ExamplePlugin/
      Content/                ← mounted as /Plugins/ExamplePlugin/
      Config/
  Saved/                      ← mounted as /Saved/ in development
    Config/
    Logs/
    Saves/
  Cache/                      ← mounted as /Cache/ in development
    Shaders/
    Cooked/
  build/
```

### 5.2 Installed layout

```text
RTRLab/
  RTRLab.exe
  Content/                    ← /Project/
    Config/                   ← /Project/Config/
  EngineContent/              ← /Engine/
    Config/                   ← /Engine/Config/
  Plugins/
  Saved/                      ← optional fallback only; normally user dir in shipping
  Cache/                      ← optional fallback only; normally user dir in shipping
```

### 5.3 Platform user-data layout

In shipping builds:

- `/Saved/` maps to the platform user data directory
- `/Cache/` maps to a platform cache directory when available, otherwise under `/Saved/`

Typical mappings:

| Platform | `/Saved/` | `/Cache/` |
|----------|-----------|-----------|
| Windows | `%LOCALAPPDATA%/RTRLab/Saved/` | `%LOCALAPPDATA%/RTRLab/Cache/` |
| macOS | `~/Library/Application Support/RTRLab/Saved/` | `~/Library/Caches/RTRLab/` |
| Linux | `$XDG_DATA_HOME/RTRLab/Saved/` or `~/.local/share/RTRLab/Saved/` | `$XDG_CACHE_HOME/RTRLab/` or `~/.cache/RTRLab/` |

### 5.4 Compatibility note

The repository now uses `Content/` for `/Project/` and `Saved/` for writable
development data.

Current development mappings:

- `Content/` as `/Project/`
- `Saved/` as `/Saved/`
- `Saved/Cache/` as `/Cache/` in development
- `EngineContent/` as `/Engine/`
- `Plugins/<Name>/Content/` as `/Plugins/<Name>/`

Current layout details:

- shipped config defaults live under `Content/Config/`
- writable config overrides live under `Saved/Config/`
- `ResolveConfigPath()` follows the logical override order
  `/Saved/Config -> /Project/Config -> /Engine/Config`
- writable roots resolve to repository-local `Saved/` paths in debug/development
  builds and to platform user-data directories in shipping-style builds

The earlier `assets/` / `saved/` compatibility bridge is no longer the primary
repository layout.

---

## 6. Path Syntax Contract

### 6.1 Grammar

All logical paths use forward slashes:

```text
/MountName/Relative/Path[.ext]
```

`[.ext]` is optional:

- catalog-backed asset paths typically omit physical extensions
- document-style resource paths keep explicit file extensions

Examples:

```text
/Project/Textures/Grassy_Square
/Project/Shaders/ForwardLit
/Engine/Defaults/Materials/ErrorMaterial.json
/Plugins/Foo/Textures/Icon
/Saved/Config/imgui.ini
```

### 6.2 Normalization rules

The parser must:

- require a leading `/`
- require a valid mount root
- collapse repeated `/`
- reject `.` and `..` segments
- reject backslashes in public logical paths
- treat logical paths as **case-sensitive** on all platforms
- require repository content naming to match serialized logical path casing exactly

Plugin mount names under `/Plugins/<Name>/...` additionally follow identifier rules:

- non-empty
- ASCII letter as the first character
- remaining characters limited to ASCII letters, digits, and `_`

Physical plugin directories that do not satisfy those rules are not treated as
mountable plugin content roots by discovery/tooling.

This intentionally favors a stricter cross-platform contract over platform-local
filesystem convenience. A path that differs only by case must be treated as a
different logical path, and mismatched casing should be considered an authoring error.

### 6.3 No raw OS path leakage

Public resource-facing APIs must not require callers to know:

- project root absolute path
- executable directory
- install prefix
- archive location
- platform user-data directory

Those remain internal resolution details.

---

## 7. Resource System Responsibilities

RTRLab's resource/path layer is responsible for:

1. parsing logical paths
2. validating mount roots
3. loading and merging resource catalogs from active mounts
4. resolving catalog-backed asset paths through the merged catalog
5. resolving document-style paths through mounted backends
6. resolving write paths for writable domains
7. performing file I/O by logical path
8. enforcing read/write domain rules
9. exposing limited directory enumeration where needed

It is **not** responsible for:

- decoding textures, models, or shader binaries
- understanding scene/material semantics
- being the editor asset database

---

## 8. API Direction

The original single-file `FileSystem` bootstrap has now been split into a dedicated
resource module under `src/Core/Resource/`, with `FileSystem` retained as the public
facade.

### 8.1 Current implemented shape

The current codebase already exposes:

- logical path parsing and classification:
  `IsVirtualPath()`, `ParseVirtualPath()`, `IsCatalogBackedPath()`,
  `IsDocumentPath()`
- logical path resolution:
  `ResolveReadPath()`, `ResolveWritePath()`, `RefreshCatalogs()`
- logical file I/O:
  `Exists()`, `ReadText()`, `ReadBinary()`, `WriteText()`, `WriteBinary()`
- compatibility wrappers:
  `GetAssetPath()`, `GetSavedPath()`, `GetSavedConfigPath()`, `ResolveConfigPath()`,
  `ReadTextFile()`, `ReadBinaryFile()`

The module is currently split across small helpers such as:

- `PathTypes`
- `PathParser`
- `MountResolver`
- `ConfigResolver`
- `PhysicalIO`
- `RootDiscovery`

### 8.2 Remaining limitations

The main gap is no longer basic logical-path I/O, and it is no longer the absence of
any catalog support. The remaining architectural gap is the difference between the
current minimal per-mount catalog implementation and the long-term merged-catalog
design.

Today the implemented/remaining split is:

- extensionless read-domain asset paths such as `/Project/Textures/Grassy_Square`
  and `/Plugins/Foo/Materials/DebugGrid` now resolve through mount-local source
  catalogs in development builds
- duplicate logical paths within a single catalog are rejected
- artifact selection now prefers exact `profileTag` / `backendTag` / `platformTag`
  matches for the current runtime, then falls back to `any`
- the runtime now builds a merged global catalog table across current loose readable
  mounts (`/Project`, `/Engine`, `/Plugins/<Name>`)
- duplicate logical paths across currently loaded loose readable mounts are treated
  as invalid during merge
- explicit overlay precedence handling is still not implemented
- cooked catalog formats and packaged/archive-backed catalog loading are still not
  implemented
- document-style paths such as `/Project/Config/...`, `/Saved/...`, and `/Cache/...`
  intentionally continue to bypass catalog lookup and resolve directly through mounted
  filesystems

### 8.3 Public API shape

The exact names may change, but the resource system should evolve toward an API like:

```cpp
enum class PathDomain
{
    Project,
    Engine,
    Plugin,
    Saved,
    Cache,
};

struct VirtualPath
{
    PathDomain domain;
    std::optional<std::string> mountName;   // set only for Plugin domain
    std::string relativePath;
};

class FileSystem
{
public:
    static void Init();

    static bool IsVirtualPath(std::string_view path);
    static std::optional<VirtualPath> ParseVirtualPath(std::string_view path);
    static bool IsCatalogBackedPath(std::string_view path);
    static bool IsDocumentPath(std::string_view path);

    static std::optional<std::filesystem::path> ResolveReadPath(std::string_view virtualPath);
    static std::optional<std::filesystem::path> ResolveWritePath(std::string_view virtualPath);

    static bool Exists(std::string_view virtualPath);
    static std::optional<std::string> ReadText(std::string_view virtualPath);
    static std::optional<std::vector<uint8_t>> ReadBinary(std::string_view virtualPath);

    static bool WriteText(std::string_view virtualPath, std::string_view data);
    static bool WriteBinary(std::string_view virtualPath, std::span<const uint8_t> data);
};
```

Notes:

- `mountName` is populated only for `/Plugins/<Name>/...`
- most of this surface is now implemented in the current `FileSystem` facade
- the first implementation may still return `std::optional`, but the long-term API
  should likely move to a richer result type so resolution failures can distinguish
  between invalid path, unknown mount, denied write domain, and file-not-found
- asynchronous loading is intentionally out of scope for the first implementation, but
  future async APIs must build on the same logical path contract rather than inventing
  a second path system

### 8.4 Compatibility layer

During migration, keep compatibility helpers such as:

- `GetAssetPath()` implemented as a legacy wrapper to `/Project/...`
- `GetSavedPath()` implemented as a legacy wrapper to `/Saved/...`
- `ResolveConfigPath()` rewritten on top of the new logical config rules

That lets the codebase migrate incrementally without locking the new design to the old
API forever.

---

## 9. Resource Catalog

The resource catalog is a first-class subsystem of the path/resource layer.

Implementation status:

- a minimal source-catalog JSON loader is implemented
- a source catalog indexing tool (`rtr_asset_index`) is implemented for loose
  project, engine, and plugin content roots
- a minimal cooked catalog generation tool (`rtr_asset_cook`) is implemented for
  loose development-time cooked roots under `Saved/Cache/Cooked/`
- the current runtime scans project, engine, and plugin loose mounts and merges their
  mount-local `.rtr/catalog.json` files into a global development-time resolution
  table
- the current runtime can resolve catalog-backed project, engine, and plugin assets
  from that merged development-time table
- duplicate logical paths within a single catalog are treated as invalid
- duplicate logical paths across currently loaded loose readable mounts are also
  treated as invalid
- artifact selection now applies a basic runtime policy across `profileTag`,
  `backendTag`, and `platformTag`, preferring exact matches over `any`
- the current runtime can prefer loose cooked mounts for catalog-backed asset reads
  when the active resource profile is `cooked`
- catalog support is still incomplete: cooked catalogs still use the loose JSON
  schema as a placeholder, there is no packaged/archive resolution table, and there
  is no platform/backend/profile-aware packaged/overlay precedence policy yet

It is **not** an optional helper and it is **not** a temporary convenience for the
current cooking discussion. It is the stable bridge between:

- extensionless logical asset paths
- source-authoring files
- cooked runtime artifacts
- loose-file mounts
- packaged/archive mounts

### 9.1 Current source catalog workflow

Loose source catalogs are now generated by tooling rather than maintained by hand.

The `rtr_asset_index` tool scans:

- `Content/` as `/Project/`
- `EngineContent/` as `/Engine/`
- `Plugins/<Name>/Content/` as `/Plugins/<Name>/`

and writes `.rtr/catalog.json` under each discovered loose content root.

Current indexing rules:

- `.rtr/` is excluded from scanning
- `Config/` is excluded because config files remain document-style resource paths
- logical asset paths are generated without physical file extensions
- generated source artifacts currently use `profileTag = "dev"` and `platformTag`,
  `backendTag = "any"`
- duplicate generated logical paths within a mount are rejected as authoring errors

This gives the current development workflow a stable bridge between loose source files
and extensionless logical asset paths without requiring handwritten catalog entries.

### 9.2 Why RTRLab needs a catalog

RTRLab has chosen extensionless logical asset paths such as:

```text
/Project/Textures/Grassy_Square
/Project/Shaders/ForwardLit
```

That means the runtime cannot resolve those paths by guessing whether the physical file
is `.png`, `.jpg`, `.slang`, `.bin`, `.rtrtex`, or something else.

Therefore, catalog-backed asset paths must resolve through a **resource catalog**.

### 9.3 What goes through the catalog

The catalog is used for:

- imported/runtime assets whose logical path does not include the physical extension
- source-to-cooked remapping
- selecting the best physical artifact for the current platform/backend/profile
- mount-precedence-aware asset resolution

The catalog is **not** used for:

- document-style config paths such as `/Project/Config/...json`
- log files
- arbitrary user data under `/Saved/`

### 9.4 Catalog ownership and lifecycle

Each readable content mount owns a catalog:

- `/Project/` mount owns a project catalog
- `/Engine/` mount owns an engine catalog
- each `/Plugins/<Name>/` mount owns a plugin catalog
- overlay/archive/cooked mounts may ship alternate catalogs for the same logical root

At runtime, the resource system loads catalogs from all active readable mounts and
merges them into a global resolved view according to mount precedence. In the current
development implementation, this table is built on first use and can be rebuilt
explicitly through `RefreshCatalogs()` when readable mounts change after startup.

### 9.5 Catalog generation pipeline

RTRLab should adopt a two-stage catalog pipeline:

#### Source catalog generation

A dedicated indexing/import tool scans authoring-time content roots and generates
**source catalogs**.

Recommended ownership:

- project content: generated from `Content/`
- engine content: generated from `EngineContent/`
- plugin content: generated from `Plugins/<Name>/Content/`

Recommended tool naming:

```text
rtr_asset_index
```

Responsibilities:

- scan supported asset source files
- classify asset kind
- derive canonical logical paths
- detect conflicts
- emit a versioned source catalog

#### Cooked catalog generation

The cook pipeline consumes source catalogs and emits **cooked catalogs** alongside
cooked artifacts.

Recommended tool naming:

```text
rtr_asset_cook
```

Responsibilities:

- read source catalogs
- transform source files into runtime-ready artifacts
- record the produced artifact variants
- emit cooked catalogs for loose cooked directories and packaged archives

Current minimal implementation:

- `rtr_asset_cook` reads source `.rtr/catalog.json` files from loose content mounts
- it writes current cooked artifacts under `Saved/Cache/Cooked/Project`,
  `Saved/Cache/Cooked/Engine`, and `Saved/Cache/Cooked/Plugins/<Name>`
- it writes loose cooked `catalog.json` files that currently retain the same JSON
  schema as source catalogs, but rewrite artifact `profileTag` to `cooked`
- project texture assets currently switch from source `.jpg`/`.png` style artifacts
  to cooked `.rtrtex` artifact paths in the generated cooked catalog
- the current runtime can select those cooked loose mounts for catalog-backed reads
  when the active resource profile is `cooked`
- the current runtime can also load and validate the current bootstrap cooked
  texture payload format through a dedicated helper, so the texture cook flow now
  has a basic generation-and-consumption contract

This is intentionally a bootstrap implementation. It proves that the same logical
path can resolve from loose source content in `dev` and from loose cooked artifacts
in `cooked` without yet committing to the final binary cooked catalog format or a
final transcoding backend. The current texture cook step decodes source images to an
engine-owned RGBA8 bootstrap blob and writes that payload at cooked `.rtrtex`
artifact paths; the artifact identity is now distinct from the source image and no
longer pretends to be final KTX2 output.

### 9.6 Physical storage and format strategy

The design should support different physical encodings for developer readability and
shipping efficiency, while preserving a single canonical in-memory schema.

Recommended strategy:

- **source catalogs**: human-inspectable `JSON`
- **cooked catalogs**: versioned binary format optimized for startup/load speed

Recommended locations:

```text
Content/.rtr/catalog.json
EngineContent/.rtr/catalog.json
Plugins/Foo/Content/.rtr/catalog.json

Cache/Cooked/Project/.rtr/catalog.bin
Cache/Cooked/Engine/.rtr/catalog.bin
Cache/Cooked/Plugins/Foo/.rtr/catalog.bin
```

For packaged archives, the archive must contain the cooked catalog for the mounted root,
for example:

```text
data_project.pak -> contains /Project/ content + .rtr/catalog.bin
data_engine.pak  -> contains /Engine/ content  + .rtr/catalog.bin
```

The exact file names may change, but the architectural rule should remain:

- every readable catalog-backed mount carries a catalog artifact
- the runtime never reconstructs the catalog by guessing extensions

### 9.7 Canonical catalog schema

RTRLab should standardize an engine-owned schema similar to:

```cpp
enum class ResourceKind
{
    Texture,
    Shader,
    Material,
    Mesh,
    Scene,
    Audio,
    BinaryBlob,
};

struct ArtifactRecord
{
    std::string relativePath;      // physical path inside the mount/backend
    std::string format;            // e.g. jpg, png, slang, rtrtex, metallib, spirv
    std::string platformTag;       // e.g. any, windows, macos, linux
    std::string backendTag;        // e.g. any, opengl, metal, vulkan
    std::string profileTag;        // e.g. dev, cooked, shipping
    uint64_t contentHash = 0;
};

struct ResourceCatalogEntry
{
    std::string logicalPath;       // e.g. /Project/Textures/Grassy_Square
    ResourceKind kind;
    PathDomain domain;
    std::optional<std::string> mountName;   // plugin name when applicable
    std::string sourceRelativePath;         // authoring-time file within the mount
    uint64_t sourceHash = 0;
    std::vector<ArtifactRecord> artifacts;
};
```

Important design rules:

- `logicalPath` is the stable identity
- `sourceRelativePath` is descriptive metadata, not the public resource identity
- `artifacts` may contain both development and cooked variants
- artifact selection is runtime policy, not a serialized caller concern

### 9.8 Catalog loading and merge model

At startup or mount activation time:

1. load the catalog associated with each readable mount
2. validate catalog version/schema compatibility
3. merge entries into a global resolution table
4. apply mount precedence rules

Conflict rules:

- duplicate `logicalPath` entries within the same catalog are invalid
- duplicate `logicalPath` entries across equal-precedence mounts are invalid unless
  the mount type explicitly allows override semantics
- duplicate `logicalPath` entries across overlay layers are allowed only when the
  higher-priority mount is explicitly marked as an override layer
- plugin namespace entries do not collide with project namespace entries because
  `/Plugins/<Name>/...` is a distinct logical root

### 9.9 Runtime resolution algorithm

For a catalog-backed logical path:

1. parse and validate the virtual path
2. query the merged catalog by `logicalPath`
3. choose the best matching artifact for the active runtime profile
4. resolve that artifact into a physical file path or archive entry
5. hand the bytes/path to the higher-level asset loader

Artifact selection policy should consider:

- current build/profile (`dev`, `cooked`, `shipping`)
- active backend (`opengl`, `metal`, `vulkan`)
- platform tag
- mount precedence

### 9.10 Document-style paths versus catalog-backed paths

This document intentionally uses two path styles:

- catalog-backed asset paths: `/Project/Textures/Grassy_Square`
- document-style paths: `/Project/Config/Input/DebugCameraControl.json`

The distinction is deliberate.

Catalog-backed asset paths:

- omit physical source extensions
- resolve through the resource catalog
- are used for imported/runtime assets

Document-style paths:

- retain explicit filenames and extensions
- resolve directly through the mounted filesystem
- are used for config and other directly consumed documents

This means `/Project/Config/...` does **not** go through the extensionless asset catalog
lookup flow.

---

## 10. Mount Backends

The public path system must be independent from storage backend.

### 10.1 Initial backends

| Backend | Description | Typical use |
|--------|-------------|-------------|
| Directory mount | Reads from a physical folder tree | Development, tests, loose installs |
| User directory mount | Writable OS directory | `/Saved/`, `/Cache/` |

### 10.2 Future backends

| Backend | Description | Typical use |
|--------|-------------|-------------|
| Archive mount | Reads from zip/pak/container files | Shipping builds |
| Overlay mount | Higher-priority patch/mod layer | DLC, hotfix, local overrides |
| Cooked-output mount | Reads preprocessed runtime-ready assets | Console/mobile/release pipelines |

### 10.3 Mount precedence

For read domains, the system resolves from highest priority to lowest priority.

Example for `/Project/...`:

1. hotfix overlay
2. explicit project overlay mod
3. packaged cooked content
4. loose cooked output
5. loose source content in development

Plugins do **not** implicitly override `/Project/...` just because they are plugins.
Plugin content lives in its own namespace under `/Plugins/<Name>/...`.

If RTRLab later supports project-content overrides from mods or patches, that must be
implemented as an explicit overlay mount policy, not as a side effect of the plugin
namespace existing.

The caller still asks for the same path:

```text
/Project/Textures/Grassy_Square
```

---

## 11. Config System Integration

Config files are not generic content assets and should not remain buried under the
project content tree forever.

### 11.1 Recommended config model

- shipped project config defaults live under `/Project/Config/`
- shipped engine config defaults live under `/Engine/Config/`
- user overrides live under `/Saved/Config/`
- code reads config through a **resolved config namespace**

Config paths are **document-style resource paths**:

- they retain file extensions such as `.json`
- they resolve directly through mounted filesystems
- they do not participate in extensionless catalog-backed asset lookup

Examples:

```text
/Project/Config/Input/DebugCameraControl.json
/Engine/Config/Input/DefaultBindings.json
/Saved/Config/Input/DebugCameraControl.json
```

### 11.2 Resolution chain

For logical config key `Input/DebugCameraControl.json`:

1. `/Saved/Config/Input/DebugCameraControl.json`
2. project default: `/Project/Config/Input/DebugCameraControl.json`
3. engine default: `/Engine/Config/Input/DebugCameraControl.json`

### 11.3 Behavior

- reads prefer saved override
- missing saved override may be auto-seeded from defaults
- writes always go to `/Saved/Config/...`
- shipped defaults are never modified in place

This preserves the useful behavior the current `ResolveConfigPath()` already provides,
while moving it into a cleaner long-term structure.

---

## 12. Serialization Rules

Serialization must distinguish between:

- **asset references**
- **user-data file locations**

### 12.1 Asset references

When serialized data references a loadable engine resource, it should store a logical
resource path:

```json
{
  "albedo": "/Project/Textures/Grassy_Square"
}
```

Not:

```json
{
  "albedo": "Content/textures/Grassy_Square.jpg"
}
```

And never:

```json
{
  "albedo": "C:/Users/name/dev/RTRLab/Content/textures/Grassy_Square.jpg"
}
```

### 12.2 User data

Runtime-owned data such as logs or per-user config overrides should not be serialized as
general asset references. Those locations are system-managed.

### 12.3 Serialization API impact

The existing `Serialization::SaveToFile` / `LoadFromFile` APIs may keep taking
`std::filesystem::path` for low-level use, but higher-level systems should gain
resource-aware wrappers so application code can operate on logical paths directly.

For serialized asset references specifically, code should prefer a validated wrapper
type rather than a raw `std::string`, so malformed physical paths and non-asset logical
paths are rejected at deserialize time.

---

## 13. Cooking and Packaging

Cooking and packaging are downstream of the resource model.

### 13.1 Cooking

Cooking transforms authoring-time content into runtime-ready data.

Examples:

- shader source → compiled backend output
- PNG/JPG → GPU-ready texture container
- material graph source → flattened runtime description

RTRLab adopts the following rule:

- **logical resource paths do not encode physical source extensions**
- **logical resource paths do not change when content is cooked**
- source files and cooked outputs are both resolved through a manifest/catalog layer

Examples:

```text
Logical path:   /Project/Shaders/ForwardLit
Source file:    Content/Shaders/ForwardLit.slang
Cooked file:    Cache/Cooked/Shaders/ForwardLit.bin

Logical path:   /Project/Textures/Grassy_Square
Source file:    Content/Textures/Grassy_Square.jpg
Cooked file:    Cache/Cooked/Textures/Grassy_Square.rtrtex
```

Cooked outputs may live physically under `Cache/` or `build/` during development, but
must still resolve back to the same logical identity:

```text
/Project/Shaders/ForwardLit
/Project/Textures/Grassy_Square
```

This requires the resource catalog defined in Section 9.
The runtime must **not** guess file extensions by probing `.png`, `.jpg`, `.rtrtex`,
`.slang`, `.bin`, and similar variants at load time.

If two different source files would map to the same logical path, that is an authoring
error and must be rejected by tooling.

### 13.2 Packaging

Packaging should mount cooked archives into the same roots:

- `/Project/`
- `/Engine/`
- `/Plugins/...`

The packaging format is an implementation choice.

Possible formats:

- ZIP
- custom `.pak`
- multiple chunk archives

The public path model must remain unchanged regardless of format choice.

### 13.3 Recommendation

Do **not** choose the final path API based on today's shader build scripts.

Choose the path API first, then make cook/package tooling mount its outputs into that
API.

---

## 14. Migration Plan Summary

This section is a high-level summary of the same numbered phases expanded in the
checklist below.

### Phase 0 - Design freeze and naming decisions

- design direction approved
- long-term physical rename accepted:
  `assets/ -> Content/`, `saved/ -> Saved/`
- config defaults move immediately to mounted `Config/` subtrees

### Phase 1 - Core path primitives

- implemented

### Phase 2 - Read/write resolution layer

- implemented for `/Project/`, `/Engine/`, `/Plugins/<Name>/`, `/Saved/`, and `/Cache/`

### Phase 3 - Resource catalog design and implementation

- in progress; loose-mount source catalog loading, merged development-time
  resolution, duplicate-path rejection, basic runtime artifact selection,
  refreshable catalog state, source catalog generation tooling, and
  project/engine/plugin asset lookup are implemented

### Phase 4 - Public FileSystem API migration

- largely implemented in the `FileSystem` facade

### Phase 5 - Compatibility bridge for current repository layout

- historical bridge completed; repository now uses `Content/` / `Saved/` directly

### Phase 6 - Caller migration in runtime systems

- mostly implemented; runtime diagnostics, crash artifacts, ImGui ini, and config-path
  serialization helpers now resolve through logical resource paths

### Phase 7 - Config system cleanup

- largely implemented; project, engine, and saved config all use mounted `Config/`
  paths with engine-default fallback backing

### Phase 8 - Serialization and asset-reference rules

- in progress; logical-path serialization helpers exist, but asset-reference policy and
  leakage audit are still pending

### Phase 9 - Optional physical directory rename

- accepted and implemented for the active repository layout

### Phase 10 - Engine and plugin mounts

- largely implemented; `EngineContent/` backs `/Engine/`, plugin mounts are discovered
  from `Plugins/<Name>/Content/`, and plugin mount names now follow the documented
  identifier rules

### Phase 11 - Mount backend abstraction

- introduce backend abstraction when needed

### Phase 12 - Cooking integration

- in progress; loose cooked catalog generation, runtime selection between source
  and cooked project artifacts, and bootstrap cooked texture read/validation are
  now implemented for development-time mounts

### Phase 13 - Archive packaging

- add archive mount without changing public paths

### Phase 14 - Final cleanup

- remove migration-era assumptions and freeze the serialized contract

---

## 15. End-to-End Work Checklist

This section turns the design into an implementation checklist.

The intent is that the team can work top-to-bottom and finish the whole module
without repeatedly redefining scope.

### 15.1 Phase 0 - Design freeze and naming decisions

- [x] Approve the public logical roots:
      `/Project/`, `/Engine/`, `/Plugins/<Name>/`, `/Saved/`, `/Cache/`
- [x] Decide whether the long-term physical rename is accepted:
      `assets/ -> Content/`, `saved/ -> Saved/`
- [x] Decide whether config defaults move out of content immediately or in a later pass
- [x] Freeze the logical path syntax rules:
      leading `/`, forward slashes only, no `.` / `..`, no raw OS paths in serialized references
- [x] Freeze the path-class distinction:
      catalog-backed assets vs document-style resource paths
- [x] Identify all current systems that read or write paths directly:
      `FileSystem`, diagnostics, ImGui ini, serialization callsites, future asset references

### 15.2 Phase 1 - Core path primitives

- [x] Add a logical path parser to `FileSystem`
- [x] Add mount/domain representation in code
- [x] Implement virtual path validation
- [x] Implement normalization rules
- [x] Reject invalid mount roots and traversal segments
- [x] Implement path-class detection:
      catalog-backed asset path vs document-style path
- [x] Add unit tests for path parsing and normalization

### 15.3 Phase 2 - Read/write resolution layer

- [x] Implement `/Project/` read resolution
- [x] Implement `/Saved/` write resolution
- [x] Implement `/Cache/` write resolution
- [x] Add platform-specific shipping mappings for `/Saved/` and `/Cache/`
- [x] Add a clear policy for `/Engine/` even if it is initially backed by an empty directory
- [x] Add directory creation for writable domains
- [x] Add contract tests for read/write domain behavior

### 15.4 Phase 3 - Resource catalog design and implementation

- [ ] Finalize the canonical in-memory catalog schema
- [x] Finalize source catalog serialization format for current loose-mount
      development catalogs
- [ ] Finalize cooked catalog serialization format beyond the current temporary
      loose JSON bootstrap
- [x] Implement catalog versioning and compatibility checks
- [x] Implement catalog loader for readable mounts
- [x] Implement merged global resolution table
- [x] Implement conflict detection rules
- [x] Implement artifact selection policy by platform/backend/profile
- [x] Add tests for catalog lookup and duplicate-path rejection

### 15.5 Phase 4 - Public FileSystem API migration

- [x] Add `IsVirtualPath()`
- [x] Add `ParseVirtualPath()`
- [x] Add `IsCatalogBackedPath()`
- [x] Add `IsDocumentPath()`
- [x] Add `ResolveReadPath()`
- [x] Add `ResolveWritePath()`
- [x] Add logical-path-based `Exists()`
- [x] Add logical-path-based `ReadText()`
- [x] Add logical-path-based `ReadBinary()`
- [x] Add logical-path-based write helpers
- [x] Keep legacy wrappers for `GetAssetPath()` / `GetSavedPath()` temporarily
- [x] Mark legacy physical-path helpers as migration-only in comments/docs

### 15.6 Phase 5 - Compatibility bridge for current repository layout

- [x] Mount current `Content/` as `/Project/`
- [x] Mount current `Saved/` as `/Saved/`
- [x] Keep current startup root discovery working across the directory rename
- [x] Rewrite `ResolveConfigPath()` on top of the new logical config model
- [x] Confirm that existing tests still pass through compatibility wrappers

### 15.7 Phase 6 - Caller migration in runtime systems

- [x] Migrate ImGui ini handling to logical write-path resolution
- [x] Migrate diagnostics/log output to logical write-path resolution
- [x] Migrate crash dump/log-tail paths to logical write-path resolution
- [x] Migrate config loading to the new config namespace rules
- [x] Stop introducing new direct `std::filesystem::path` dependencies in gameplay-facing code
- [x] Audit current and future serialization callsites for physical-path leakage

### 15.8 Phase 7 - Config system cleanup

- [x] Create `/Project/Config/` physical backing layout
- [x] Create `/Engine/Config/` physical backing layout
- [x] Move shipped config defaults into mounted config subtrees
- [x] Preserve existing auto-seed behavior for first-run user config creation
- [x] Add tests for saved override precedence and default seeding

### 15.9 Phase 8 - Serialization and asset-reference rules

- [x] Define which data types are allowed to serialize logical resource paths
- [x] Update serialization guidance to prefer `/Project/...` or `/Engine/...`
- [x] Ban absolute filesystem paths in serialized asset references
- [x] Add validation/logging for malformed logical resource strings
- [x] Add at least one contract test that loads a serialized logical asset reference

### 15.10 Phase 9 - Optional physical directory rename

- [x] Rename `assets/` to `Content/` if the team accepts the change
- [x] Rename `saved/` to `Saved/` if the team accepts the change
- [x] Update CMake copy/install logic
- [x] Update `.gitignore`, docs, tests, and helper scripts
- [x] Keep a short-lived migration shim only if necessary

### 15.11 Phase 10 - Engine and plugin mounts

- [x] Add `/Engine/` loose-directory mount
- [x] Define the physical location for engine-shipped content
- [x] Add `/Plugins/<Name>/` mount registration model
- [x] Decide plugin mount naming rules
- [x] Add tests for plugin mount discovery and precedence

### 15.12 Phase 11 - Mount backend abstraction

- [ ] Introduce backend-agnostic mount interfaces if the simple resolver becomes too rigid
- [ ] Add loose directory mount implementation
- [ ] Add writable user-directory mount implementation
- [ ] Add overlay ordering policy
- [ ] Add directory enumeration only where truly needed

### 15.13 Phase 12 - Cooking integration

- [x] Implement source catalog generation tool
- [x] Implement cooked catalog generation tool
- [x] Define how cooked outputs map back into `/Project/` and `/Engine/` for the
      current loose development-time layout under `Saved/Cache/Cooked/`
- [x] Add runtime read/validation for the current bootstrap cooked texture format
- [ ] Decide whether cooking writes under `build/`, `Cache/`, or both
- [ ] Ensure cooked assets do not change public logical paths
- [x] Add tests that verify the same logical path can resolve from loose vs cooked backends

### 15.14 Phase 13 - Archive packaging

- [ ] Choose archive format: ZIP first or custom `.pak`
- [ ] Implement archive mount
- [ ] Mount packaged content into `/Project/` and `/Engine/`
- [ ] Ensure packaged mounts carry cooked catalogs
- [ ] Define patch/mod overlay precedence over packaged data
- [ ] Add contract tests for packaged-path parity with loose files

### 15.15 Phase 14 - Final cleanup

- [x] Remove legacy callsites that rely on raw `GetAssetPath()` / `GetSavedPath()`
- [ ] Remove temporary compatibility-only assumptions from docs
- [ ] Update onboarding/build documentation to describe logical paths first
- [ ] Confirm every asset-facing subsystem now speaks logical paths
- [ ] Freeze the resource path format as a long-term serialized contract

### 15.16 Definition of done

This module is done when all of the following are true:

- [ ] Runtime systems load shipped resources by logical path, not by ad hoc physical path joins
- [ ] Runtime systems write user data only through writable logical domains
- [x] Config defaults and user overrides follow the documented resolution chain
- [ ] Loose-file development and packaged/cooked builds expose the same public logical paths
- [ ] Serialized asset references no longer depend on repository-relative or absolute filesystem paths
- [x] `/Engine/` and `/Plugins/...` are no longer just future ideas in the design doc
- [ ] The remaining compatibility wrappers are either removed or intentionally retained with documented scope

---

## 16. Testing Requirements

This system needs contract tests, not just unit tests.

### 16.1 Required contracts

1. `/Project/...` resolves in development from loose project content.
2. `/Saved/...` resolves to a writable directory.
3. write attempts to `/Project/...` fail.
4. invalid mount roots are rejected.
5. `..` segments are rejected.
6. config resolution prefers `/Saved/Config/...` over shipped defaults.
7. the same logical path resolves correctly from loose and packaged mounts.
8. plugin mount precedence works as expected.
9. catalog-backed paths and document-style paths follow different resolution flows as documented.

### 16.2 Compatibility tests

While legacy wrappers still exist:

- `GetAssetPath("X")` must match `/Project/X`
- `GetSavedPath("X")` must match `/Saved/X`

---

## 17. Recommended Decision

RTRLab should **not** preserve `assets/` and `saved/` as the architectural center of the
resource system.

Instead:

- adopt Unreal-style logical mount points as the public model
- use `Content/` and `Saved/` as the primary physical development layout
- separate shipped config defaults from general content

This is the version that best supports future plugins, cooked assets, packaged builds,
and long-lived serialized asset references without dragging old path assumptions
forward forever.
