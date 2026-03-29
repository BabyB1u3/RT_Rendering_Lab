# Resource Packaging & Virtual File System

This document describes how RTRLab discovers, organizes, and packages runtime
resources (assets, compiled shaders, configuration) and manages user-writable
data (settings, saves, caches) across the full lifecycle: development, testing,
installation, and future distribution.

---

## 1. Current State (Post-Slang Migration)

### 1.1 Build-time Flow

```
assets/shaders/*.slang          ─── slangc ───▶  build/shaders/glsl/*.glsl
assets/textures/*                                 (build artifact)
assets/models/*
```

### 1.2 POST_BUILD Copy

After linking `RTRLab`, CMake copies resources next to the executable:

```
bin/Debug/
  RTRLab.exe
  assets/
    shaders/
      *.slang                   ← source (copied from source tree)
      compiled/
        glsl/                   ← build artifacts (copied from build/)
          ForwardLit.vert.glsl
          ForwardLit.frag.glsl
          ...
    textures/
    models/
```

### 1.3 Runtime Resource Discovery (`FileSystem`)

`FileSystem::Init()` discovers the project root via a priority chain:

| Priority | Strategy                    | When it matches                                |
|----------|-----------------------------|------------------------------------------------|
| 1        | `RTRL_ROOT` env var         | CI, automated testing, custom setups           |
| 2        | Walk up from exe path       | Deployment / POST_BUILD output                 |
| 3        | `GLAB_ROOT_DIR` (compile)   | VS debugger (CWD = source root)                |
| 4        | Current working directory   | Last resort                                    |

Once the root is found, all paths are resolved relative to it:

- `GetAssetPath("textures/wood.png")` → `{root}/assets/textures/wood.png`
- `GetCompiledShaderDir()` → `{root}/assets/shaders/compiled/` (primary), falls back to
  `GLAB_SHADER_BUILD_DIR` during development before the first POST_BUILD copy runs.

### 1.4 `cmake --install`

```bash
cmake --install . --prefix dist/RTRLab
```

Produces a clean, self-contained directory:

```
dist/RTRLab/
  RTRLab.exe
  assets/
    shaders/
      compiled/glsl/            ← compiled shaders only, no .slang source
    textures/
    models/
```

No compile-time absolute paths are needed - `FindRootFromExecutable()` locates
`assets/` next to the exe.

---

## 2. Problems Remaining

| # | Problem | Impact |
|---|---------|--------|
| ~~P1~~ | ~~`GLAB_ROOT_DIR` leaks the build machine's absolute path into the binary~~ | By design: Debug-only, enables `saved/` in source tree. Release builds have no compile-time paths. |
| P2 | POST_BUILD copies the entire `assets/` directory every build, including `.slang` source | Slow on large asset sets; ships source files unnecessarily |
| P3 | No asset cooking / compression | Textures are raw PNG/JPG at runtime; no mip-chain precompute |
| P4 | No archive packaging | Cannot distribute as a single file; directory structure is exposed |
| P5 | No virtual file system | Every path is a physical filesystem path; can't overlay or hot-patch |

---

## 3. Phased Evolution Plan

### Phase A - Incremental Copy & Build Optimization

**Goal**: Make POST_BUILD efficient; clean up shipped artifacts.

**Note**: `GLAB_ROOT_DIR` and `GLAB_SHADER_BUILD_DIR` are intentionally kept as
Debug-only compile defines. They enable `saved/` to live in the source tree during
development (persists across clean builds) and provide a shader fallback path.
Since Debug binaries are never distributed, the embedded absolute paths are harmless.

**Changes**:

1. **Incremental POST_BUILD copy**. Replace `copy_directory` (which is unconditional)
   with a CMake script that uses `file(COPY ... PATTERN)` or a custom stamp-file approach
   to skip unchanged files. Alternatively, use `cmake -E copy_if_different` per file.

2. **Exclude `.slang` from POST_BUILD copy** (and install). Source shaders are
   development-only artifacts; only compiled output is needed at runtime.

### Phase B - Asset Cooking Pipeline

**Goal**: Pre-process assets at build time for optimal runtime loading.

```
assets/textures/wood.png  ─── cook ───▶  build/cooked/textures/wood.ktx2
assets/shaders/*.slang    ─── slangc ──▶  build/cooked/shaders/glsl/*.glsl
                                          build/cooked/shaders/spirv/*.spv
                                          build/cooked/shaders/metal/*.metal
```

**Key decisions**:

- **Texture format**: KTX2 (Khronos container) with basis/ASTC compression.
  GPU-ready; no runtime decompression needed on hardware that supports ASTC or BC.
- **Shader format**: per-backend directories under `cooked/shaders/`. The GLSL
  backend may keep plain text; SPIR-V and Metal store bytecode.
- **Cook manifest**: A JSON/binary manifest (`cooked/manifest.json`) listing every
  cooked asset, its source hash, and output paths. Enables incremental re-cooking.
- **CMake integration**: A `glab_cook_assets()` function that drives the pipeline,
  similar to `glab_compile_shaders()`.

**Runtime change**: `FileSystem::GetAssetPath()` resolves from the cooked directory
first, falling back to raw assets in development (so you can iterate without cooking).

### Phase C - Virtual File System (VFS)

**Goal**: Decouple resource access from physical filesystem layout.

```
 Application Code
       │
       ▼
┌─────────────┐      resolve("shaders/glsl/ForwardLit.vert.glsl")
│     VFS     │──────────────────────────────────────────────────────▶ data
│  (IFileSystem) │
└──────┬──────┘
       │ mounts (priority order)
       ├── OverlayMount("/mods/custom.zip")    ← hot-patch / modding
       ├── ArchiveMount("/data.pak")           ← shipped archive
       └── DirectoryMount("/assets/")          ← development loose files
```

**Interface**:

```cpp
class IFileSystem
{
public:
    virtual ~IFileSystem() = default;

    /// Read an entire file. Returns empty optional on not-found.
    virtual std::optional<std::vector<uint8_t>> ReadFile(std::string_view virtualPath) = 0;

    /// Check existence without reading.
    virtual bool Exists(std::string_view virtualPath) = 0;

    /// List entries in a virtual directory.
    virtual std::vector<std::string> List(std::string_view virtualDir) = 0;
};
```

**Mount types**:

| Mount | Description | Use case |
|-------|-------------|----------|
| `DirectoryMount` | Reads from a physical directory | Development (loose files) |
| `ArchiveMount` | Reads from a zip / custom .pak archive | Shipping (single-file distribution) |
| `OverlayMount` | Layered on top; first-match wins | Modding, hot-patching, DLC |

**Migration path**:

1. Wrap current `FileSystem` into `DirectoryMount`.
2. Change all callsites from `FileSystem::ReadTextFile(path)` → `VFS::ReadFile(virtualPath)`.
3. Add `ArchiveMount` when packaging support is needed.

### Phase D - Archive Packaging (`.pak`)

**Goal**: Ship all runtime resources as a single archive.

**Format options**:

| Format | Pros | Cons |
|--------|------|------|
| ZIP | Standard, tooling everywhere, can memory-map individual entries | No custom indexing; slightly slower random access |
| Custom `.pak` | Table-of-contents at file start, O(1) entry lookup, can be memory-mapped as a whole | Must build/maintain format + tooling |
| SQLite | Battle-tested, supports queries, transactions | Overkill; not optimized for streaming |

**Recommendation**: Start with **ZIP** via a lightweight library (miniz or libzip).
If profiling shows file-lookup overhead matters, migrate to a custom `.pak` with a
hash-indexed TOC.

**Build integration**:

```cmake
# After install, pack into a .pak
add_custom_command(TARGET package POST_BUILD
    COMMAND pak_tool --create
            --input  ${CMAKE_INSTALL_PREFIX}/assets
            --output ${CMAKE_INSTALL_PREFIX}/data.pak
)
```

**Install layout with `.pak`**:

```
dist/RTRLab/
  RTRLab.exe
  data.pak            ← all assets + compiled shaders
```

---

## 4. Saved Directory & Config Resolution

All runtime-writable files (user settings, saves, logs, caches) live under a
single **saved** directory, separate from the read-only install/assets directory.
This follows the standard AAA game architecture:

```
Install directory (read-only):  assets + executable + shipped defaults
User directory (writable):      user settings + saves + logs + caches
```

### 4.1 Saved Directory Location

| Context | Location | Rationale |
|---------|----------|-----------|
| Development (`GLAB_ROOT_DIR` defined) | `{source_root}/saved/` | Persists across clean builds |
| Release (Windows) | `%LOCALAPPDATA%/RTRLab/` | Standard per-user writable location |
| Release (macOS) | `~/Library/Application Support/RTRLab/` | Platform convention |
| Release (Linux) | `$XDG_DATA_HOME/RTRLab/` or `~/.local/share/RTRLab/` | XDG spec |

### 4.2 Saved Directory Structure

```
saved/
  configs/           ← user-editable settings (input bindings, imgui.ini, etc.)
  saves/             ← save data (future)
  logs/              ← runtime logs (future)
  cache/             ← caches (future)
```

### 4.3 Config Resolution Chain

Shipped default configs live in `assets/configs/` (read-only, will be packed
into `.pak` in Phase D). User-editable copies live in `saved/configs/`.

`FileSystem::ResolveConfigPath(relativePath)` resolves as follows:

1. **`saved/configs/{relativePath}`** exists → return it (user override)
2. **`assets/configs/{relativePath}`** exists → auto-copy to `saved/configs/`,
   return the copy (first-run initialization)
3. Neither exists → return empty path (caller falls back to hardcoded defaults)

**Write path**: All config writes go to `saved/configs/` via
`FileSystem::GetSavedConfigPath()`. The assets directory is never written to.

**Reset to defaults**: Delete the file (or entire directory) under
`saved/configs/`. Next launch auto-copies fresh defaults from assets.

### 4.4 FileSystem API

```cpp
class FileSystem
{
public:
    static void Init();

    // Read-only assets (install directory)
    static const std::filesystem::path &GetRootPath();
    static std::filesystem::path GetAssetPath(std::string_view relativePath);
    static std::filesystem::path GetCompiledShaderDir();

    // Saved (writable, per-user)
    static const std::filesystem::path &GetSavedDir();
    static std::filesystem::path GetSavedPath(std::string_view relativePath);
    static std::filesystem::path GetSavedConfigPath(std::string_view relativePath);

    // Config resolution (saved → assets, auto-copy on first access)
    static std::filesystem::path ResolveConfigPath(std::string_view relativePath);

    // File I/O
    static std::string ReadTextFile(const std::filesystem::path &path);
    static std::vector<uint8_t> ReadBinaryFile(const std::filesystem::path &path);
    static bool Exists(const std::filesystem::path &path);
};
```

---

## 5. Directory Layout Summary

### Development (source tree + build tree)

```
RT_Rendering_Lab/                     ← source root
  assets/
    configs/
      input/ShadowMapping.json        ← shipped default configs
      input/MaterialPlayground.json
    shaders/
      ForwardLit.slang                ← shader source (editable)
      ShadowDepth.slang
      TexturePreview.slang
      modules/
    textures/
    models/
  saved/                              ← runtime writable (gitignored)
    configs/
      input/ShadowMapping.json        ← user-editable copy (auto-created)
      imgui.ini
  build/                              ← CMake build tree
    shaders/glsl/                     ← slangc output
    bin/Debug/
      RTRLab.exe
      assets/                         ← POST_BUILD copy
        configs/input/                ← shipped defaults
        shaders/
          *.slang                     ← copied source (Phase A will exclude)
          compiled/glsl/              ← compiled shaders
        textures/
        models/
```

### Installed / Deployed

```
RTRLab/                               ← install prefix
  RTRLab.exe
  assets/
    configs/input/                    ← shipped defaults (read-only)
    shaders/
      compiled/
        glsl/                         ← Phase A
        spirv/                        ← Phase B (Vulkan backend)
        metal/                        ← Phase B (Metal backend)
    textures/                         ← Phase B: cooked (KTX2)
    models/
```

User configs at runtime: `%LOCALAPPDATA%/RTRLab/configs/`

### Shipped (Phase D)

```
RTRLab/
  RTRLab.exe
  data.pak                            ← everything in one archive (incl. default configs)
```

User configs at runtime: `%LOCALAPPDATA%/RTRLab/configs/`

---

## 6. Timeline Alignment

| Phase | Depends on | Aligns with roadmap |
|-------|------------|---------------------|
| A - Incremental copy, remove compile-time paths | Slang migration (done) | Current (R3+) |
| B - Asset cooking | Vulkan/Metal backends landing | R4 (Metal), R5 (Vulkan) |
| C - Virtual file system | Phase B (multiple asset formats) | R5–R6 |
| D - Archive packaging | Phase C (VFS abstraction in place) | R7+ (Distribution) |

---

## 7. Risk & Alternatives

| Risk | Mitigation |
|------|-----------|
| VFS abstraction adds indirection overhead | Profile; DirectoryMount is a thin wrapper with near-zero cost. Archive mount uses memory-mapping. |
| ZIP random-access too slow for many small shaders | Batch-read at startup into a cache; or migrate to custom `.pak` |
| Cook pipeline complexity | Start minimal (shaders only - already done). Add texture cooking only when texture count or format diversity demands it. |
| Breaking change for existing `FileSystem` callers | Phase C wraps `FileSystem` as a mount; existing callsites can migrate incrementally via a compatibility shim. |
