# -----------------------------------------------------------------------------
# Project setup
# -----------------------------------------------------------------------------
set(GLAB_PROJECT_LANGUAGES C CXX)
if(APPLE)
    list(APPEND GLAB_PROJECT_LANGUAGES OBJC OBJCXX)
endif()

project(RTRLab
    VERSION 0.1.0
    DESCRIPTION "A C++ rendering playground"
    LANGUAGES ${GLAB_PROJECT_LANGUAGES}
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# -----------------------------------------------------------------------------
# Global output layout
# -----------------------------------------------------------------------------
set(GLAB_SUPPORTED_CONFIGURATIONS
    Debug
    RelWithDebInfo
    Release
)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

if(CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES "${GLAB_SUPPORTED_CONFIGURATIONS}" CACHE STRING
        "Semicolon-separated list of supported build configurations"
        FORCE
    )
else()
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS ${GLAB_SUPPORTED_CONFIGURATIONS})

    if(NOT CMAKE_BUILD_TYPE IN_LIST GLAB_SUPPORTED_CONFIGURATIONS)
        list(JOIN GLAB_SUPPORTED_CONFIGURATIONS ", " GLAB_SUPPORTED_CONFIGURATIONS_TEXT)
        message(FATAL_ERROR
            "Unsupported CMAKE_BUILD_TYPE='${CMAKE_BUILD_TYPE}'. "
            "Supported values: ${GLAB_SUPPORTED_CONFIGURATIONS_TEXT}"
        )
    endif()
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

foreach(OUTPUTCONFIG IN LISTS GLAB_SUPPORTED_CONFIGURATIONS)
    string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG_UPPER)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG_UPPER} ${CMAKE_BINARY_DIR}/bin/${OUTPUTCONFIG})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG_UPPER} ${CMAKE_BINARY_DIR}/lib/${OUTPUTCONFIG})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG_UPPER} ${CMAKE_BINARY_DIR}/lib/${OUTPUTCONFIG})
endforeach()

# -----------------------------------------------------------------------------
# Build options
# -----------------------------------------------------------------------------
option(GLAB_BUILD_TESTS "Build tests" ON)
option(GLAB_ENABLE_WARNINGS "Enable warnings" ON)
option(GLAB_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(GLAB_ENABLE_PCH "Enable precompiled headers for local C++ targets" ON)
option(GLAB_ENABLE_UNITY_BUILD "Enable unity builds for local C++ targets" OFF)
option(GLAB_ENABLE_MSVC_MP "Enable MSVC multi-processor compilation (/MP)" ON)
option(GLAB_ENABLE_RELEASE_SYMBOLS "Emit debug symbols for MSVC Release builds" OFF)
option(GLAB_BUILD_IMGUI_DEMO "Build Dear ImGui demo translation unit" OFF)

# -----------------------------------------------------------------------------
# Backend selection
# -----------------------------------------------------------------------------
set(GLAB_SUPPORTED_BACKENDS
    OpenGL
    Metal
    Vulkan
)

if(APPLE)
    set(GLAB_DEFAULT_BACKEND Metal)
else()
    set(GLAB_DEFAULT_BACKEND OpenGL)
endif()

set(GLAB_GRAPHICS_BACKEND "${GLAB_DEFAULT_BACKEND}" CACHE STRING
    "Graphics backend selection"
)
set_property(CACHE GLAB_GRAPHICS_BACKEND PROPERTY STRINGS ${GLAB_SUPPORTED_BACKENDS})

if(NOT GLAB_GRAPHICS_BACKEND IN_LIST GLAB_SUPPORTED_BACKENDS)
    list(JOIN GLAB_SUPPORTED_BACKENDS ", " GLAB_SUPPORTED_BACKENDS_TEXT)
    message(FATAL_ERROR
        "Unsupported GLAB_GRAPHICS_BACKEND='${GLAB_GRAPHICS_BACKEND}'. "
        "Supported values: ${GLAB_SUPPORTED_BACKENDS_TEXT}"
    )
endif()

set(GLAB_BACKEND_OPENGL OFF)
set(GLAB_BACKEND_METAL OFF)
set(GLAB_BACKEND_VULKAN OFF)

if(GLAB_GRAPHICS_BACKEND STREQUAL "OpenGL")
    set(GLAB_BACKEND_OPENGL ON)
elseif(GLAB_GRAPHICS_BACKEND STREQUAL "Metal")
    if(NOT APPLE)
        message(FATAL_ERROR "GLAB_GRAPHICS_BACKEND=Metal is only supported on Apple platforms")
    endif()
    set(GLAB_BACKEND_METAL ON)
elseif(GLAB_GRAPHICS_BACKEND STREQUAL "Vulkan")
    set(GLAB_BACKEND_VULKAN ON)
endif()
