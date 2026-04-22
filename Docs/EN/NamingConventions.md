# Naming Convention

This document defines the naming convention for code and documentation in this repository.

## Scope

This convention applies to:

- `Src/`
- `Tests/`

This convention does not apply to:

- `Vendor/`
- `Archive/`
- third-party APIs
- generated code or generated assets

## General Rules

### Types

Use `PascalCase` for:

- classes
- structs
- enums
- enum values
- type aliases

Examples:

```cpp
class Application;
struct SwapchainDesc;
enum class BackendType
{
    Vulkan,
    Metal,
};
using ByteBuffer = std::vector<uint8_t>;
```

### Functions and Methods

Use `PascalCase`.

Examples:

```cpp
void Initialize();
void BeginFrame();
void SetPosition(const glm::vec3& position);
bool ShouldRenderFrame() const;
```

Use clear verb-led names for actions.

Preferred verbs:

- `Create`
- `Initialize`
- `Shutdown`
- `Begin`
- `End`
- `Load`
- `Save`
- `Resolve`
- `Register`
- `Reset`

Exception:

- STL-style container compatibility helpers may keep the standard lowercase names
  `begin`, `end`, `rbegin`, and `rend`

### Local Variables and Parameters

Use `lowerCamelCase`.

Examples:

```cpp
const auto currentTime = glfwGetTime();
uint32_t imageIndex = 0;
std::string errorMessage;
```

Function-local `const` values still use `lowerCamelCase` when they are ordinary read-only temporaries or intermediate results.

Examples:

```cpp
const auto logTail = ReadLogTail(path, 80);
const std::string reason = fmt::format("{}", message);
```

### Member Fields

Use `m_` followed by `PascalCase`.

Examples:

```cpp
Window* m_Window = nullptr;
bool m_Running = true;
uint32_t m_FrameIndex = 0;
```

### Static Storage (Non-Constant)

Use `s_` followed by `PascalCase`.

Use this style for:

- class static fields
- function-local `static` variables that hold mutable persistent state
  (including the SIOF-safe "Construct-On-First-Use" idiom)

Constants follow the `k_` rule below regardless of scope.

Examples:

```cpp
static Application* s_Instance;
inline static int s_LiveCount = 0;

std::vector<Entry>& Entries()
{
    static std::vector<Entry> s_Entries; // SIOF-safe class-level state
    return s_Entries;
}

void OnAttach()
{
    static std::string s_IniPath; // function-private persistent state
    // ...
}
```

### Namespace-Scope Globals

Use `g_` followed by `PascalCase`.

Examples:

```cpp
std::mutex g_LoggerMutex;
std::atomic<uint64_t> g_FrameNumber{0};
```

### Constants

Use `k_` followed by `PascalCase`.

Examples:

```cpp
constexpr size_t k_MaxEntries = 1024;
inline constexpr std::string_view k_PakArchiveExtension = ".rtrpak";
static constexpr const char* k_AppName = "RTRLab";
```

Use this style for:

- namespace-scope constants
- class static constants
- `constexpr` local constants
- local `static const` / `static constexpr` constants that act as named constants

Examples:

```cpp
constexpr uint32_t k_MaxSupportedFrames = 62;
static const auto k_StartTime = Clock::now();
```

## Naming by Category

### Namespaces

Use `PascalCase`.

Examples:

- `Resource`
- `Serialization`
- `Diagnostics`
- `TestSupport`

Do not mix `PascalCase` and `snake_case` namespaces within the same layer.

### Abstract Types and Interfaces

Do not use the `I` prefix.

Use:

```cpp
class InputTrigger;
class Device;
class CommandList;
```

Do not use:

```cpp
class IInputTrigger;
class IDevice;
class ICommandList;
```

### Enums and Flags

Use `PascalCase` for enum types and enumerators.

Examples:

```cpp
enum class TextureState
{
    Undefined,
    RenderTarget,
    ShaderRead,
};
```

Use singular type names for flag categories.

Examples:

- `BufferUsage`
- `TextureAspect`

### Booleans

Boolean names must read like predicates.

Use names beginning with:

- `is`
- `has`
- `can`
- `should`
- `was`
- `needs`

Examples:

```cpp
bool isRenderingActive() const;
bool hasProject = std::filesystem::exists(projectRoot);
bool shouldLogFailure = false;
bool wasConnected = device->IsConnected();
```

Member booleans keep the same meaning after the prefix:

```cpp
bool m_IsRendering = false;
bool m_Running = true;
bool m_FrameRenderedThisTick = false;
```

### Accessors

Use:

- `GetX` for general accessors
- `SetX` for mutators
- `IsX` or `HasX` for boolean queries

Examples:

```cpp
Window& GetWindow();
void SetAspectRatio(float aspectRatio);
bool IsConnected() const;
```

### Factory and Lifecycle Names

Use explicit lifecycle names.

Examples:

```cpp
Scope<Device> CreateDevice(BackendType backend);
void InitializeRHI();
void BeginFrame();
void EndFrame();
void PresentFrame();
```

## Test Naming

Test files use descriptive `PascalCase` names.

Examples:

- `TestCamera.cpp`
- `TestCookedCatalog.cpp`
- `RootDiscoveryTestSupport.h`

Test helper functions, types, locals, and namespaces follow the same rules as production code.

## Documentation Naming

Documentation uses `PascalCase` names.

Use:

- `NamingConvention.md`
- `Design.md`
- `Internals.md`
- `CodeReview.md`
- `Readme.md`
- `Roadmap.md`
- `Rhi.md`
- `RhiBackendVulkan.md`

## Review Rule

Every new repository-owned name must follow this document.

If a changed symbol, file, or document already violates this convention and the rename is local and safe, rename it as part of the same change.
