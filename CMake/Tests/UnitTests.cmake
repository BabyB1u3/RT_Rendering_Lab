# -----------------------------------------------------------------------------
# Unit tests
# -----------------------------------------------------------------------------
set(GLAB_UNIT_TEST_SOURCES
    Unit/TestAssetPath.cpp
    Unit/TestBuiltinTraits.cpp
    Unit/TestCamera.cpp
    Unit/TestCommandLine.cpp
    Unit/TestCookedCatalog.cpp
    Unit/TestDefaultCameraController.cpp
    Unit/TestEventBus.cpp
    Unit/TestFileSystemVirtualPaths.cpp
    Unit/TestImGuiConsoleSink.cpp
    Unit/TestInput.cpp
    Unit/TestInputAction.cpp
    Unit/TestInputContextStack.cpp
    Unit/TestInputDeviceManager.cpp
    Unit/TestInputModifiers.cpp
    Unit/TestInputNames.cpp
    Unit/TestInputPatterns.cpp
    Unit/TestInputRecording.cpp
    Unit/TestInputTriggers.cpp
    Unit/TestJsonBackend.cpp
    Unit/TestLayerStackLifecycle.cpp
    Unit/TestLayerStack.cpp
    Unit/TestLogCategories.cpp
    Unit/TestPakArchive.cpp
    Unit/TestPropertyTree.cpp
    Unit/TestSourceCatalog.cpp
    Unit/TestSpecifications.cpp
    Unit/TestTime.cpp
    Unit/TestTransform.cpp
)

glab_add_test_executable(rtrlab_unit_tests "Unit." "unit;common"
    ${GLAB_UNIT_TEST_SOURCES}
)
