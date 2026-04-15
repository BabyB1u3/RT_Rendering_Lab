# Naming Conventions

This document defines the repository-wide naming rules for engine code, tools, and tests. It is intended to become the source of truth for future `clang-tidy` naming checks.

The default rule is simple:

- Use `PascalCase` for nearly all project-defined identifiers.
- Use accessor prefixes `GetXxx`, `HasXxx`, and `IsXxx` for read-only queries.
- Use a single-letter prefix plus underscore plus `PascalCase` for private members and constants.

Examples:

- Type: `Application`
- Method: `RenderFrame`
- Read-only accessor: `GetWindow`, `HasContext`, `IsRunning`
- Public field: `Width`
- Local variable: `CurrentCount`
- Private member: `m_Window`
- Private static member: `s_Instance`
- Constant: `k_DefaultWidth`

## Scope

These rules apply to project-owned code under `Src/`, `Tests/`, repository tools, and future project-owned modules.

These rules do not force-renormalize:

- third-party identifiers
- platform SDK identifiers
- vendor API types and functions
- generated code

Examples of names that should keep their original spelling:

- `GLFWwindow`
- `ImGui`
- `CAMetalLayer`
- `MTLDevice`

## Core Rules

### 1. Types

Use `PascalCase` for:

- classes
- structs
- unions
- concepts
- type aliases
- enum types

Examples:

- `Application`
- `WindowProps`
- `InputActionMap`
- `TextureDesc`
- `TriggerState`

### 2. Functions And Methods

Use `PascalCase` for all project-defined free functions, member functions, and static member functions.

Examples:

- `RenderFrame`
- `InitializeRHI`
- `CreateCocoaNativeWindowHandle`
- `CaptureCallstack`

Do not introduce new lower-camel project APIs such as:

- `beginFrame`
- `createTexture`
- `getDesc`

If an API is project-owned and renamed during cleanup, prefer:

- `BeginFrame`
- `CreateTexture`
- `GetDesc`

### 3. Read-Only Accessors

Read-only accessors must use one of the following prefixes:

- `GetXxx`: returns a value, reference, pointer, handle, object, or computed result
- `HasXxx`: returns whether something exists, is available, or is owned
- `IsXxx`: returns whether something is currently in a state

Examples:

- `GetWindow`
- `GetCurrentCommandList`
- `HasContext`
- `HasAction`
- `IsRunning`
- `IsActionDown`

Avoid bare noun accessors such as:

- `Window()`
- `Context()`
- `Width()`
- `Height()`
- `Format()`

When a value is exposed through a project-owned accessor, prefer the prefixed form:

- `GetWidth`
- `GetHeight`
- `GetFormat`

### 4. Namespaces

Use `PascalCase` for project-defined namespaces.

Examples:

- `Diagnostics`
- `Resource`
- `TestSupport`

Do not introduce new snake_case namespaces for project-owned code.

Legacy namespaces may be migrated gradually, for example:

- `test_support` -> `TestSupport`

### 5. Enum Values

Use `PascalCase` for enum entries and enum-like constants.

Examples:

- `Load`
- `Clear`
- `DontCare`
- `RenderTarget`
- `CpuToGpu`

## Data Names

### 6. Public And Protected Data Members

Use `PascalCase` for public and protected fields.

Examples:

- `Name`
- `Width`
- `Height`
- `Priority`
- `ConsumesInput`

This rule especially applies to:

- plain-old-data structs
- settings/config structs
- descriptor structs
- lightweight runtime state structs when they expose fields directly

### 7. Private Data Members

Use `m_` + `PascalCase` for non-static private members.

Examples:

- `m_Window`
- `m_EventBus`
- `m_CommandList`
- `m_FrameRenderedThisTick`

Do not use:

- `window_`
- `_Window`
- `window`

### 8. Static Data Members

Use `s_` + `PascalCase` for static data members.

Examples:

- `s_Instance`
- `s_Initialized`
- `s_TotalTime`

### 9. Constants

Use `k_` + `PascalCase` for named constants.

This applies to:

- namespace-scope constants
- file-local constants
- static local constants
- class constants
- `constexpr` constants
- `constinit` constants

Examples:

- `k_DefaultWidth`
- `k_HexDigits`
- `kProjectContentDirName`
- `kMaxSupportedFrames`

Do not use:

- `DEFAULT_WIDTH`
- `DefaultWidth`
- `g_DefaultWidth`

## Variables And Parameters

### 10. Local Variables

Use `PascalCase` for local variables.

Examples:

- `CurrentCount`
- `ErrorMessage`
- `RenderPassDescriptor`
- `OutputPath`

### 11. Parameters

Use `PascalCase` for parameters.

Examples:

- `Width`
- `Height`
- `CommandList`
- `RelativePath`

Avoid parameter names such as:

- `width`
- `height`
- `commandList`
- `relativePath`

## Booleans

### 12. Boolean Naming

Boolean variables, members, and fields still follow the general case rules for their category, but their wording should read naturally as a predicate.

Examples:

- `IsRunning`
- `IsMinimized`
- `HasContext`
- `ConsumesInput`
- `m_FrameRenderedThisTick`

When the identifier is a query method, prefer `IsXxx` or `HasXxx`.

When the identifier is stored state, use a natural predicate phrase in `PascalCase`.

## Acronyms And Initialisms

### 13. Acronym Handling

Prefer normal `PascalCase` word boundaries for project-defined names, but established domain spellings may remain when they are already a stable part of the codebase or external API vocabulary.

Acceptable examples:

- `ImGuiLayer`
- `RHIDevice`
- `CreateCocoaNativeWindowHandle`

Avoid inventing inconsistent mixed forms for the same concept in different places.

For a single concept, choose one spelling and keep it everywhere.

## Migration Guidance

### 14. Legacy Names

Existing code currently contains a mix of:

- `PascalCase` APIs
- lower-camel APIs
- `snake_case` namespaces
- mixed public-field styles

New code must follow this document even before the full repository is migrated.

Repository cleanup should move toward this end state:

- project-owned methods become `PascalCase`
- read-only accessors become `GetXxx`, `HasXxx`, or `IsXxx`
- project-owned namespaces become `PascalCase`
- private members keep `m_`
- static data keeps `s_`
- constants use `k_`

## Quick Reference

| Category | Rule | Example |
| --- | --- | --- |
| Class / Struct / Enum type | `PascalCase` | `Application` |
| Namespace | `PascalCase` | `Diagnostics` |
| Free function / Method | `PascalCase` | `RenderFrame` |
| Read-only accessor | `GetXxx` / `HasXxx` / `IsXxx` | `GetWindow` |
| Public field | `PascalCase` | `Width` |
| Local variable | `PascalCase` | `CurrentCount` |
| Parameter | `PascalCase` | `RelativePath` |
| Private member | `m_` + `PascalCase` | `m_Window` |
| Static member | `s_` + `PascalCase` | `s_Instance` |
| Constant | `k_` + `PascalCase` | `k_DefaultWidth` |
| Enum value | `PascalCase` | `RenderTarget` |

## Notes For Future Clang-Tidy

When `clang-tidy` naming checks are introduced, this document should map directly to:

- type naming
- method/function naming
- namespace naming
- enum constant naming
- public member naming
- private member naming
- static member naming
- constant naming
- accessor prefix policy

The intent is to make the eventual automation enforce an already agreed style rather than invent one after the fact.
