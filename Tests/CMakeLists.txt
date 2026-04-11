include(GoogleTest)

function(glab_configure_test_target target_name test_prefix test_labels)
    set(escaped_test_labels "${test_labels}")
    string(REPLACE ";" "\\;" escaped_test_labels "${escaped_test_labels}")

    target_link_libraries(${target_name}
        PRIVATE
            RTRLabCore
            GTest::gtest_main
    )

    target_include_directories(${target_name}
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/Support
    )

    if(GLAB_ENABLE_PCH)
        target_precompile_headers(${target_name} PRIVATE
            "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/pch.h>"
        )
    endif()

    glab_configure_local_target(${target_name})
    gtest_discover_tests(${target_name}
        TEST_PREFIX "${test_prefix}"
        PROPERTIES
            LABELS "${escaped_test_labels}"
    )
endfunction()

function(glab_add_test_executable target_name test_prefix test_labels)
    add_executable(${target_name}
        ${ARGN}
    )

    glab_configure_test_target(${target_name} "${test_prefix}" "${test_labels}")
endfunction()

# -----------------------------------------------------------------------------
# Unit tests
# -----------------------------------------------------------------------------
set(GLAB_UNIT_TEST_SOURCES
    Unit/TestAssetPath.cpp
    Unit/TestBuiltinTraits.cpp
    Unit/TestCamera.cpp
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

# -----------------------------------------------------------------------------
# Integration tests
# -----------------------------------------------------------------------------
set(GLAB_INTEGRATION_TEST_SOURCES
    Integration/TestCookPipeline.cpp
    Integration/TestLoggerConsoleSink.cpp
    Integration/TestMountBackend.cpp
    Integration/TestInputRecordingIO.cpp
    Integration/TestPackagePipeline.cpp
    Integration/TestSourceIndexPipeline.cpp
)

glab_add_test_executable(rtrlab_integration_tests "Integration." "integration;common"
    ${GLAB_INTEGRATION_TEST_SOURCES}
)

set(GLAB_DEV_PROFILE_TEST_SOURCES
    Integration/TestCatalogRegistryDev.cpp
    Integration/TestMountDiscoveryDev.cpp
    Integration/TestRootDiscoveryDev.cpp
)

glab_add_test_executable(rtrlab_dev_profile_tests "DevProfile." "integration;dev-profile"
    ${GLAB_DEV_PROFILE_TEST_SOURCES}
)

set(GLAB_SHIPPING_PROFILE_TEST_SOURCES
    Integration/TestMountDiscoveryShipping.cpp
    Integration/TestRootDiscoveryShipping.cpp
)

glab_add_test_executable(rtrlab_shipping_profile_tests "ShippingProfile." "integration;shipping-profile"
    ${GLAB_SHIPPING_PROFILE_TEST_SOURCES}
)

# -----------------------------------------------------------------------------
# Smoke tests
# -----------------------------------------------------------------------------
# Add Smoke/*.cpp here once the first smoke tests are migrated.

if(GLAB_BACKEND_OPENGL)
    # set_source_files_properties(
    #     ${CMAKE_CURRENT_SOURCE_DIR}/Support/GLTestContext.cpp
    #     PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON
    # )

    # glab_add_test_executable(rtrlab_integration_tests_opengl
    #     integration/opengl/TestRenderResults.cpp
    #     integration/opengl/TestSceneRenderer.cpp
    #     integration/opengl/TestShader.cpp
    #     integration/opengl/TestTexture.cpp
    #     Support/GLTestContext.cpp
    # )

endif()
