include(GoogleTest)

function(glab_configure_test_target target_name)
    target_link_libraries(${target_name}
        PRIVATE
            RTRLabCore
            GTest::gtest_main
    )

    target_include_directories(${target_name}
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/support
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
    unit/TestTime.cpp
    unit/TestLayerStack.cpp
    unit/TestTransform.cpp
    unit/TestCamera.cpp
    unit/TestDefaultCameraController.cpp
    unit/TestSpecifications.cpp
    unit/TestInputModifiers.cpp
    unit/TestInputTriggers.cpp
    unit/TestInputContextStack.cpp
    unit/TestInput.cpp
    unit/TestPropertyTree.cpp
)

glab_configure_test_target(rtrlab_unit_tests)

add_executable(rtrlab_contract_tests
    contract/core/TestDiagnostics.cpp
    contract/core/TestEventBus.cpp
    contract/core/TestFileSystem.cpp
    contract/core/TestLayerStackLifecycle.cpp
)

glab_configure_test_target(rtrlab_contract_tests)

if(GLAB_BACKEND_OPENGL)
    # set_source_files_properties(
    #     ${CMAKE_CURRENT_SOURCE_DIR}/support/GLTestContext.cpp
    #     PROPERTIES
    #         SKIP_UNITY_BUILD_INCLUSION ON
    # )

    # add_executable(rtrlab_contract_tests_opengl
    #     contract/opengl/TestFramebuffer.cpp
    #     contract/opengl/TestRenderPasses.cpp
    #     contract/opengl/TestRenderTarget.cpp
    #     support/GLTestContext.cpp
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
    #     support/GLTestContext.cpp
    # )

    # if(TARGET CompileShaders)
    #     add_dependencies(rtrlab_integration_tests_opengl CompileShaders)
    # endif()

    # glab_configure_test_target(rtrlab_integration_tests_opengl)
endif()
