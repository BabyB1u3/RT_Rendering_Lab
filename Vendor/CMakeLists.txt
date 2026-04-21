# -----------------------------------------------------------------------------
# Local helpers
# -----------------------------------------------------------------------------
function(glab_add_header_only_vendor target_name include_dir)
    add_library(${target_name} INTERFACE)
    target_include_directories(${target_name} INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/${include_dir}
    )
endfunction()

# -----------------------------------------------------------------------------
# GLFW
# -----------------------------------------------------------------------------
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

add_subdirectory(glfw)

target_compile_definitions(glfw INTERFACE
    GLFW_INCLUDE_NONE
)

# -----------------------------------------------------------------------------
# GLAD
# -----------------------------------------------------------------------------
add_library(glad STATIC
    glad/src/glad.c
)

target_include_directories(glad PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/glad/include
)

glab_enable_parallel_compilation(glad)

# -----------------------------------------------------------------------------
# Volk
# -----------------------------------------------------------------------------
if(GLAB_BACKEND_VULKAN)
    set(GLAB_VOLK_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/volk/volk.h")
    set(GLAB_VOLK_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/volk/volk.c")
    set(GLAB_VMA_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/vma/vk_mem_alloc.h")
    set(GLAB_VMA_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/vma/vma.cpp")

    if(NOT EXISTS "${GLAB_VOLK_HEADER}" OR NOT EXISTS "${GLAB_VOLK_SOURCE}")
        message(FATAL_ERROR
            "Vendored Volk sources are missing. Expected files:\n"
            "  ${GLAB_VOLK_HEADER}\n"
            "  ${GLAB_VOLK_SOURCE}"
        )
    endif()

    add_library(volk STATIC
        ${GLAB_VOLK_SOURCE}
    )

    set(GLAB_VULKAN_INCLUDE_HINTS)
    if(Vulkan_INCLUDE_DIR)
        list(APPEND GLAB_VULKAN_INCLUDE_HINTS "${Vulkan_INCLUDE_DIR}")
    endif()
    if(Vulkan_INCLUDE_DIRS)
        list(APPEND GLAB_VULKAN_INCLUDE_HINTS ${Vulkan_INCLUDE_DIRS})
    endif()
    if(DEFINED ENV{VULKAN_SDK} AND NOT "$ENV{VULKAN_SDK}" STREQUAL "")
        list(APPEND GLAB_VULKAN_INCLUDE_HINTS
            "$ENV{VULKAN_SDK}"
            "$ENV{VULKAN_SDK}/include"
            "$ENV{VULKAN_SDK}/Include"
        )
    endif()

    find_path(GLAB_VULKAN_INCLUDE_ROOT
        NAMES vulkan/vulkan.h
        HINTS ${GLAB_VULKAN_INCLUDE_HINTS}
        PATH_SUFFIXES "" include Include
    )

    if(NOT GLAB_VULKAN_INCLUDE_ROOT)
        message(FATAL_ERROR
            "Failed to locate the Vulkan header root required by vendored Volk. "
            "Could not find 'vulkan/vulkan.h' from hints: ${GLAB_VULKAN_INCLUDE_HINTS}"
        )
    endif()

    if(NOT EXISTS "${GLAB_VMA_HEADER}" OR NOT EXISTS "${GLAB_VMA_SOURCE}")
        message(FATAL_ERROR
            "Vendored VMA sources are missing. Expected files:\n"
            "  ${GLAB_VMA_HEADER}\n"
            "  ${GLAB_VMA_SOURCE}"
        )
    endif()

    target_include_directories(volk PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${GLAB_VULKAN_INCLUDE_ROOT}
    )

    if(WIN32)
        target_compile_definitions(volk PUBLIC
            VK_USE_PLATFORM_WIN32_KHR
        )
    elseif(UNIX AND NOT APPLE)
        if(GLFW_BUILD_X11)
            target_compile_definitions(volk PUBLIC
                VK_USE_PLATFORM_XLIB_KHR
            )
        endif()

        if(GLFW_BUILD_WAYLAND)
            target_compile_definitions(volk PUBLIC
                VK_USE_PLATFORM_WAYLAND_KHR
            )
        endif()
    endif()

    glab_enable_parallel_compilation(volk)

    add_library(vma STATIC
        ${GLAB_VMA_SOURCE}
    )

    target_include_directories(vma PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${GLAB_VULKAN_INCLUDE_ROOT}
    )

    target_compile_definitions(vma PRIVATE
        VMA_STATIC_VULKAN_FUNCTIONS=0
        VMA_DYNAMIC_VULKAN_FUNCTIONS=0
    )

    target_link_libraries(vma PUBLIC
        volk
    )

    glab_enable_parallel_compilation(vma)
endif()

# -----------------------------------------------------------------------------
# Header-only dependencies
# -----------------------------------------------------------------------------
glab_add_header_only_vendor(glm glm)
glab_add_header_only_vendor(nlohmann_json nlohmann)
glab_add_header_only_vendor(magic_enum magic_enum)
glab_add_header_only_vendor(spdlog spdlog/include)

target_compile_options(spdlog INTERFACE
    $<$<CXX_COMPILER_ID:MSVC>:/utf-8>
)

# -----------------------------------------------------------------------------
# STB
# -----------------------------------------------------------------------------
add_library(stb STATIC
    stb/stb_image.cpp
)

target_include_directories(stb PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/stb
)

glab_enable_parallel_compilation(stb)

# -----------------------------------------------------------------------------
# Dear ImGui
# -----------------------------------------------------------------------------
set(GLAB_IMGUI_SOURCES
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/backends/imgui_impl_glfw.cpp
)

if(GLAB_BACKEND_OPENGL)
    list(APPEND GLAB_IMGUI_SOURCES
        imgui/backends/imgui_impl_opengl3.cpp
    )
endif()

if(GLAB_BACKEND_METAL)
    list(APPEND GLAB_IMGUI_SOURCES
        imgui/backends/imgui_impl_metal.mm
    )
endif()

if(GLAB_BUILD_IMGUI_DEMO)
    list(APPEND GLAB_IMGUI_SOURCES
        imgui/imgui_demo.cpp
    )
endif()

add_library(imgui STATIC
    ${GLAB_IMGUI_SOURCES}
)

target_include_directories(imgui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends
)

target_link_libraries(imgui PRIVATE
    glfw
)

if(GLAB_BACKEND_OPENGL)
    target_link_libraries(imgui PRIVATE
        glad
    )
endif()

glab_enable_parallel_compilation(imgui)
