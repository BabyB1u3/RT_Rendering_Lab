set(GLAB_CORE_SOURCES
    core/Application.cpp
    core/Input.cpp
    core/Time.cpp
    core/Window.cpp

    graphics/Buffers.cpp
    graphics/Material.cpp
    graphics/Mesh.cpp
    graphics/MeshFactory.cpp
    graphics/RenderCommand.cpp
    graphics/RendeTarget.cpp
    graphics/Shader.cpp
    graphics/SubTexture.cpp
    graphics/Texture.cpp
    graphics/VertexArray.cpp

    renderer/SceneRenderer.cpp

    renderer/passes/ForwardPass.cpp
    renderer/passes/RenderPass.cpp
    renderer/passes/ShadowPass.cpp
    renderer/passes/TexturePreviewPass.cpp

    scene/Camera.cpp
    scene/DebugCameraController.cpp

    demos/ShadowMappingLab.cpp
)

set(GLAB_CORE_HEADERS
    core/Application.h
    core/Input.h
    core/Time.h
    core/Window.h

    graphics/Buffers.h
    graphics/Material.h
    graphics/Mesh.h
    graphics/MeshFactory.h
    graphics/RenderCommand.h
    graphics/RendeTarget.h
    graphics/Shader.h
    graphics/SubTexture.h
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

    demos/ShadowMappingLab.h
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
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:RTRLab>"
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES
    ${GLAB_CORE_SOURCES}
    ${GLAB_CORE_HEADERS}
    main.cpp
)