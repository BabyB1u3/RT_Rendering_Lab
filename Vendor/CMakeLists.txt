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
    set(GLAB_VULKAN_SDK_INCLUDE_DIR "${Vulkan_INCLUDE_DIR}")
    if(NOT GLAB_VULKAN_SDK_INCLUDE_DIR AND Vulkan_INCLUDE_DIRS)
        list(GET Vulkan_INCLUDE_DIRS 0 GLAB_VULKAN_SDK_INCLUDE_DIR)
    endif()

    set(GLAB_VOLK_HEADER "${GLAB_VULKAN_SDK_INCLUDE_DIR}/Volk/volk.h")
    set(GLAB_VOLK_SOURCE "${GLAB_VULKAN_SDK_INCLUDE_DIR}/Volk/volk.c")

    if(NOT EXISTS "${GLAB_VOLK_HEADER}" OR NOT EXISTS "${GLAB_VOLK_SOURCE}")
        message(FATAL_ERROR
            "The selected Vulkan SDK does not provide Volk. "
            "Install the Volk SDK component (com.lunarg.vulkan.volk)."
        )
    endif()

    add_library(volk STATIC
        ${GLAB_VOLK_SOURCE}
    )

    target_include_directories(volk PUBLIC
        ${GLAB_VULKAN_SDK_INCLUDE_DIR}
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
