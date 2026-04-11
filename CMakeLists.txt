cmake_minimum_required(VERSION 3.20)

include(${CMAKE_CURRENT_SOURCE_DIR}/CMake/ProjectOptions.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/CMake/TargetHelpers.cmake)

if(GLAB_BACKEND_OPENGL)
    find_package(OpenGL REQUIRED)
endif()

add_subdirectory(Vendor)
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
