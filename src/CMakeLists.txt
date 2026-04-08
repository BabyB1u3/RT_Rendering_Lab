# -----------------------------------------------------------------------------
# Source inventory
# -----------------------------------------------------------------------------
set(GLAB_APP_SOURCES
    Core/FileSystem.cpp
    Core/Time.cpp
    Core/App/Application.cpp
    Core/App/Window.cpp
    Core/App/Layer.cpp
    Core/App/LayerStack.cpp
)

set(GLAB_APP_HEADERS
    Core/Base.h
    Core/FileSystem.h
    Core/Time.h
    Core/App/Application.h
    Core/App/Layer.h
    Core/App/LayerStack.h
    Core/App/Window.h
    Core/Event/EventBus.h
    Core/Event/Events.h
    Core/Event/ScopedConnection.h
)

set(GLAB_INPUT_SOURCES
    Core/Input/Input.cpp
    Core/Input/Action/InputAction.cpp
    Core/Input/Action/InputContextStack.cpp
    Core/Input/Code/InputNames.cpp
    Core/Input/Device/GamepadDevice.cpp
    Core/Input/Device/InputDeviceManager.cpp
    Core/Input/Device/KeyboardDevice.cpp
    Core/Input/Device/MouseDevice.cpp
)

set(GLAB_INPUT_HEADERS
    Core/Input/Input.h
    Core/Input/Action/InputAction.h
    Core/Input/Action/InputActionSerialization.h
    Core/Input/Action/InputContextStack.h
    Core/Input/Action/InputModifier.h
    Core/Input/Action/InputTrigger.h
    Core/Input/Code/GamepadCode.h
    Core/Input/Code/InputNames.h
    Core/Input/Code/KeyCode.h
    Core/Input/Code/MouseCode.h
    Core/Input/Device/GamepadDevice.h
    Core/Input/Device/InputDevice.h
    Core/Input/Device/InputDeviceManager.h
    Core/Input/Device/KeyboardDevice.h
    Core/Input/Device/MouseDevice.h
)

set(GLAB_DIAGNOSTICS_SOURCES
    Core/Diagnostics/Assert.cpp
    Core/Diagnostics/CrashHandler.cpp
    Core/Diagnostics/Debugger.cpp
    Core/Diagnostics/FrameFormatter.cpp
    Core/Diagnostics/ImGuiConsoleSink.cpp
    Core/Diagnostics/JsonLineSink.cpp
    Core/Diagnostics/Logger.cpp
)

set(GLAB_DIAGNOSTICS_HEADERS
    Core/Diagnostics/Assert.h
    Core/Diagnostics/Callstack.h
    Core/Diagnostics/CrashHandler.h
    Core/Diagnostics/Debugger.h
    Core/Diagnostics/ErrorMacros.h
    Core/Diagnostics/FrameFormatter.h
    Core/Diagnostics/ImGuiConsoleSink.h
    Core/Diagnostics/JsonLineSink.h
    Core/Diagnostics/LogCategories.h
    Core/Diagnostics/Logger.h
    Core/Diagnostics/LogMacros.h
)

set(GLAB_SERIALIZATION_SOURCES
    Core/Serialization/JsonBackend.cpp
    Core/Serialization/PropertyTree.cpp
)

set(GLAB_SERIALIZATION_HEADERS
    Core/Serialization/BuiltinTraits.h
    Core/Serialization/IFormatBackend.h
    Core/Serialization/JsonBackend.h
    Core/Serialization/PropertyTree.h
    Core/Serialization/Serialization.h
    Core/Serialization/SerializationTraits.h
)

set(GLAB_SCENE_SOURCES
    Scene/Camera.cpp
    Scene/DebugCameraController.cpp
)

set(GLAB_SCENE_HEADERS
    Scene/Camera.h
    Scene/DebugCameraController.h
    Scene/Light.h
    Scene/SceneData.h
    Scene/Transform.h
)

set(GLAB_DEMO_SOURCES
    Demos/DemoRegistry.cpp
    Demos/LabLayer.cpp
    Demos/01_HelloWindow/HelloWindow.cpp
)

set(GLAB_DEMO_HEADERS
    Demos/DemoBase.h
    Demos/DemoRegistry.h
    Demos/LabLayer.h
    Demos/01_HelloWindow/HelloWindow.h
)

set(GLAB_GUI_SOURCES
    GUI/ImGuiLayer.cpp
    GUI/Panels/ConsolePanel.cpp
    GUI/Panels/DebugPanel.cpp
    GUI/Panels/DemoSelectorPanel.cpp
)

set(GLAB_GUI_HEADERS
    GUI/ImGuiLayer.h
    GUI/Backends/Metal/MetalImGuiBridge.h
    GUI/Panels/ConsolePanel.h
    GUI/Panels/DebugPanel.h
    GUI/Panels/DemoSelectorPanel.h
)

set(GLAB_PLATFORM_SOURCES)
if(WIN32)
    list(APPEND GLAB_PLATFORM_SOURCES
        Core/Diagnostics/Backends/Win32/Callstack.cpp
        Core/Diagnostics/Backends/Win32/CrashHandler.cpp
    )
else()
    list(APPEND GLAB_PLATFORM_SOURCES
        Core/Diagnostics/Backends/Posix/Callstack.cpp
        Core/Diagnostics/Backends/Posix/CrashHandler.cpp
    )
endif()

set(GLAB_METAL_SOURCES)
set(GLAB_METAL_HEADERS)

set(GLAB_CORE_SOURCES
    ${GLAB_APP_SOURCES}
    ${GLAB_INPUT_SOURCES}
    ${GLAB_DIAGNOSTICS_SOURCES}
    ${GLAB_SERIALIZATION_SOURCES}
    ${GLAB_SCENE_SOURCES}
    ${GLAB_DEMO_SOURCES}
    ${GLAB_GUI_SOURCES}
    ${GLAB_PLATFORM_SOURCES}
)

set(GLAB_CORE_HEADERS
    ${GLAB_APP_HEADERS}
    ${GLAB_INPUT_HEADERS}
    ${GLAB_DIAGNOSTICS_HEADERS}
    ${GLAB_SERIALIZATION_HEADERS}
    ${GLAB_SCENE_HEADERS}
    ${GLAB_DEMO_HEADERS}
    ${GLAB_GUI_HEADERS}
)

# -----------------------------------------------------------------------------
# Local helpers
# -----------------------------------------------------------------------------
function(glab_enable_local_pch target_name)
    if(GLAB_ENABLE_PCH)
        target_precompile_headers(${target_name} PRIVATE
            "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/pch.h>"
        )
    endif()
endfunction()

function(glab_copy_runtime_assets target_name)
    add_custom_command(TARGET ${target_name} POST_BUILD
        # Copy source assets (textures, models, etc.)
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_SOURCE_DIR}/assets
                $<TARGET_FILE_DIR:${target_name}>/assets
    )

    if(TARGET CompileShaders)
        add_custom_command(TARGET ${target_name} POST_BUILD
            # Merge compiled shaders into assets/shaders/compiled/ only when
            # the shader build target is enabled and produces this directory.
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    ${CMAKE_BINARY_DIR}/shaders
                    $<TARGET_FILE_DIR:${target_name}>/assets/shaders/compiled
        )
    endif()
endfunction()

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

    set_source_files_properties(
        ${CMAKE_CURRENT_SOURCE_DIR}/Core/App/Application.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Core/App/Window.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Core/Input/Input.cpp
        PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON
    )
endif()

if(GLAB_BACKEND_METAL)
    target_sources(RTRLabCore PRIVATE
        ${GLAB_METAL_SOURCES}
        ${GLAB_METAL_HEADERS}
    )

    set_source_files_properties(${GLAB_METAL_SOURCES}
        PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON
    )

    target_link_libraries(RTRLabCore PUBLIC
        "-framework AppKit"
        "-framework Metal"
        "-framework QuartzCore"
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

# -----------------------------------------------------------------------------
# Main executable
# -----------------------------------------------------------------------------
add_executable(RTRLab
    main.cpp
)

target_link_libraries(RTRLab PRIVATE
    RTRLabCore
)

if(GLAB_ENABLE_PCH)
    target_precompile_headers(RTRLab REUSE_FROM RTRLabCore)
endif()

if(TARGET CompileShaders)
    add_dependencies(RTRLab CompileShaders)
endif()

glab_copy_runtime_assets(RTRLab)
glab_configure_local_target(RTRLab)

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

# -----------------------------------------------------------------------------
# Install rules
# -----------------------------------------------------------------------------
install(TARGETS RTRLab RUNTIME DESTINATION .)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/assets/
        DESTINATION assets
        PATTERN "*.slang" EXCLUDE)

if(TARGET CompileShaders)
    install(DIRECTORY ${CMAKE_BINARY_DIR}/shaders/
            DESTINATION assets/shaders/compiled)
endif()
