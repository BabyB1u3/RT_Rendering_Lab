# ============================================================================
# SetupSlang.cmake - Download and configure Slang shader compiler
# ============================================================================
# Downloads the correct slangc release binary for the current platform.
# The binary is cached in the build directory and reused across rebuilds.
#
# After inclusion, the following variables are set:
#   SLANGC  - absolute path to the slangc executable
# ============================================================================

set(SLANG_VERSION "2026.5" CACHE STRING "Slang release version to download")

# ---------------------------------------------------------------------------
# Platform detection -> archive filename
# ---------------------------------------------------------------------------
if(WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(_SLANG_PLATFORM "windows-x86_64")
        set(_SLANG_EXT "zip")
    else()
        message(FATAL_ERROR "Slang: 32-bit Windows is not supported.")
    endif()
elseif(APPLE)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
        set(_SLANG_PLATFORM "macos-aarch64")
    else()
        set(_SLANG_PLATFORM "macos-x86_64")
    endif()
    set(_SLANG_EXT "tar.gz")
elseif(UNIX)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
        set(_SLANG_PLATFORM "linux-x86_64")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
        set(_SLANG_PLATFORM "linux-aarch64")
    else()
        message(FATAL_ERROR "Slang: unsupported Linux architecture: ${CMAKE_SYSTEM_PROCESSOR}")
    endif()
    set(_SLANG_EXT "tar.gz")
else()
    message(FATAL_ERROR "Slang: unsupported platform.")
endif()

set(_SLANG_ARCHIVE_NAME "slang-${SLANG_VERSION}-${_SLANG_PLATFORM}")
set(_SLANG_ARCHIVE_FILE "${_SLANG_ARCHIVE_NAME}.${_SLANG_EXT}")
set(_SLANG_DOWNLOAD_URL
    "https://github.com/shader-slang/slang/releases/download/v${SLANG_VERSION}/${_SLANG_ARCHIVE_FILE}")

# ---------------------------------------------------------------------------
# Download destination (inside build tree, not checked into repo)
# ---------------------------------------------------------------------------
set(_SLANG_DOWNLOAD_DIR "${CMAKE_BINARY_DIR}/_deps/slang")
set(_SLANG_ARCHIVE_PATH "${_SLANG_DOWNLOAD_DIR}/${_SLANG_ARCHIVE_FILE}")
# Slang archives extract flat (bin/, lib/, include/ directly under dest dir)
set(_SLANG_INSTALL_DIR  "${_SLANG_DOWNLOAD_DIR}")

# Resolve slangc path
if(WIN32)
    set(SLANGC "${_SLANG_INSTALL_DIR}/bin/slangc.exe")
else()
    set(SLANGC "${_SLANG_INSTALL_DIR}/bin/slangc")
endif()

# ---------------------------------------------------------------------------
# Download and extract (only if slangc doesn't already exist)
# ---------------------------------------------------------------------------
if(NOT EXISTS "${SLANGC}")
    message(STATUS "Slang: downloading v${SLANG_VERSION} for ${_SLANG_PLATFORM}...")
    message(STATUS "  URL: ${_SLANG_DOWNLOAD_URL}")

    file(DOWNLOAD
        "${_SLANG_DOWNLOAD_URL}"
        "${_SLANG_ARCHIVE_PATH}"
        STATUS _SLANG_DL_STATUS
        SHOW_PROGRESS
    )

    list(GET _SLANG_DL_STATUS 0 _SLANG_DL_CODE)
    list(GET _SLANG_DL_STATUS 1 _SLANG_DL_MSG)
    if(NOT _SLANG_DL_CODE EQUAL 0)
        file(REMOVE "${_SLANG_ARCHIVE_PATH}")
        message(FATAL_ERROR
            "Slang: download failed (${_SLANG_DL_CODE}): ${_SLANG_DL_MSG}\n"
            "  URL: ${_SLANG_DOWNLOAD_URL}\n"
            "  You can manually download from https://github.com/shader-slang/slang/releases\n"
            "  and extract to: ${_SLANG_INSTALL_DIR}")
    endif()

    message(STATUS "Slang: extracting to ${_SLANG_DOWNLOAD_DIR}...")
    file(ARCHIVE_EXTRACT
        INPUT "${_SLANG_ARCHIVE_PATH}"
        DESTINATION "${_SLANG_DOWNLOAD_DIR}"
    )

    # Make slangc executable on Unix
    if(NOT WIN32)
        file(CHMOD "${SLANGC}" PERMISSIONS
            OWNER_READ OWNER_WRITE OWNER_EXECUTE
            GROUP_READ GROUP_EXECUTE
            WORLD_READ WORLD_EXECUTE
        )
    endif()

    # Clean up archive to save disk space
    file(REMOVE "${_SLANG_ARCHIVE_PATH}")

    if(NOT EXISTS "${SLANGC}")
        message(FATAL_ERROR
            "Slang: slangc not found after extraction.\n"
            "  Expected at: ${SLANGC}\n"
            "  Archive extracted to: ${_SLANG_DOWNLOAD_DIR}\n"
            "  Check the archive structure matches the expected layout.")
    endif()
endif()

message(STATUS "Slang: using slangc at ${SLANGC}")

# Cache it so CompileShaders.cmake and other modules can find it
set(SLANGC "${SLANGC}" CACHE FILEPATH "Path to slangc compiler" FORCE)
