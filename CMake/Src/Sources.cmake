# -----------------------------------------------------------------------------
# Source inventory
# -----------------------------------------------------------------------------
set(GLAB_APP_SOURCES
    Core/Util/Time.cpp
    Core/Util/CommandLine.cpp
    Core/App/Application.cpp
    Core/App/Window/Window.cpp
    Core/App/Layer/Layer.cpp
    Core/App/Layer/LayerStack.cpp
)

set(GLAB_APP_HEADERS
    Core/Util/Base.h
    Core/Util/CommandLine.h
    Core/Util/Time.h
    Core/App/Application.h
    Core/App/Layer/Layer.h
    Core/App/Layer/LayerStack.h
    Core/App/Window/Window.h
    Core/Event/EventBus.h
    Core/Event/Events.h
    Core/Event/ScopedConnection.h
)

if(WIN32)
    list(APPEND GLAB_APP_SOURCES
        Core/App/Window/Native/Win32/Win32NativeWindow.cpp
    )

    list(APPEND GLAB_APP_HEADERS
        Core/App/Window/Native/Win32/Win32NativeWindow.h
    )
endif()

if(APPLE)
    list(APPEND GLAB_APP_SOURCES
        Core/App/Window/Native/Cocoa/CocoaNativeWindow.mm
    )

    list(APPEND GLAB_APP_HEADERS
        Core/App/Window/Native/Cocoa/CocoaNativeWindow.h
    )
endif()

if(UNIX AND NOT APPLE)
    list(APPEND GLAB_APP_SOURCES
        Core/App/Window/Native/Linux/LinuxNativeWindow.cpp
    )

    list(APPEND GLAB_APP_HEADERS
        Core/App/Window/Native/Linux/LinuxNativeWindow.h
    )
endif()

set(GLAB_RESOURCE_SOURCES
    Core/Resource/Cook/CookedCatalog.cpp
    Core/Resource/FileSystem.cpp
    Core/Resource/Mount/MountBackend.cpp
    Core/Resource/Mount/MountResolver.cpp
    Core/Resource/Package/PakArchive.cpp
    Core/Resource/Path/PathParser.cpp
    Core/Resource/IO/PhysicalIO.cpp
    Core/Resource/Catalog/ResourceCatalog.cpp
    Core/Resource/Catalog/SourceCatalog.cpp
    Core/Resource/Mount/RootDiscovery.cpp
)

set(GLAB_RESOURCE_HEADERS
    Core/Resource/Catalog/AssetPath.h
    Core/Resource/Cook/CookedCatalog.h
    Core/Resource/FileSystem.h
    Core/Resource/Mount/MountBackend.h
    Core/Resource/Mount/MountResolver.h
    Core/Resource/Package/PakArchive.h
    Core/Resource/Path/PathParser.h
    Core/Resource/Path/PathTypes.h
    Core/Resource/IO/PhysicalIO.h
    Core/Resource/Catalog/ResourceCatalog.h
    Core/Resource/Catalog/SourceCatalog.h
    Core/Resource/Mount/RootDiscovery.h
)

set(GLAB_INPUT_SOURCES
    Core/Input/Input.cpp
    Core/Input/Action/InputAction.cpp
    Core/Input/Action/InputContextStack.cpp
    Core/Input/Action/InputPatterns.cpp
    Core/Input/Action/InputSource.cpp
    Core/Input/Code/InputNames.cpp
    Core/Input/Device/GamepadDevice.cpp
    Core/Input/Device/InputDeviceManager.cpp
    Core/Input/Device/KeyboardDevice.cpp
    Core/Input/Device/MouseDevice.cpp
    Core/Input/Replay/InputRecording.cpp
    Core/Input/Replay/ReplayDevice.cpp
)

set(GLAB_INPUT_HEADERS
    Core/Input/Input.h
    Core/Input/Action/InputAction.h
    Core/Input/Action/InputActionSerialization.h
    Core/Input/Action/InputContextStack.h
    Core/Input/Action/InputModifier.h
    Core/Input/Action/InputPatterns.h
    Core/Input/Action/InputSource.h
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
    Core/Input/Replay/InputRecording.h
    Core/Input/Replay/ReplayDevice.h
)

set(GLAB_DIAGNOSTICS_SOURCES
    Core/Diagnostics/Assert/Assert.cpp
    Core/Diagnostics/Crash/CrashHandler.cpp
    Core/Diagnostics/Crash/Debugger.cpp
    Core/Diagnostics/Logging/FrameFormatter.cpp
    Core/Diagnostics/Logging/ImGuiConsoleSink.cpp
    Core/Diagnostics/Logging/JsonLineSink.cpp
    Core/Diagnostics/Logging/Logger.cpp
)

set(GLAB_DIAGNOSTICS_HEADERS
    Core/Diagnostics/Assert/Assert.h
    Core/Diagnostics/Crash/Callstack.h
    Core/Diagnostics/Crash/CrashHandler.h
    Core/Diagnostics/Crash/Debugger.h
    Core/Diagnostics/Assert/ErrorMacros.h
    Core/Diagnostics/Logging/FrameFormatter.h
    Core/Diagnostics/Logging/ImGuiConsoleSink.h
    Core/Diagnostics/Logging/JsonLineSink.h
    Core/Diagnostics/Logging/LogCategories.h
    Core/Diagnostics/Logging/Logger.h
    Core/Diagnostics/Logging/LogMacros.h
)

set(GLAB_SERIALIZATION_SOURCES
    Core/Serialization/JsonBackend.cpp
    Core/Serialization/PropertyTree.cpp
)

set(GLAB_SERIALIZATION_HEADERS
    Core/Serialization/BuiltinTraits.h
    Core/Serialization/FormatBackend.h
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

set(GLAB_RENDER_SOURCES
    Render/RHI/RHIFactory.cpp
    Render/RHI/RHIUpload.cpp
    Render/RHI/ResourceStateTracker.cpp
    Render/RHI/Backends/Common/RHIShellCommon.cpp
    Render/Renderer/ForwardRenderer.cpp
    Render/Shader/ShaderCompiler.cpp
    Render/Shader/ShaderReflection.cpp
    Render/Shader/SlangCompiler.cpp
    Render/Shader/SlangReflectionConverter.cpp
    Render/Shader/SlangReflectionJson.cpp
    Render/Shader/ShaderParameterWriter.cpp
)

set(GLAB_RENDER_HEADERS
    Render/RHI/NativeWindowHandle.h
    Render/RHI/RHI.h
    Render/RHI/RHIFactory.h
    Render/RHI/RHIResources.h
    Render/RHI/RHIPipeline.h
    Render/RHI/RHICommandList.h
    Render/RHI/RHIDevice.h
    Render/RHI/RHIUpload.h
    Render/RHI/Backends/Common/RHIShellCommon.h
    Render/Renderer/ForwardRenderer.h
    Render/Renderer/FrameGlobals.h
    Render/Renderer/Material.h
    Render/Renderer/Mesh.h
    Render/Renderer/RenderObject.h
    Render/Shader/ShaderCompiler.h
    Render/Shader/ShaderReflection.h
    Render/Shader/SlangCompiler.h
    Render/Shader/SlangReflectionConverter.h
    Render/Shader/SlangReflectionJson.h
    Render/Shader/ShaderParameterWriter.h
    Render/Shader/ShaderTypes.h
)

set(GLAB_RENDER_BACKEND_VULKAN_SOURCES
    Render/RHI/Backends/Vulkan/Common/VulkanBarriers.cpp
    Render/RHI/Backends/Vulkan/Command/VulkanCommandList.cpp
    Render/RHI/Backends/Vulkan/Common/VulkanConversions.cpp
    Render/RHI/Backends/Vulkan/Common/VulkanDescriptors.cpp
    Render/RHI/Backends/Vulkan/Device/VulkanDevice.cpp
    Render/RHI/Backends/Vulkan/Resources/VulkanResourceSet.cpp
    Render/RHI/Backends/Vulkan/Presentation/VulkanSurface.cpp
    Render/RHI/Backends/Vulkan/Presentation/VulkanSwapchain.cpp
    Render/RHI/Backends/Vulkan/Resources/VulkanTexture.cpp
)

set(GLAB_RENDER_BACKEND_VULKAN_HEADERS
    Render/RHI/Backends/Vulkan/Common/VulkanBarriers.h
    Render/RHI/Backends/Vulkan/Resources/VulkanBuffer.h
    Render/RHI/Backends/Vulkan/Command/VulkanCommandList.h
    Render/RHI/Backends/Vulkan/Common/VulkanCommon.h
    Render/RHI/Backends/Vulkan/Common/VulkanConversions.h
    Render/RHI/Backends/Vulkan/Common/VulkanDescriptors.h
    Render/RHI/Backends/Vulkan/Device/VulkanDevice.h
    Render/RHI/Backends/Vulkan/Pipeline/VulkanGraphicsPipeline.h
    Render/RHI/Backends/Vulkan/Pipeline/VulkanPipelineLayout.h
    Render/RHI/Backends/Vulkan/Resources/VulkanResourceSet.h
    Render/RHI/Backends/Vulkan/Resources/VulkanSampler.h
    Render/RHI/Backends/Vulkan/Pipeline/VulkanShaderProgram.h
    Render/RHI/Backends/Vulkan/Presentation/VulkanSurface.h
    Render/RHI/Backends/Vulkan/Presentation/VulkanSwapchain.h
    Render/RHI/Backends/Vulkan/Resources/VulkanTexture.h

)

set(GLAB_RENDER_BACKEND_METAL_SOURCES
    Render/RHI/Backends/Metal/Resources/MetalBuffer.mm
    Render/RHI/Backends/Metal/Command/MetalCommandList.mm
    Render/RHI/Backends/Metal/Common/MetalConversions.mm
    Render/RHI/Backends/Metal/Device/MetalDevice.mm
    Render/RHI/Backends/Metal/Pipeline/MetalGraphicsPipeline.mm
    Render/RHI/Backends/Metal/Pipeline/MetalPipelineLayout.mm
    Render/RHI/Backends/Metal/Resources/MetalResourceSet.mm
    Render/RHI/Backends/Metal/Resources/MetalSampler.mm
    Render/RHI/Backends/Metal/Pipeline/MetalShaderProgram.mm
    Render/RHI/Backends/Metal/Presentation/MetalSwapchain.mm
    Render/RHI/Backends/Metal/Resources/MetalTexture.mm
)

set(GLAB_RENDER_BACKEND_METAL_HEADERS
    Render/RHI/Backends/Metal/Resources/MetalBuffer.h
    Render/RHI/Backends/Metal/Command/MetalCommandList.h
    Render/RHI/Backends/Metal/Common/MetalCommon.h
    Render/RHI/Backends/Metal/Common/MetalConversions.h
    Render/RHI/Backends/Metal/Device/MetalDevice.h
    Render/RHI/Backends/Metal/Pipeline/MetalGraphicsPipeline.h
    Render/RHI/Backends/Metal/Pipeline/MetalPipelineLayout.h
    Render/RHI/Backends/Metal/Resources/MetalResourceSet.h
    Render/RHI/Backends/Metal/Resources/MetalSampler.h
    Render/RHI/Backends/Metal/Pipeline/MetalShaderProgram.h
    Render/RHI/Backends/Metal/Presentation/MetalSwapchain.h
    Render/RHI/Backends/Metal/Resources/MetalTexture.h

)

if(GLAB_BACKEND_VULKAN)
    list(APPEND GLAB_RENDER_SOURCES
        ${GLAB_RENDER_BACKEND_VULKAN_SOURCES}
    )

    list(APPEND GLAB_RENDER_HEADERS
        ${GLAB_RENDER_BACKEND_VULKAN_HEADERS}
    )
endif()

if(GLAB_BACKEND_METAL)
    list(APPEND GLAB_RENDER_SOURCES
        ${GLAB_RENDER_BACKEND_METAL_SOURCES}
    )

    list(APPEND GLAB_RENDER_HEADERS
        ${GLAB_RENDER_BACKEND_METAL_HEADERS}
    )
endif()

set(GLAB_DEMO_SOURCES
    Demos/DemoRegistry.cpp
    Demos/DemoRenderUtils.cpp
    Demos/LabLayer.cpp
    Demos/01_ClearColor/ClearColorDemo.cpp
    Demos/02_Triangle/TriangleDemo.cpp
    Demos/03_Quad/QuadDemo.cpp
    Demos/04_TexturedQuad/TexturedQuadDemo.cpp
    Demos/05_RotatingCube/RotatingCubeDemo.cpp
    Demos/06_TexturedRotatingCube/TexturedRotatingCubeDemo.cpp
    Demos/07_MultiObjectTexturedScene/MultiObjectTexturedSceneDemo.cpp
)

set(GLAB_DEMO_HEADERS
    Demos/DemoBase.h
    Demos/DemoRegistry.h
    Demos/DemoRenderUtils.h
    Demos/LabLayer.h
    Demos/01_ClearColor/ClearColorDemo.h
    Demos/02_Triangle/TriangleDemo.h
    Demos/03_Quad/QuadDemo.h
    Demos/04_TexturedQuad/TexturedQuadDemo.h
    Demos/05_RotatingCube/RotatingCubeDemo.h
    Demos/06_TexturedRotatingCube/TexturedRotatingCubeDemo.h
    Demos/07_MultiObjectTexturedScene/MultiObjectTexturedSceneDemo.h
)

set(GLAB_GUI_SOURCES
    GUI/ImGuiLayer.cpp
    GUI/Panels/ConsolePanel.cpp
    GUI/Panels/DebugPanel.cpp
    GUI/Panels/DemoSelectorPanel.cpp
)

set(GLAB_GUI_HEADERS
    GUI/ImGuiLayer.h
    GUI/Panels/ConsolePanel.h
    GUI/Panels/DebugPanel.h
    GUI/Panels/DemoSelectorPanel.h
)

if(GLAB_BACKEND_METAL)
    list(APPEND GLAB_GUI_SOURCES
        GUI/Backends/Metal/MetalImGuiBridge.mm
    )

    list(APPEND GLAB_GUI_HEADERS
        GUI/Backends/Metal/MetalImGuiBridge.h
    )
endif()

if(GLAB_BACKEND_VULKAN)
    list(APPEND GLAB_GUI_SOURCES
        GUI/Backends/Vulkan/VulkanImGuiBridge.cpp
    )

    list(APPEND GLAB_GUI_HEADERS
        GUI/Backends/Vulkan/VulkanImGuiBridge.h
    )
endif()

set(GLAB_PLATFORM_SOURCES)
if(WIN32)
    list(APPEND GLAB_PLATFORM_SOURCES
        Core/Diagnostics/Crash/Backends/Win32/Win32Callstack.cpp
        Core/Diagnostics/Crash/Backends/Win32/Win32CrashHandler.cpp
    )
else()
    list(APPEND GLAB_PLATFORM_SOURCES
        Core/Diagnostics/Crash/Backends/Posix/PosixCallstack.cpp
        Core/Diagnostics/Crash/Backends/Posix/PosixCrashHandler.cpp
    )
endif()

set(GLAB_CORE_SKIP_UNITY_SOURCES
    Core/App/Window/Window.cpp
)

if(WIN32)
    list(APPEND GLAB_CORE_SKIP_UNITY_SOURCES
        Core/App/Window/Native/Win32/Win32NativeWindow.cpp
    )
endif()

if(APPLE)
    list(APPEND GLAB_CORE_SKIP_UNITY_SOURCES
        Core/App/Window/Native/Cocoa/CocoaNativeWindow.mm
    )
endif()

if(UNIX AND NOT APPLE)
    list(APPEND GLAB_CORE_SKIP_UNITY_SOURCES
        Core/App/Window/Native/Linux/LinuxNativeWindow.cpp
    )
endif()

if(GLAB_BACKEND_METAL)
    list(APPEND GLAB_CORE_SKIP_UNITY_SOURCES
        ${GLAB_RENDER_BACKEND_METAL_SOURCES}
        GUI/Backends/Metal/MetalImGuiBridge.mm
    )
endif()

if(GLAB_BACKEND_VULKAN)
    list(APPEND GLAB_CORE_SKIP_UNITY_SOURCES
        ${GLAB_RENDER_BACKEND_VULKAN_SOURCES}
        GUI/Backends/Vulkan/VulkanImGuiBridge.cpp
    )
endif()

set(GLAB_CORE_SOURCES
    ${GLAB_APP_SOURCES}
    ${GLAB_RESOURCE_SOURCES}
    ${GLAB_INPUT_SOURCES}
    ${GLAB_DIAGNOSTICS_SOURCES}
    ${GLAB_SERIALIZATION_SOURCES}
    ${GLAB_SCENE_SOURCES}
    ${GLAB_RENDER_SOURCES}
    ${GLAB_DEMO_SOURCES}
    ${GLAB_GUI_SOURCES}
    ${GLAB_PLATFORM_SOURCES}
)

set(GLAB_CORE_HEADERS
    ${GLAB_APP_HEADERS}
    ${GLAB_RESOURCE_HEADERS}
    ${GLAB_INPUT_HEADERS}
    ${GLAB_DIAGNOSTICS_HEADERS}
    ${GLAB_SERIALIZATION_HEADERS}
    ${GLAB_SCENE_HEADERS}
    ${GLAB_RENDER_HEADERS}
    ${GLAB_DEMO_HEADERS}
    ${GLAB_GUI_HEADERS}
)
