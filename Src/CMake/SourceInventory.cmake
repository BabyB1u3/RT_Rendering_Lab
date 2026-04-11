# -----------------------------------------------------------------------------
# Source inventory
# -----------------------------------------------------------------------------
set(GLAB_APP_SOURCES
    Core/Util/Time.cpp
    Core/Util/CommandLine.cpp
    Core/App/Application.cpp
    Core/App/Window.cpp
    Core/App/Layer.cpp
    Core/App/LayerStack.cpp
)

set(GLAB_APP_HEADERS
    Core/Util/Base.h
    Core/Util/CommandLine.h
    Core/Util/Time.h
    Core/App/Application.h
    Core/App/Layer.h
    Core/App/LayerStack.h
    Core/App/Window.h
    Core/Event/EventBus.h
    Core/Event/Events.h
    Core/Event/ScopedConnection.h
)

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
        Core/Diagnostics/Crash/Backends/Win32/Win32Callstack.cpp
        Core/Diagnostics/Crash/Backends/Win32/Win32CrashHandler.cpp
    )
else()
    list(APPEND GLAB_PLATFORM_SOURCES
        Core/Diagnostics/Crash/Backends/Posix/PosixCallstack.cpp
        Core/Diagnostics/Crash/Backends/Posix/PosixCrashHandler.cpp
    )
endif()

set(GLAB_METAL_SOURCES)
set(GLAB_METAL_HEADERS)

set(GLAB_CORE_SOURCES
    ${GLAB_APP_SOURCES}
    ${GLAB_RESOURCE_SOURCES}
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
    ${GLAB_RESOURCE_HEADERS}
    ${GLAB_INPUT_HEADERS}
    ${GLAB_DIAGNOSTICS_HEADERS}
    ${GLAB_SERIALIZATION_HEADERS}
    ${GLAB_SCENE_HEADERS}
    ${GLAB_DEMO_HEADERS}
    ${GLAB_GUI_HEADERS}
)
