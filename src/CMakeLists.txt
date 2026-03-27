set(GLAB_CORE_SOURCES
    core/app/Application.cpp
    core/FileSystem.cpp
    core/input/Input.cpp
    core/input/InputAction.cpp
    core/input/InputContextStack.cpp
    core/input/InputNames.cpp
    core/app/Layer.cpp
    core/app/LayerStack.cpp
    core/diagnostics/Assert.cpp
    core/diagnostics/CrashHandler.cpp
    core/diagnostics/Debugger.cpp
    core/diagnostics/FrameFormatter.cpp
    core/diagnostics/ImGuiConsoleSink.cpp
    core/diagnostics/JsonLineSink.cpp
    core/diagnostics/Logger.cpp
    core/serialization/PropertyTree.cpp
    core/serialization/JsonBackend.cpp
    core/Time.cpp
    core/app/Window.cpp

    graphics/Material.cpp
    graphics/Mesh.cpp
    graphics/MeshFactory.cpp
    graphics/RenderCommand.cpp

    graphics/GraphicsDevice.cpp

    renderer/SceneRenderer.cpp

    renderer/passes/ForwardPass.cpp
    renderer/passes/ShadowPass.cpp
    renderer/passes/TexturePreviewPass.cpp

    scene/Camera.cpp
    scene/DebugCameraController.cpp

    demos/LabLayer.cpp
    demos/DemoRegistry.cpp
    demos/showcase/MaterialPlayground/MaterialPlayground.cpp
    demos/showcase/ShadowMapping/ShadowMapping.cpp
    demos/tutorial/01_ClearScreen/ClearScreen.cpp
    demos/tutorial/02_Triangle/Triangle.cpp
    demos/tutorial/03_TexturedQuad/TexturedQuad.cpp
    demos/tutorial/04_TransformedCube/TransformedCube.cpp

    gui/ImGuiLayer.cpp
    gui/panels/ConsolePanel.cpp
    gui/panels/DebugPanel.cpp
    gui/panels/DemoSelectorPanel.cpp
    gui/backends/metal/MetalImGuiBridge.h
)

set(GLAB_CORE_HEADERS
    core/app/Application.h
    core/Base.h
    core/FileSystem.h
    core/event/EventBus.h
    core/event/Events.h
    core/input/Input.h
    core/input/InputAction.h
    core/input/InputContextStack.h
    core/input/InputModifier.h
    core/input/InputTrigger.h
    core/input/KeyCode.h
    core/event/ScopedConnection.h
    core/app/Layer.h
    core/app/LayerStack.h
    core/input/MouseCode.h
    core/diagnostics/Assert.h
    core/diagnostics/Callstack.h
    core/diagnostics/CrashHandler.h
    core/diagnostics/Debugger.h
    core/diagnostics/ErrorMacros.h
    core/diagnostics/LogCategories.h
    core/diagnostics/FrameFormatter.h
    core/diagnostics/ImGuiConsoleSink.h
    core/diagnostics/JsonLineSink.h
    core/diagnostics/Logger.h
    core/diagnostics/LogMacros.h
    core/serialization/PropertyTree.h
    core/serialization/SerializationTraits.h
    core/serialization/BuiltinTraits.h
    core/serialization/IFormatBackend.h
    core/serialization/JsonBackend.h
    core/serialization/Serialization.h
    core/input/InputActionSerialization.h
    core/Time.h
    core/app/Window.h

    graphics/Buffers.h
    graphics/Framebuffer.h
    graphics/Material.h
    graphics/Mesh.h
    graphics/MeshFactory.h
    graphics/RenderCommand.h
    graphics/Texture.h

    graphics/GraphicsDevice.h

    graphics/interfaces/ITexture2D.h
    graphics/interfaces/IVertexBuffer.h
    graphics/interfaces/IIndexBuffer.h
    graphics/interfaces/IVertexArray.h
    graphics/interfaces/IShader.h
    graphics/interfaces/IFramebuffer.h
    graphics/interfaces/IRenderTarget.h
    graphics/interfaces/IRenderCommand.h
    graphics/interfaces/IGraphicsDevice.h
    graphics/interfaces/GraphicsInterfaces.h

    renderer/RenderContext.h
    renderer/RenderItem.h
    renderer/SceneRenderer.h
    renderer/SceneRendererTypes.h

    renderer/passes/ForwardPass.h
    renderer/passes/RenderPass.h
    renderer/passes/ShadowPass.h
    renderer/passes/TexturePreviewPass.h

    scene/Camera.h
    scene/DebugCameraController.h
    scene/Light.h
    scene/SceneData.h
    scene/Transform.h

    demos/DemoBase.h
    demos/DemoRegistry.h
    demos/LabLayer.h
    demos/showcase/MaterialPlayground/MaterialPlayground.h
    demos/showcase/ShadowMapping/ShadowMapping.h
    demos/tutorial/01_ClearScreen/ClearScreen.h
    demos/tutorial/02_Triangle/Triangle.h
    demos/tutorial/03_TexturedQuad/TexturedQuad.h
    demos/tutorial/04_TransformedCube/TransformedCube.h

    gui/ImGuiLayer.h
    gui/panels/ConsolePanel.h
    gui/panels/DebugPanel.h
    gui/panels/DemoSelectorPanel.h
)

if(GLAB_BACKEND_OPENGL)
    list(APPEND GLAB_CORE_SOURCES
        graphics/backends/opengl/GLTexture2D.cpp
        graphics/backends/opengl/GLVertexBuffer.cpp
        graphics/backends/opengl/GLIndexBuffer.cpp
        graphics/backends/opengl/GLVertexArray.cpp
        graphics/backends/opengl/GLShader.cpp
        graphics/backends/opengl/GLFramebuffer.cpp
        graphics/backends/opengl/GLRenderTarget.cpp
        graphics/backends/opengl/GLRenderCommand.cpp
        graphics/backends/opengl/GLGraphicsDevice.cpp
    )

    list(APPEND GLAB_CORE_HEADERS
        graphics/backends/opengl/GLCast.h
        graphics/backends/opengl/GLTexture2D.h
        graphics/backends/opengl/GLVertexBuffer.h
        graphics/backends/opengl/GLIndexBuffer.h
        graphics/backends/opengl/GLVertexArray.h
        graphics/backends/opengl/GLShader.h
        graphics/backends/opengl/GLFramebuffer.h
        graphics/backends/opengl/GLRenderTarget.h
        graphics/backends/opengl/GLRenderCommand.h
        graphics/backends/opengl/GLGraphicsDevice.h
    )
endif()

if(WIN32)
    list(APPEND GLAB_CORE_SOURCES
        core/diagnostics/backends/win32/CrashHandler.cpp
        core/diagnostics/backends/win32/Callstack.cpp
    )
else()
    list(APPEND GLAB_CORE_SOURCES
        core/diagnostics/backends/posix/CrashHandler.cpp
        core/diagnostics/backends/posix/Callstack.cpp
    )
endif()

if(GLAB_BACKEND_METAL)
    set(GLAB_METAL_SOURCES
        graphics/backends/metal/MetalGraphicsDevice.mm
        graphics/backends/metal/MetalRenderCommand.mm
        graphics/backends/metal/MetalShader.mm
        graphics/backends/metal/MetalVertexBuffer.mm
        graphics/backends/metal/MetalIndexBuffer.mm
        graphics/backends/metal/MetalVertexArray.mm
        graphics/backends/metal/MetalTexture2D.mm
        graphics/backends/metal/MetalFramebuffer.mm
        graphics/backends/metal/MetalRenderTarget.mm
        gui/backends/metal/MetalImGuiBridge.mm
    )

    set(GLAB_METAL_HEADERS
        graphics/backends/metal/MetalCast.h
        graphics/backends/metal/MetalTypes.h
        graphics/backends/metal/MetalGraphicsDevice.h
        graphics/backends/metal/MetalRenderCommand.h
        graphics/backends/metal/MetalShader.h
        graphics/backends/metal/MetalVertexBuffer.h
        graphics/backends/metal/MetalIndexBuffer.h
        graphics/backends/metal/MetalVertexArray.h
        graphics/backends/metal/MetalTexture2D.h
        graphics/backends/metal/MetalFramebuffer.h
        graphics/backends/metal/MetalRenderTarget.h
        gui/backends/metal/MetalImGuiBridge.h
    )

    set_source_files_properties(${GLAB_METAL_SOURCES}
        PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON
    )
endif()

add_library(RTRLabCore STATIC
    ${GLAB_CORE_SOURCES}
    ${GLAB_CORE_HEADERS}
)

if(GLAB_BACKEND_OPENGL)
    set_source_files_properties(
        ${CMAKE_CURRENT_SOURCE_DIR}/core/app/Application.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/core/input/Input.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/core/app/Window.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/graphics/backends/opengl/GLFramebuffer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/graphics/backends/opengl/GLGraphicsDevice.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/graphics/backends/opengl/GLIndexBuffer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/graphics/backends/opengl/GLRenderCommand.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/graphics/backends/opengl/GLRenderTarget.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/graphics/backends/opengl/GLShader.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/graphics/backends/opengl/GLTexture2D.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/graphics/backends/opengl/GLVertexArray.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/graphics/backends/opengl/GLVertexBuffer.cpp
        PROPERTIES
            SKIP_UNITY_BUILD_INCLUSION ON
    )
endif()

if(WIN32)
    target_link_libraries(RTRLabCore PUBLIC
        dbghelp
    )
endif()

target_include_directories(RTRLabCore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(RTRLabCore PUBLIC
    glfw
    glm
    stb
    spdlog
    imgui
    nlohmann_json
    magic_enum
)

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
endif()

if(GLAB_BACKEND_METAL)
    target_sources(RTRLabCore PRIVATE
        ${GLAB_METAL_SOURCES}
        ${GLAB_METAL_HEADERS}
    )

    target_link_libraries(RTRLabCore PUBLIC
        "-framework Metal"
        "-framework QuartzCore"
        "-framework AppKit"
    )

    target_compile_definitions(RTRLabCore PUBLIC
        GLAB_BACKEND_METAL
    )
endif()

target_compile_definitions(RTRLabCore PUBLIC
    GLM_ENABLE_EXPERIMENTAL
    GLAB_ASSET_DIR="assets"
    $<$<CONFIG:Debug>:GLAB_ROOT_DIR="${CMAKE_SOURCE_DIR}">
    $<$<CONFIG:Debug>:GLAB_SHADER_BUILD_DIR="${CMAKE_BINARY_DIR}/shaders">
    $<$<CONFIG:Debug>:RTRLAB_LOG_MIN_LEVEL=0>
    $<$<CONFIG:Release>:RTRLAB_LOG_MIN_LEVEL=2>
    $<$<CONFIG:RelWithDebInfo>:RTRLAB_LOG_MIN_LEVEL=1>
)

if(GLAB_ENABLE_PCH)
    target_precompile_headers(RTRLabCore PRIVATE
        "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/pch.h>"
    )
endif()

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

add_executable(RTRLab
    main.cpp
)

target_link_libraries(RTRLab PRIVATE
    RTRLabCore
)

if(GLAB_ENABLE_PCH)
    target_precompile_headers(RTRLab REUSE_FROM RTRLabCore)
endif()

glab_enable_build_speedups(RTRLabCore)
glab_enable_build_speedups(RTRLab)

if(GLAB_ENABLE_WARNINGS)
    glab_set_warnings(RTRLabCore)
    glab_set_warnings(RTRLab)
endif()

if(GLAB_ENABLE_ASAN)
    glab_enable_sanitizers(RTRLabCore)
    glab_enable_sanitizers(RTRLab)
endif()

if(TARGET CompileShaders)
    add_dependencies(RTRLab CompileShaders)
endif()

add_custom_command(TARGET RTRLab POST_BUILD
    # Copy source assets (textures, models, etc.)
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/assets
            $<TARGET_FILE_DIR:RTRLab>/assets
    # Merge compiled shaders into assets/shaders/compiled/
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_BINARY_DIR}/shaders
            $<TARGET_FILE_DIR:RTRLab>/assets/shaders/compiled
)

set_target_properties(RTRLab PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES
    ${GLAB_CORE_SOURCES}
    ${GLAB_CORE_HEADERS}
    ${GLAB_METAL_SOURCES}
    ${GLAB_METAL_HEADERS}
    main.cpp
)

# ============================================================================
# Install rules - `cmake --install . --prefix <dir>` produces a self-contained
# deployable directory with no compile-time absolute paths required.
# ============================================================================
install(TARGETS RTRLab RUNTIME DESTINATION .)

# Source assets (textures, models, etc.), excluding .slang source files
install(DIRECTORY ${CMAKE_SOURCE_DIR}/assets/
        DESTINATION assets
        PATTERN "*.slang" EXCLUDE)

# Compiled shaders (Slang -> GLSL / SPIR-V / MSL) merged under assets/
install(DIRECTORY ${CMAKE_BINARY_DIR}/shaders/
        DESTINATION assets/shaders/compiled)
