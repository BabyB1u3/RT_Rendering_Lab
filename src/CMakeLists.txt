set(GLAB_CORE_SOURCES
    core/Application.cpp
    core/FileSystem.cpp
    core/Input.cpp
    core/Layer.cpp
    core/LayerStack.cpp
    core/Logger.cpp
    core/Time.cpp
    core/Window.cpp

    graphics/Buffers.cpp
    graphics/FrameBuffer.cpp
    graphics/Material.cpp
    graphics/Mesh.cpp
    graphics/MeshFactory.cpp
    graphics/RenderCommand.cpp
    graphics/RenderTarget.cpp
    graphics/Shader.cpp
    graphics/Texture.cpp
    graphics/VertexArray.cpp

    renderer/SceneRenderer.cpp

    renderer/passes/ForwardPass.cpp
    renderer/passes/ShadowPass.cpp
    renderer/passes/TexturePreviewPass.cpp

    scene/Camera.cpp
    scene/DebugCameraController.cpp

    demos/LabLayer.cpp
    demos/DemoRegistry.cpp
    demos/ShadowMapping/ShadowMapping.cpp

    gui/ImGuiLayer.cpp
    gui/Panels/DebugPanel.cpp
    gui/Panels/DemoSelectorPanel.cpp
)

set(GLAB_CORE_HEADERS
    core/Application.h
    core/Base.h
    core/FileSystem.h
    core/Input.h
    core/Layer.h
    core/LayerStack.h
    core/Logger.h
    core/Time.h
    core/Window.h

    graphics/Buffers.h
    graphics/FrameBuffer.h
    graphics/Material.h
    graphics/Mesh.h
    graphics/MeshFactory.h
    graphics/RenderCommand.h
    graphics/RenderTarget.h
    graphics/Shader.h
    graphics/Texture.h
    graphics/VertexArray.h

    renderer/RenderItem.h
    renderer/SceneRenderer.h

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
    demos/ShadowMapping/ShadowMapping.h

    gui/ImGuiLayer.h
    gui/Panels/DebugPanel.h
    gui/Panels/DemoSelectorPanel.h
)

add_library(RTRLabCore STATIC
    ${GLAB_CORE_SOURCES}
    ${GLAB_CORE_HEADERS}
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
    OpenGL::GL
)

target_compile_definitions(RTRLabCore PUBLIC
    GLM_ENABLE_EXPERIMENTAL
    GLAB_ASSET_DIR="assets"
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

add_executable(RTRLab
    main.cpp
)

target_link_libraries(RTRLab PRIVATE
    RTRLabCore
)

if(GLAB_ENABLE_WARNINGS)
    glab_set_warnings(RTRLabCore)
    glab_set_warnings(RTRLab)
endif()

if(GLAB_ENABLE_ASAN)
    glab_enable_sanitizers(RTRLabCore)
    glab_enable_sanitizers(RTRLab)
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