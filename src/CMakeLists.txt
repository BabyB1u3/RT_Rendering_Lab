set(GLAB_CORE_SOURCES
    core/Application.cpp
    core/Window.cpp
    core/Input.cpp
    core/Time.cpp
    core/FileSystem.cpp
    core/Logger.cpp

    graphics/Renderer.cpp
    graphics/Shader.cpp
    graphics/Texture.cpp
    graphics/Cubemap.cpp
    graphics/Material.cpp
    graphics/Mesh.cpp
    graphics/Model.cpp
    graphics/Camera.cpp
    graphics/Framebuffer.cpp
    graphics/Renderbuffer.cpp
    graphics/VertexArray.cpp
    graphics/VertexBuffer.cpp
    graphics/IndexBuffer.cpp
    graphics/UniformBuffer.cpp
    graphics/GBuffer.cpp

    gui/ImGuiLayer.cpp
    gui/DebugPanel.cpp
    gui/DemoSelectorPanel.cpp

    demos/DemoRegistry.cpp
    demos/forward_lighting/ForwardLightingDemo.cpp
)

set(GLAB_CORE_HEADERS
    core/Application.h
    core/Window.h
    core/Input.h
    core/Time.h
    core/FileSystem.h
    core/Logger.h

    graphics/Renderer.h
    graphics/Shader.h
    graphics/Texture.h
    graphics/Cubemap.h
    graphics/Material.h
    graphics/Mesh.h
    graphics/Model.h
    graphics/Camera.h
    graphics/Framebuffer.h
    graphics/Renderbuffer.h
    graphics/VertexArray.h
    graphics/VertexBuffer.h
    graphics/IndexBuffer.h
    graphics/UniformBuffer.h
    graphics/GBuffer.h

    gui/ImGuiLayer.h
    gui/DebugPanel.h
    gui/DemoSelectorPanel.h

    demos/DemoBase.h
    demos/DemoRegistry.h
    demos/forward_lighting/ForwardLightingDemo.h
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
    imgui
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