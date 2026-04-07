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
/Project/Textures/Grassy_Square.jpg
/Project/Shaders/ForwardLit.slang
/Engine/Editor/Icons/Play.png
/Plugins/ExamplePlugin/Materials/Checker.json
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
/Project/Textures/Grassy_Square.jpg
/Engine/Shaders/Common/Fullscreen.slang
/Plugins/Foo/Content/Materials/DebugGrid.json
```

Properties:

- stable across development, install, and packaging
- valid in serialized data
- resolved through the resource mount table
- never directly concatenated with OS paths by gameplay code

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

That means `/Project/Textures/Grassy_Square.jpg` can come from:

- a loose development directory
- a cooked output directory
- a packaged archive
- a hotfix overlay

without changing any serialized references or gameplay code.

---

## 5. Proposed Physical Layout

This document recommends a deliberate directory rename away from the current
`assets/`-centric naming.

### 5.1 Recommended development layout

```text
RTRLab/
  Content/                    ← mounted as /Project/
    Textures/
    Shaders/
    Materials/
    Scenes/
  EngineContent/              ← mounted as /Engine/ (optional at first)
    Editor/
    Defaults/
  Config/
    Defaults/                 ← shipped text defaults, not generic runtime content
      Input/
      App/
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
  EngineContent/              ← /Engine/
  Config/
    Defaults/
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

The current repository still uses `assets/` and `saved/`.

That layout may be preserved temporarily during migration by mounting:

- `assets/` as `/Project/`
- `saved/` as `/Saved/`

However, the long-term recommendation is to rename them to `Content/` and `Saved/`
because those names communicate intent better and reduce future confusion between
"source assets", "runtime content", "cooked assets", and "editor resources".

---

## 6. Path Syntax Contract

### 6.1 Grammar

All logical paths use forward slashes:

```text
/MountName/Relative/Path.ext
```

Examples:

```text
/Project/Textures/Grassy_Square.jpg
/Project/Shaders/ForwardLit.slang
/Engine/Defaults/Materials/ErrorMaterial.json
/Plugins/Foo/Content/Textures/Icon.png
/Saved/Config/imgui.ini
```

### 6.2 Normalization rules

The parser must:

- require a leading `/`
- require a valid mount root
- collapse repeated `/`
- reject `.` and `..` segments
- reject backslashes in public logical paths
- preserve case in stored strings, but comparison policy may be platform-dependent
  internally if needed

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
3. resolving read paths through mounted backends
4. resolving write paths for writable domains
5. performing file I/O by logical path
6. enforcing read/write domain rules
7. exposing limited directory enumeration where needed

It is **not** responsible for:

- decoding textures, models, or shader binaries
- understanding scene/material semantics
- being the editor asset database

---

## 8. API Direction

The current `FileSystem` class is a useful bootstrap, but its public surface is too
physical-path-oriented for the long-term model.

### 8.1 Current limitation

Today the API is built around:

- `GetAssetPath(relative)`
- `GetSavedPath(relative)`
- `ReadTextFile(std::filesystem::path)`
- `ReadBinaryFile(std::filesystem::path)`

That makes the caller choose the physical storage class too early.

### 8.2 Target public API shape

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
    std::string mountName;   // plugin name when applicable
    std::string relativePath;
};

class FileSystem
{
public:
    static void Init();

    static bool IsVirtualPath(std::string_view path);
    static std::optional<VirtualPath> ParseVirtualPath(std::string_view path);

    static std::optional<std::filesystem::path> ResolveReadPath(std::string_view virtualPath);
    static std::optional<std::filesystem::path> ResolveWritePath(std::string_view virtualPath);

    static bool Exists(std::string_view virtualPath);
    static std::optional<std::string> ReadText(std::string_view virtualPath);
    static std::optional<std::vector<uint8_t>> ReadBinary(std::string_view virtualPath);

    static bool WriteText(std::string_view virtualPath, std::string_view data);
    static bool WriteBinary(std::string_view virtualPath, std::span<const uint8_t> data);
};
```

### 8.3 Compatibility layer

During migration, keep compatibility helpers such as:

- `GetAssetPath()` implemented as a legacy wrapper to `/Project/...`
- `GetSavedPath()` implemented as a legacy wrapper to `/Saved/...`
- `ResolveConfigPath()` rewritten on top of the new logical config rules

That lets the codebase migrate incrementally without locking the new design to the old
API forever.

---

## 9. Mount Backends

The public path system must be independent from storage backend.

### 9.1 Initial backends

| Backend | Description | Typical use |
|--------|-------------|-------------|
| Directory mount | Reads from a physical folder tree | Development, tests, loose installs |
| User directory mount | Writable OS directory | `/Saved/`, `/Cache/` |

### 9.2 Future backends

| Backend | Description | Typical use |
|--------|-------------|-------------|
| Archive mount | Reads from zip/pak/container files | Shipping builds |
| Overlay mount | Higher-priority patch/mod layer | DLC, hotfix, local overrides |
| Cooked-output mount | Reads preprocessed runtime-ready assets | Console/mobile/release pipelines |

### 9.3 Mount precedence

For read domains, the system resolves from highest priority to lowest priority.

Example for `/Project/...`:

1. hotfix overlay
2. mod/plugin override
3. packaged cooked content
4. loose cooked output
5. loose source content in development

The caller still asks for the same path:

```text
/Project/Textures/Grassy_Square.jpg
```

---

## 10. Config System Integration

Config files are not generic content assets and should not remain buried under the
project content tree forever.

### 10.1 Recommended config model

- shipped default config text lives under `Config/Defaults/`
- user overrides live under `/Saved/Config/`
- code reads config through a **resolved config namespace**

Examples:

```text
Config/Defaults/Input/DebugCameraControl.json
/Saved/Config/Input/DebugCameraControl.json
```

### 10.2 Resolution chain

For logical config key `Input/DebugCameraControl.json`:

1. `/Saved/Config/Input/DebugCameraControl.json`
2. project default: `Config/Defaults/Input/DebugCameraControl.json`
3. engine default: `EngineConfig/Defaults/Input/DebugCameraControl.json` or equivalent

### 10.3 Behavior

- reads prefer saved override
- missing saved override may be auto-seeded from defaults
- writes always go to `/Saved/Config/...`
- shipped defaults are never modified in place

This preserves the useful behavior the current `ResolveConfigPath()` already provides,
while moving it into a cleaner long-term structure.

---

## 11. Serialization Rules

Serialization must distinguish between:

- **asset references**
- **user-data file locations**

### 11.1 Asset references

When serialized data references a loadable engine resource, it should store a logical
resource path:

```json
{
  "albedo": "/Project/Textures/Grassy_Square.jpg"
}
```

Not:

```json
{
  "albedo": "assets/textures/Grassy_Square.jpg"
}
```

And never:

```json
{
  "albedo": "C:/Users/name/dev/RTRLab/assets/textures/Grassy_Square.jpg"
}
```

### 11.2 User data

Runtime-owned data such as logs or per-user config overrides should not be serialized as
general asset references. Those locations are system-managed.

### 11.3 Serialization API impact

The existing `Serialization::SaveToFile` / `LoadFromFile` APIs may keep taking
`std::filesystem::path` for low-level use, but higher-level systems should gain
resource-aware wrappers so application code can operate on logical paths directly.

---

## 12. Cooking and Packaging

Cooking and packaging are downstream of the resource model.

### 12.1 Cooking

Cooking transforms authoring-time content into runtime-ready data.

Examples:

- shader source → compiled backend output
- PNG/JPG → GPU-ready texture container
- material graph source → flattened runtime description

Cooked outputs may live physically under `Cache/` or `build/` during development, but
should still be mounted back into the same logical namespace:

```text
/Project/Shaders/ForwardLit.bin
/Project/Textures/Grassy_Square.ktx2
```

### 12.2 Packaging

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

### 12.3 Recommendation

Do **not** choose the final path API based on today's shader build scripts.

Choose the path API first, then make cook/package tooling mount its outputs into that
API.

---

## 13. Migration Plan

### Phase 0 - Vocabulary freeze

Adopt the logical roots in documentation and new code:

- `/Project/`
- `/Engine/`
- `/Plugins/...`
- `/Saved/`
- `/Cache/`

### Phase 1 - Virtual-path parser and resolver

Implement:

- logical path parsing
- read/write domain validation
- read resolution for `/Project/` and `/Saved/`
- compatibility wrappers over current `assets/` and `saved/`

### Phase 2 - Caller migration

Migrate current systems away from physical layout assumptions:

- ImGui ini location
- diagnostics logs
- config loading
- future scene/material resource references

### Phase 3 - Directory cleanup

Rename physical roots if desired:

- `assets/` → `Content/`
- `saved/` → `Saved/`

Move shipped config defaults out of content into `Config/Defaults/`.

### Phase 4 - Engine and plugin mounts

Add:

- `/Engine/`
- `/Plugins/<Name>/`

even if initially backed only by loose directories.

### Phase 5 - Cooking and archives

Add cooked-output mounts and archive mounts without changing public paths.

---

## 14. Testing Requirements

This system needs contract tests, not just unit tests.

### 14.1 Required contracts

1. `/Project/...` resolves in development from loose project content.
2. `/Saved/...` resolves to a writable directory.
3. write attempts to `/Project/...` fail.
4. invalid mount roots are rejected.
5. `..` segments are rejected.
6. config resolution prefers `/Saved/Config/...` over shipped defaults.
7. the same logical path resolves correctly from loose and packaged mounts.
8. plugin mount precedence works as expected.

### 14.2 Compatibility tests

While legacy wrappers still exist:

- `GetAssetPath("X")` must match `/Project/X`
- `GetSavedPath("X")` must match `/Saved/X`

---

## 15. Recommended Decision

RTRLab should **not** preserve `assets/` and `saved/` as the architectural center of the
resource system.

Instead:

- adopt Unreal-style logical mount points as the public model
- treat existing `assets/` / `saved/` directories as temporary migration-era physical
  backends
- strongly consider renaming them to `Content/` and `Saved/`
- separate shipped config defaults from general content

This is the version that best supports future plugins, cooked assets, packaged builds,
and long-lived serialized asset references without dragging old path assumptions
forward forever.
