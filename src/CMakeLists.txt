set(GLAB_CORE_SOURCES
    Core/FileSystem.cpp
    Core/Time.cpp

    Core/App/Application.cpp
    Core/App/Window.cpp
    Core/App/Layer.cpp
    Core/App/LayerStack.cpp

    Core/Input/Input.cpp
    Core/Input/Action/InputAction.cpp
    Core/Input/Action/InputContextStack.cpp
    Core/Input/Device/InputDeviceManager.cpp
    Core/Input/Device/KeyboardDevice.cpp
    Core/Input/Device/MouseDevice.cpp
    Core/Input/Device/GamepadDevice.cpp
    Core/Input/Code/InputNames.cpp

    Core/Diagnostics/Assert.cpp
    Core/Diagnostics/CrashHandler.cpp
    Core/Diagnostics/Debugger.cpp
    Core/Diagnostics/FrameFormatter.cpp
    Core/Diagnostics/ImGuiConsoleSink.cpp
    Core/Diagnostics/JsonLineSink.cpp
    Core/Diagnostics/Logger.cpp

    Core/Serialization/PropertyTree.cpp
    Core/Serialization/JsonBackend.cpp
    
    Scene/Camera.cpp
    Scene/DebugCameraController.cpp

    Demos/LabLayer.cpp
    Demos/DemoRegistry.cpp
    Demos/01_HelloWindow/HelloWindow.cpp

    GUI/ImGuiLayer.cpp
    GUI/Panels/ConsolePanel.cpp
    GUI/Panels/DebugPanel.cpp
    GUI/Panels/DemoSelectorPanel.cpp
    GUI/Backends/Metal/MetalImGuiBridge.h
)

set(GLAB_CORE_HEADERS
    Core/Base.h
    Core/Time.h
    Core/FileSystem.h
    
    Core/App/Application.h
    Core/App/Window.h
    Core/App/Layer.h
    Core/App/LayerStack.h
    
    Core/Event/EventBus.h
    Core/Event/Events.h
    Core/Event/ScopedConnection.h
    
    Core/Input/Input.h
    Core/Input/Action/InputAction.h
    Core/Input/Action/InputContextStack.h
    Core/Input/Device/InputDevice.h
    Core/Input/Device/InputDeviceManager.h
    Core/Input/Device/KeyboardDevice.h
    Core/Input/Device/MouseDevice.h
    Core/Input/Code/GamepadCode.h
    Core/Input/Device/GamepadDevice.h
    Core/Input/Action/InputModifier.h
    Core/Input/Action/InputTrigger.h
    Core/Input/Code/KeyCode.h
    Core/Input/Code/MouseCode.h
    Core/Input/Action/InputActionSerialization.h
    Core/Input/Code/InputNames.h

    Core/Diagnostics/Assert.h
    Core/Diagnostics/Callstack.h
    Core/Diagnostics/CrashHandler.h
    Core/Diagnostics/Debugger.h
    Core/Diagnostics/ErrorMacros.h
    Core/Diagnostics/LogCategories.h
    Core/Diagnostics/FrameFormatter.h
    Core/Diagnostics/ImGuiConsoleSink.h
    Core/Diagnostics/JsonLineSink.h
    Core/Diagnostics/Logger.h
    Core/Diagnostics/LogMacros.h
    
    Core/Serialization/PropertyTree.h
    Core/Serialization/SerializationTraits.h
    Core/Serialization/BuiltinTraits.h
    Core/Serialization/IFormatBackend.h
    Core/Serialization/JsonBackend.h
    Core/Serialization/Serialization.h

    Scene/Camera.h
    Scene/DebugCameraController.h
    Scene/Light.h
    Scene/SceneData.h
    Scene/Transform.h

    Demos/DemoBase.h
    Demos/DemoRegistry.h
    Demos/LabLayer.h
    Demos/01_HelloWindow/HelloWindow.h

    GUI/ImGuiLayer.h
    GUI/Panels/ConsolePanel.h
    GUI/Panels/DebugPanel.h
    GUI/Panels/DemoSelectorPanel.h
)

if(GLAB_BACKEND_OPENGL)
    list(APPEND GLAB_CORE_SOURCES

    )

    list(APPEND GLAB_CORE_HEADERS

    )
endif()

if(WIN32)
    list(APPEND GLAB_CORE_SOURCES
	Core/Diagnostics/Backends/Win32/CrashHandler.cpp
	Core/Diagnostics/Backends/Win32/Callstack.cpp
    )
else()
    list(APPEND GLAB_CORE_SOURCES
        Core/Diagnostics/Backends/Posix/CrashHandler.cpp
        Core/Diagnostics/Backends/Posix/Callstack.cpp
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
        ${CMAKE_CURRENT_SOURCE_DIR}/Core/App/Application.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Core/Input/Input.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Core/App/Window.cpp
        # ${CMAKE_CURRENT_SOURCE_DIR}/graphics/Backends/opengl/GLFramebuffer.cpp
        # ${CMAKE_CURRENT_SOURCE_DIR}/graphics/Backends/opengl/GLGraphicsDevice.cpp
        # ${CMAKE_CURRENT_SOURCE_DIR}/graphics/Backends/opengl/GLIndexBuffer.cpp
        # ${CMAKE_CURRENT_SOURCE_DIR}/graphics/Backends/opengl/GLRenderCommand.cpp
        # ${CMAKE_CURRENT_SOURCE_DIR}/graphics/Backends/opengl/GLRenderTarget.cpp
        # ${CMAKE_CURRENT_SOURCE_DIR}/graphics/Backends/opengl/GLShader.cpp
        # ${CMAKE_CURRENT_SOURCE_DIR}/graphics/Backends/opengl/GLTexture2D.cpp
        # ${CMAKE_CURRENT_SOURCE_DIR}/graphics/Backends/opengl/GLVertexArray.cpp
        # ${CMAKE_CURRENT_SOURCE_DIR}/graphics/Backends/opengl/GLVertexBuffer.cpp
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
