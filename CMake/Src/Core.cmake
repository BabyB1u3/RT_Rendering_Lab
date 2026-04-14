# -----------------------------------------------------------------------------
# Core library
# -----------------------------------------------------------------------------
add_library(RTRLabCore STATIC
    ${GLAB_CORE_SOURCES}
    ${GLAB_CORE_HEADERS}
)

target_include_directories(RTRLabCore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if(GLAB_CORE_SKIP_UNITY_SOURCES)
    list(TRANSFORM GLAB_CORE_SKIP_UNITY_SOURCES PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/")
    set_source_files_properties(
        ${GLAB_CORE_SKIP_UNITY_SOURCES}
        PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON
    )
endif()

if(APPLE AND GLAB_BACKEND_METAL)
    set_source_files_properties(
        "${CMAKE_CURRENT_SOURCE_DIR}/Render/RHI/Backends/Metal/MetalDevice.mm"
        PROPERTIES COMPILE_FLAGS "-fno-objc-arc"
    )
endif()

target_link_libraries(RTRLabCore PUBLIC
    glfw
    glm
    imgui
    magic_enum
    nlohmann_json
    spdlog
    stb
)

if(WIN32)
    target_link_libraries(RTRLabCore PUBLIC
        dbghelp
    )
endif()

if(UNIX AND NOT APPLE)
    target_link_libraries(RTRLabCore PUBLIC
        dl
    )
endif()

if(GLAB_BACKEND_OPENGL)
    target_link_libraries(RTRLabCore PUBLIC
        glad
        OpenGL::GL
    )

    target_compile_definitions(RTRLabCore PUBLIC
        GLAB_BACKEND_OPENGL
    )
endif()

if(APPLE)
    target_link_libraries(RTRLabCore PUBLIC
        "-framework AppKit"
        "-framework QuartzCore"
    )
endif()

if(GLAB_BACKEND_METAL)
    target_link_libraries(RTRLabCore PUBLIC
        "-framework Metal"
    )

    target_compile_definitions(RTRLabCore PUBLIC
        GLAB_BACKEND_METAL
    )
endif()

if(GLAB_BACKEND_VULKAN)
    target_link_libraries(RTRLabCore PUBLIC
        volk
    )

    target_compile_definitions(RTRLabCore PUBLIC
        GLAB_BACKEND_VULKAN
    )
endif()

if(UNIX AND NOT APPLE)
    if(GLFW_BUILD_WAYLAND)
        target_compile_definitions(RTRLabCore PRIVATE
            GLAB_GLFW_WAYLAND_NATIVE
        )
    endif()

    if(GLFW_BUILD_X11)
        target_compile_definitions(RTRLabCore PRIVATE
            GLAB_GLFW_X11_NATIVE
        )
    endif()
endif()

target_compile_definitions(RTRLabCore PUBLIC
    GLM_ENABLE_EXPERIMENTAL
    $<$<CONFIG:Debug>:RTRLAB_CONFIG_DEBUG>
    $<$<CONFIG:RelWithDebInfo>:RTRLAB_CONFIG_RELWITHDEBINFO>
    $<$<CONFIG:Release>:RTRLAB_CONFIG_RELEASE>
    $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:GLAB_ROOT_DIR="${CMAKE_SOURCE_DIR}">
    $<$<CONFIG:Debug>:RTRLAB_LOG_MIN_LEVEL=0>
    $<$<CONFIG:RelWithDebInfo>:RTRLAB_LOG_MIN_LEVEL=1>
    $<$<CONFIG:Release>:RTRLAB_LOG_MIN_LEVEL=2>
)

if(MSVC)
    target_compile_definitions(RTRLabCore PUBLIC
        _CRT_SECURE_NO_WARNINGS
        NOMINMAX
    )
    target_compile_options(RTRLabCore PUBLIC
        /wd4005  # macro redefinition (APIENTRY: glad vs windows.h, both identical)
    )
endif()

if(APPLE)
    target_compile_definitions(RTRLabCore PUBLIC
        GL_SILENCE_DEPRECATION
    )
endif()

glab_enable_local_pch(RTRLabCore)
glab_configure_local_target(RTRLabCore)
