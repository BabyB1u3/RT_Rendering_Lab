cmake_minimum_required(VERSION 3.20)

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
if(APPLE)
    set(GLAB_BACKEND_METAL ON)
    set(GLAB_BACKEND_OPENGL OFF)
else()
    set(GLAB_BACKEND_METAL OFF)
    set(GLAB_BACKEND_OPENGL ON)
endif()

# -----------------------------------------------------------------------------
# Shared helper functions
# -----------------------------------------------------------------------------
function(glab_set_warnings target_name)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

function(glab_enable_sanitizers target_name)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /fsanitize=address)
    else()
        target_compile_options(${target_name} PRIVATE -fsanitize=address -fno-omit-frame-pointer)
        target_link_options(${target_name} PRIVATE -fsanitize=address)
    endif()
endfunction()

function(glab_enable_parallel_compilation target_name)
    if(MSVC AND GLAB_ENABLE_MSVC_MP)
        target_compile_options(${target_name} PRIVATE /MP)
    endif()
endfunction()

function(glab_enable_release_symbols target_name)
    if(NOT MSVC OR NOT GLAB_ENABLE_RELEASE_SYMBOLS)
        return()
    endif()

    target_compile_options(${target_name} PRIVATE
        $<$<CONFIG:Release>:/Zi>
    )

    get_target_property(target_type ${target_name} TYPE)
    if(target_type STREQUAL "EXECUTABLE" OR
       target_type STREQUAL "SHARED_LIBRARY" OR
       target_type STREQUAL "MODULE_LIBRARY")
        target_link_options(${target_name} PRIVATE
            $<$<CONFIG:Release>:/DEBUG>
        )
    endif()
endfunction()

function(glab_enable_build_speedups target_name)
    glab_enable_parallel_compilation(${target_name})

    if(GLAB_ENABLE_UNITY_BUILD)
        set_target_properties(${target_name} PROPERTIES
            UNITY_BUILD ON
            UNITY_BUILD_BATCH_SIZE 8
        )
    endif()
endfunction()

function(glab_configure_local_target target_name)
    glab_enable_release_symbols(${target_name})
    glab_enable_build_speedups(${target_name})

    if(GLAB_ENABLE_WARNINGS)
        glab_set_warnings(${target_name})
    endif()

    if(GLAB_ENABLE_ASAN)
        glab_enable_sanitizers(${target_name})
    endif()
endfunction()

if(GLAB_BACKEND_OPENGL)
    find_package(OpenGL REQUIRED)
endif()

# Optional shader pipeline integration.
# include(cmake/SetupSlang.cmake)
# include(cmake/CompileShaders.cmake)

add_subdirectory(Vendor)
# glab_compile_shaders()
add_subdirectory(Src)

# -----------------------------------------------------------------------------
# Tests
# -----------------------------------------------------------------------------
if(GLAB_BUILD_TESTS)
    include(CTest)
    include(FetchContent)

    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )

    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(googletest)

    add_subdirectory(Tests)
endif()
