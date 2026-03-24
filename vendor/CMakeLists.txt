# ----------------------------
# GLFW
# ----------------------------
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
add_subdirectory(glfw)

target_compile_definitions(glfw INTERFACE
    GLFW_INCLUDE_NONE
)

# ----------------------------
# GLAD
# ----------------------------
add_library(glad STATIC
    glad/src/glad.c
)

target_include_directories(glad PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/glad/include
)

glab_enable_parallel_compilation(glad)

# ----------------------------
# GLM (header-only)
# ----------------------------
add_library(glm INTERFACE)
target_include_directories(glm INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/glm
)

# ----------------------------
# STB
# ----------------------------
add_library(stb STATIC
    stb/stb_image.cpp
)

target_include_directories(stb PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/stb
)

glab_enable_parallel_compilation(stb)

# ----------------------------
# spdlog (header-only)
# ----------------------------
add_library(spdlog INTERFACE)
target_include_directories(spdlog INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/spdlog/include
)
target_compile_options(spdlog INTERFACE
    $<$<CXX_COMPILER_ID:MSVC>:/utf-8>
)

# ----------------------------
# Dear ImGui
# ----------------------------
set(GLAB_IMGUI_SOURCES
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/backends/imgui_impl_glfw.cpp
    imgui/backends/imgui_impl_opengl3.cpp
)

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
    glad
)

glab_enable_parallel_compilation(imgui)
