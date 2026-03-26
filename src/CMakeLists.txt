set(GLAB_CORE_SOURCES
    core/app/Application.cpp
    core/FileSystem.cpp
    core/input/Input.cpp
    core/input/InputAction.cpp
    core/input/InputNames.cpp
    core/app/Layer.cpp
    core/app/LayerStack.cpp
    core/Logger.cpp
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

    gui/ImGuiLayer.cpp
    gui/Panels/DebugPanel.cpp
    gui/Panels/DemoSelectorPanel.cpp
    gui/MetalImGuiBridge.h
)

set(GLAB_CORE_HEADERS
    core/app/Application.h
    core/Base.h
    core/FileSystem.h
    core/event/EventBus.h
    core/event/Events.h
    core/input/Input.h
    core/input/InputAction.h
    core/input/KeyCode.h
    core/event/ScopedConnection.h
    core/app/Layer.h
    core/app/LayerStack.h
    core/input/MouseCode.h
    core/Logger.h
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

    graphics/interface/ITexture2D.h
    graphics/interface/IVertexBuffer.h
    graphics/interface/IIndexBuffer.h
    graphics/interface/IVertexArray.h
    graphics/interface/IShader.h
    graphics/interface/IFramebuffer.h
    graphics/interface/IRenderTarget.h
    graphics/interface/IRenderCommand.h
    graphics/interface/IGraphicsDevice.h
    graphics/interface/GraphicsInterfaces.h

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

    gui/ImGuiLayer.h
    gui/Panels/DebugPanel.h
    gui/Panels/DemoSelectorPanel.h
)

if(GLAB_BACKEND_OPENGL)
    list(APPEND GLAB_CORE_SOURCES
        graphics/opengl/GLTexture2D.cpp
        graphics/opengl/GLVertexBuffer.cpp
        graphics/opengl/GLIndexBuffer.cpp
        graphics/opengl/GLVertexArray.cpp
        graphics/opengl/GLShader.cpp
        graphics/opengl/GLFramebuffer.cpp
        graphics/opengl/GLRenderTarget.cpp
        graphics/opengl/GLRenderCommand.cpp
        graphics/opengl/GLGraphicsDevice.cpp
    )

    list(APPEND GLAB_CORE_HEADERS
        graphics/opengl/GLCast.h
        graphics/opengl/GLTexture2D.h
        graphics/opengl/GLVertexBuffer.h
        graphics/opengl/GLIndexBuffer.h
        graphics/opengl/GLVertexArray.h
        graphics/opengl/GLShader.h
        graphics/opengl/GLFramebuffer.h
        graphics/opengl/GLRenderTarget.h
        graphics/opengl/GLRenderCommand.h
        graphics/opengl/GLGraphicsDevice.h
    )
endif()

if(GLAB_BACKEND_METAL)
    set(GLAB_METAL_SOURCES
        graphics/metal/MetalGraphicsDevice.mm
        graphics/metal/MetalRenderCommand.mm
        graphics/metal/MetalShader.mm
        graphics/metal/MetalVertexBuffer.mm
        graphics/metal/MetalIndexBuffer.mm
        graphics/metal/MetalVertexArray.mm
        graphics/metal/MetalTexture2D.mm
        graphics/metal/MetalFramebuffer.mm
        graphics/metal/MetalRenderTarget.mm
        gui/MetalImGuiBridge.mm
    )

    set(GLAB_METAL_HEADERS
        graphics/metal/MetalCast.h
        graphics/metal/MetalTypes.h
        graphics/metal/MetalGraphicsDevice.h
        graphics/metal/MetalRenderCommand.h
        graphics/metal/MetalShader.h
        graphics/metal/MetalVertexBuffer.h
        graphics/metal/MetalIndexBuffer.h
        graphics/metal/MetalVertexArray.h
        graphics/metal/MetalTexture2D.h
        graphics/metal/MetalFramebuffer.h
        graphics/metal/MetalRenderTarget.h
        gui/MetalImGuiBridge.h
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
        ${CMAKE_CURRENT_SOURCE_DIR}/graphics/opengl/GLFramebuffer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/graphics/opengl/GLGraphicsDevice.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/graphics/opengl/GLIndexBuffer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/graphics/opengl/GLRenderCommand.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/graphics/opengl/GLRenderTarget.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/graphics/opengl/GLShader.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/graphics/opengl/GLTexture2D.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/graphics/opengl/GLVertexArray.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/graphics/opengl/GLVertexBuffer.cpp
        PROPERTIES
            SKIP_UNITY_BUILD_INCLUSION ON
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
# Install rules — `cmake --install . --prefix <dir>` produces a self-contained
# deployable directory with no compile-time absolute paths required.
# ============================================================================
install(TARGETS RTRLab RUNTIME DESTINATION .)

# Source assets (textures, models, etc.), excluding .slang source files
install(DIRECTORY ${CMAKE_SOURCE_DIR}/assets/
        DESTINATION assets
        PATTERN "*.slang" EXCLUDE)

# Compiled shaders (Slang → GLSL / SPIR-V / MSL) merged under assets/
install(DIRECTORY ${CMAKE_BINARY_DIR}/shaders/
        DESTINATION assets/shaders/compiled)
