# -----------------------------------------------------------------------------
# Shader toolchain bootstrap
# -----------------------------------------------------------------------------

option(RTRLAB_DOWNLOAD_SLANG_TOOLCHAIN "Download a fixed Slang toolchain for development builds" ON)
set(RTRLAB_SLANG_VERSION "2026.5" CACHE STRING "Slang release version used by RTRLab")

set(RTRLAB_SLANGC "")

if(RTRLAB_DOWNLOAD_SLANG_TOOLCHAIN)
    if(WIN32)
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            set(_RTRLAB_SLANG_PLATFORM "windows-x86_64")
            set(_RTRLAB_SLANG_EXT "zip")
        else()
            message(FATAL_ERROR "Slang: 32-bit Windows is not supported.")
        endif()
    elseif(APPLE)
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
            set(_RTRLAB_SLANG_PLATFORM "macos-aarch64")
        else()
            set(_RTRLAB_SLANG_PLATFORM "macos-x86_64")
        endif()
        set(_RTRLAB_SLANG_EXT "tar.gz")
    elseif(UNIX)
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
            set(_RTRLAB_SLANG_PLATFORM "linux-x86_64")
        elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
            set(_RTRLAB_SLANG_PLATFORM "linux-aarch64")
        else()
            message(FATAL_ERROR "Slang: unsupported Linux architecture: ${CMAKE_SYSTEM_PROCESSOR}")
        endif()
        set(_RTRLAB_SLANG_EXT "tar.gz")
    else()
        message(FATAL_ERROR "Slang: unsupported platform.")
    endif()

    set(_RTRLAB_SLANG_ARCHIVE_NAME "slang-${RTRLAB_SLANG_VERSION}-${_RTRLAB_SLANG_PLATFORM}")
    set(_RTRLAB_SLANG_ARCHIVE_FILE "${_RTRLAB_SLANG_ARCHIVE_NAME}.${_RTRLAB_SLANG_EXT}")
    set(_RTRLAB_SLANG_DOWNLOAD_URL
        "https://github.com/shader-slang/slang/releases/download/v${RTRLAB_SLANG_VERSION}/${_RTRLAB_SLANG_ARCHIVE_FILE}")

    set(_RTRLAB_SLANG_DOWNLOAD_DIR "${CMAKE_BINARY_DIR}/_deps/slang")
    set(_RTRLAB_SLANG_ARCHIVE_PATH "${_RTRLAB_SLANG_DOWNLOAD_DIR}/${_RTRLAB_SLANG_ARCHIVE_FILE}")
    set(_RTRLAB_SLANG_INSTALL_DIR "${_RTRLAB_SLANG_DOWNLOAD_DIR}")

    if(WIN32)
        set(RTRLAB_SLANGC "${_RTRLAB_SLANG_INSTALL_DIR}/bin/slangc.exe")
    else()
        set(RTRLAB_SLANGC "${_RTRLAB_SLANG_INSTALL_DIR}/bin/slangc")
    endif()

    if(NOT EXISTS "${RTRLAB_SLANGC}")
        message(STATUS "Slang: downloading v${RTRLAB_SLANG_VERSION} for ${_RTRLAB_SLANG_PLATFORM}...")
        file(MAKE_DIRECTORY "${_RTRLAB_SLANG_DOWNLOAD_DIR}")
        file(DOWNLOAD
            "${_RTRLAB_SLANG_DOWNLOAD_URL}"
            "${_RTRLAB_SLANG_ARCHIVE_PATH}"
            STATUS _RTRLAB_SLANG_DL_STATUS
            SHOW_PROGRESS
        )

        list(GET _RTRLAB_SLANG_DL_STATUS 0 _RTRLAB_SLANG_DL_CODE)
        list(GET _RTRLAB_SLANG_DL_STATUS 1 _RTRLAB_SLANG_DL_MSG)
        if(NOT _RTRLAB_SLANG_DL_CODE EQUAL 0)
            file(REMOVE "${_RTRLAB_SLANG_ARCHIVE_PATH}")
            message(FATAL_ERROR
                "Slang: download failed (${_RTRLAB_SLANG_DL_CODE}): ${_RTRLAB_SLANG_DL_MSG}\n"
                "  URL: ${_RTRLAB_SLANG_DOWNLOAD_URL}"
            )
        endif()

        message(STATUS "Slang: extracting to ${_RTRLAB_SLANG_DOWNLOAD_DIR}...")
        file(ARCHIVE_EXTRACT
            INPUT "${_RTRLAB_SLANG_ARCHIVE_PATH}"
            DESTINATION "${_RTRLAB_SLANG_DOWNLOAD_DIR}"
        )

        if(NOT WIN32)
            file(CHMOD "${RTRLAB_SLANGC}" PERMISSIONS
                OWNER_READ OWNER_WRITE OWNER_EXECUTE
                GROUP_READ GROUP_EXECUTE
                WORLD_READ WORLD_EXECUTE
            )
        endif()

        file(REMOVE "${_RTRLAB_SLANG_ARCHIVE_PATH}")
    endif()

    if(NOT EXISTS "${RTRLAB_SLANGC}")
        message(FATAL_ERROR "Slang: slangc not found after extraction at ${RTRLAB_SLANGC}")
    endif()

    message(STATUS "Slang: using development toolchain at ${RTRLAB_SLANGC}")
endif()

set(RTRLAB_SLANGC "${RTRLAB_SLANGC}" CACHE FILEPATH "Path to the Slang compiler used by development builds")
