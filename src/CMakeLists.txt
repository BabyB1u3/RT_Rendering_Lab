set(GLAB_CORE_SOURCES
    core/FileSystem.cpp
    core/Time.cpp

    core/app/Application.cpp
    core/app/Window.cpp
    core/app/Layer.cpp
    core/app/LayerStack.cpp

    core/input/Input.cpp
    core/input/InputAction.cpp
    core/input/InputContextStack.cpp
    core/input/InputDeviceManager.cpp
    core/input/KeyboardDevice.cpp
    core/input/MouseDevice.cpp
    core/input/GamepadDevice.cpp
    core/input/InputNames.cpp

    core/diagnostics/Assert.cpp
    core/diagnostics/CrashHandler.cpp
    core/diagnostics/Debugger.cpp
    core/diagnostics/FrameFormatter.cpp
    core/diagnostics/ImGuiConsoleSink.cpp
    core/diagnostics/JsonLineSink.cpp
    core/diagnostics/Logger.cpp

    core/serialization/PropertyTree.cpp
    core/serialization/JsonBackend.cpp
    
    scene/Camera.cpp
    scene/DebugCameraController.cpp

    demos/LabLayer.cpp
    demos/DemoRegistry.cpp
    demos/01_HelloWindow/HelloWindow.cpp

    gui/ImGuiLayer.cpp
    gui/panels/ConsolePanel.cpp
    gui/panels/DebugPanel.cpp
    gui/panels/DemoSelectorPanel.cpp
    gui/backends/metal/MetalImGuiBridge.h
)

set(GLAB_CORE_HEADERS
    core/Base.h
    core/Time.h
    core/FileSystem.h
    
    core/app/Application.h
    core/app/Window.h
    core/app/Layer.h
    core/app/LayerStack.h
    
    core/event/EventBus.h
    core/event/Events.h
    core/event/ScopedConnection.h
    
    core/input/Input.h
    core/input/InputAction.h
    core/input/InputContextStack.h
    core/input/InputDevice.h
    core/input/InputDeviceManager.h
    core/input/KeyboardDevice.h
    core/input/MouseDevice.h
    core/input/GamepadCode.h
    core/input/GamepadDevice.h
    core/input/InputModifier.h
    core/input/InputTrigger.h
    core/input/KeyCode.h
    core/input/MouseCode.h
    core/input/InputActionSerialization.h

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

    scene/Camera.h
    scene/DebugCameraController.h
    scene/Light.h
    scene/SceneData.h
    scene/Transform.h

    demos/DemoBase.h
    demos/DemoRegistry.h
    demos/LabLayer.h
    demos/01_HelloWindow/HelloWindow.h

    gui/ImGuiLayer.h
    gui/panels/ConsolePanel.h
    gui/panels/DebugPanel.h
    gui/panels/DemoSelectorPanel.h
)

if(GLAB_BACKEND_OPENGL)
    list(APPEND GLAB_CORE_SOURCES

    )

    list(APPEND GLAB_CORE_HEADERS

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

    )

    set(GLAB_METAL_HEADERS

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
        # ${CMAKE_CURRENT_SOURCE_DIR}/graphics/backends/opengl/GLFramebuffer.cpp
        # ${CMAKE_CURRENT_SOURCE_DIR}/graphics/backends/opengl/GLGraphicsDevice.cpp
        # ${CMAKE_CURRENT_SOURCE_DIR}/graphics/backends/opengl/GLIndexBuffer.cpp
        # ${CMAKE_CURRENT_SOURCE_DIR}/graphics/backends/opengl/GLRenderCommand.cpp
        # ${CMAKE_CURRENT_SOURCE_DIR}/graphics/backends/opengl/GLRenderTarget.cpp
        # ${CMAKE_CURRENT_SOURCE_DIR}/graphics/backends/opengl/GLShader.cpp
        # ${CMAKE_CURRENT_SOURCE_DIR}/graphics/backends/opengl/GLTexture2D.cpp
        # ${CMAKE_CURRENT_SOURCE_DIR}/graphics/backends/opengl/GLVertexArray.cpp
        # ${CMAKE_CURRENT_SOURCE_DIR}/graphics/backends/opengl/GLVertexBuffer.cpp
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
