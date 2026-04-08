include(GoogleTest)

function(glab_configure_test_target target_name)
    target_link_libraries(${target_name}
        PRIVATE
            RTRLabCore
            GTest::gtest_main
    )

    target_include_directories(${target_name}
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/Support
            ${CMAKE_SOURCE_DIR}/src
    )

    if(GLAB_ENABLE_PCH)
        target_precompile_headers(${target_name} PRIVATE
            "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/pch.h>"
        )
    endif()

    glab_enable_build_speedups(${target_name})

    if(GLAB_ENABLE_WARNINGS)
        glab_set_warnings(${target_name})
    endif()

    if(GLAB_ENABLE_ASAN)
        glab_enable_sanitizers(${target_name})
    endif()

    gtest_discover_tests(${target_name})
endfunction()

add_executable(rtrlab_unit_tests
    Unit/TestTime.cpp
    Unit/TestLayerStack.cpp
    Unit/TestTransform.cpp
    Unit/TestCamera.cpp
    Unit/TestDefaultCameraController.cpp
    Unit/TestSpecifications.cpp
    Unit/TestInputModifiers.cpp
    Unit/TestInputTriggers.cpp
    Unit/TestInputDeviceManager.cpp
    Unit/TestInputContextStack.cpp
    Unit/TestInput.cpp
    Unit/TestInputAction.cpp
    Unit/TestInputNames.cpp
    Unit/TestPropertyTree.cpp
    Unit/TestBuiltinTraits.cpp
    Unit/TestJsonBackend.cpp
)

glab_configure_test_target(rtrlab_unit_tests)

add_executable(rtrlab_contract_tests
    Contract/Core/TestDiagnosticsCore.cpp
    Contract/Core/TestImGuiConsoleSink.cpp
    Contract/Core/TestLogger.cpp
    Contract/Core/TestLogCategories.cpp
    Contract/Core/TestEventBus.cpp
    Contract/Core/TestFileSystem.cpp
    Contract/Core/TestLayerStackLifecycle.cpp
    Contract/Core/TestSerializationFileIO.cpp
)

glab_configure_test_target(rtrlab_contract_tests)

add_executable(rtrlab_contract_tests_log_min_level
    Contract/Core/TestDiagnosticsMinLevel.cpp
)

glab_configure_test_target(rtrlab_contract_tests_log_min_level)

if(GLAB_BACKEND_OPENGL)
    # set_source_files_properties(
    #     ${CMAKE_CURRENT_SOURCE_DIR}/Support/GLTestContext.cpp
    #     PROPERTIES
    #         SKIP_UNITY_BUILD_INCLUSION ON
    # )

    # add_executable(rtrlab_contract_tests_opengl
    #     Contract/opengl/TestFramebuffer.cpp
    #     Contract/opengl/TestRenderPasses.cpp
    #     Contract/opengl/TestRenderTarget.cpp
    #     Support/GLTestContext.cpp
    # )

    # if(TARGET CompileShaders)
    #     add_dependencies(rtrlab_contract_tests_opengl CompileShaders)
    # endif()

    # glab_configure_test_target(rtrlab_contract_tests_opengl)

    # add_executable(rtrlab_integration_tests_opengl
    #     integration/opengl/TestShader.cpp
    #     integration/opengl/TestTexture.cpp
    #     integration/opengl/TestRenderResults.cpp
    #     integration/opengl/TestSceneRenderer.cpp
    #     Support/GLTestContext.cpp
    # )

    # if(TARGET CompileShaders)
    #     add_dependencies(rtrlab_integration_tests_opengl CompileShaders)
    # endif()

    # glab_configure_test_target(rtrlab_integration_tests_opengl)
endif()
