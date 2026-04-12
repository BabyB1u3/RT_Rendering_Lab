# -----------------------------------------------------------------------------
# Unit tests
# -----------------------------------------------------------------------------
set(GLAB_UNIT_TEST_SOURCES
    Common/Unit/Scene/TestCamera.cpp
    Common/Unit/Scene/TestDefaultCameraController.cpp
    Common/Unit/Scene/TestTransform.cpp
    Common/Unit/Core/App/TestLayerStack.cpp
    Common/Unit/Core/App/TestLayerStackLifecycle.cpp
    Common/Unit/Core/App/TestSpecifications.cpp
    Common/Unit/Core/Diagnostics/TestImGuiConsoleSink.cpp
    Common/Unit/Core/Diagnostics/TestLogCategories.cpp
    Common/Unit/Core/Event/TestEventBus.cpp
    Common/Unit/Core/Input/TestInput.cpp
    Common/Unit/Core/Input/TestInputAction.cpp
    Common/Unit/Core/Input/TestInputContextStack.cpp
    Common/Unit/Core/Input/TestInputDeviceManager.cpp
    Common/Unit/Core/Input/TestInputModifiers.cpp
    Common/Unit/Core/Input/TestInputNames.cpp
    Common/Unit/Core/Input/TestInputPatterns.cpp
    Common/Unit/Core/Input/TestInputRecording.cpp
    Common/Unit/Core/Input/TestInputTriggers.cpp
    Common/Unit/Core/Resource/TestAssetPath.cpp
    Common/Unit/Core/Resource/TestCookedCatalog.cpp
    Common/Unit/Core/Resource/TestFileSystemVirtualPaths.cpp
    Common/Unit/Core/Resource/TestPakArchive.cpp
    Common/Unit/Core/Resource/TestSourceCatalog.cpp
    Common/Unit/Core/Serialization/TestBuiltinTraits.cpp
    Common/Unit/Core/Serialization/TestJsonBackend.cpp
    Common/Unit/Core/Serialization/TestPropertyTree.cpp
    Common/Unit/Core/Util/TestCommandLine.cpp
    Common/Unit/Core/Util/TestTime.cpp
)

glab_add_test_executable(rtrlab_unit_tests "Unit." "unit;common"
    ${GLAB_UNIT_TEST_SOURCES}
)
