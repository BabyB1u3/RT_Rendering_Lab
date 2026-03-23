set(GLAB_CORE_SOURCES
    core/Application.cpp
    core/FileSystem.cpp
    core/Input.cpp
    core/InputAction.cpp
    core/Layer.cpp
    core/LayerStack.cpp
    core/Logger.cpp
    core/Time.cpp
    core/Window.cpp

    graphics/Material.cpp
    graphics/Mesh.cpp
    graphics/MeshFactory.cpp
    graphics/RenderCommand.cpp

    graphics/GraphicsDevice.cpp
    graphics/opengl/GLTexture2D.cpp
    graphics/opengl/GLVertexBuffer.cpp
    graphics/opengl/GLIndexBuffer.cpp
    graphics/opengl/GLVertexArray.cpp
    graphics/opengl/GLShader.cpp
    graphics/opengl/GLFramebuffer.cpp
    graphics/opengl/GLRenderTarget.cpp
    graphics/opengl/GLRenderCommand.cpp
    graphics/opengl/GLGraphicsDevice.cpp

    renderer/SceneRenderer.cpp

    renderer/passes/ForwardPass.cpp
    renderer/passes/ShadowPass.cpp
    renderer/passes/TexturePreviewPass.cpp

    scene/Camera.cpp
    scene/DebugCameraController.cpp

    demos/LabLayer.cpp
    demos/DemoRegistry.cpp
    demos/MaterialPlayground/MaterialPlayground.cpp
    demos/ShadowMapping/ShadowMapping.cpp

    gui/ImGuiLayer.cpp
    gui/Panels/DebugPanel.cpp
    gui/Panels/DemoSelectorPanel.cpp
)

set(GLAB_CORE_HEADERS
    core/Application.h
    core/Base.h
    core/FileSystem.h
    core/EventBus.h
    core/Events.h
    core/Input.h
    core/InputAction.h
    core/KeyCode.h
    core/ScopedConnection.h
    core/Layer.h
    core/LayerStack.h
    core/MouseCode.h
    core/Logger.h
    core/Time.h
    core/Window.h

    graphics/Buffers.h
    graphics/Framebuffer.h
    graphics/Material.h
    graphics/Mesh.h
    graphics/MeshFactory.h
    graphics/RenderCommand.h
    graphics/Texture.h

    graphics/GraphicsDevice.h
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
    demos/MaterialPlayground/MaterialPlayground.h
    demos/ShadowMapping/ShadowMapping.h

    gui/ImGuiLayer.h
    gui/Panels/DebugPanel.h
    gui/Panels/DemoSelectorPanel.h
)

add_library(RTRLabCore STATIC
    ${GLAB_CORE_SOURCES}
    ${GLAB_CORE_HEADERS}
)

set_source_files_properties(
    ${CMAKE_CURRENT_SOURCE_DIR}/core/Application.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/Input.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/Window.cpp
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

target_include_directories(RTRLabCore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(RTRLabCore PUBLIC
    glfw
    glad
    glm
    stb
    spdlog
    imgui
    OpenGL::GL
    spirv-cross-core
    spirv-cross-glsl
)

target_compile_definitions(RTRLabCore PUBLIC
    GLM_ENABLE_EXPERIMENTAL
    GLAB_ASSET_DIR="assets"
    GLAB_ROOT_DIR="${CMAKE_SOURCE_DIR}"
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
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/assets
            $<TARGET_FILE_DIR:RTRLab>/assets
)

set_target_properties(RTRLab PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES
    ${GLAB_CORE_SOURCES}
    ${GLAB_CORE_HEADERS}
    main.cpp
)
