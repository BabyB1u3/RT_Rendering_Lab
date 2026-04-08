include(GoogleTest)

function(glab_add_test_runtime_content_dependency target_name)
    set(runtime_content_target ${target_name}_runtime_content)

    if(EXISTS ${CMAKE_SOURCE_DIR}/EngineContent)
        add_custom_target(${runtime_content_target}
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    ${CMAKE_SOURCE_DIR}/Content
                    $<TARGET_FILE_DIR:${target_name}>/Content
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    ${CMAKE_SOURCE_DIR}/EngineContent
                    $<TARGET_FILE_DIR:${target_name}>/EngineContent
            COMMENT "Copying runtime Content for ${target_name}"
        )
    else()
        add_custom_target(${runtime_content_target}
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    ${CMAKE_SOURCE_DIR}/Content
                    $<TARGET_FILE_DIR:${target_name}>/Content
            COMMENT "Copying runtime Content for ${target_name}"
        )
    endif()

    add_dependencies(${target_name} ${runtime_content_target})
endfunction()

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

    glab_add_test_runtime_content_dependency(${target_name})
    glab_configure_local_target(${target_name})
    gtest_discover_tests(${target_name})
endfunction()

function(glab_add_test_executable target_name)
    add_executable(${target_name}
        ${ARGN}
    )

    glab_configure_test_target(${target_name})
endfunction()

# -----------------------------------------------------------------------------
# Unit tests
# -----------------------------------------------------------------------------
glab_add_test_executable(rtrlab_unit_tests
    Unit/TestBuiltinTraits.cpp
    Unit/TestCamera.cpp
    Unit/TestCookedCatalog.cpp
    Unit/TestDefaultCameraController.cpp
    Unit/TestFileSystemVirtualPaths.cpp
    Unit/TestInput.cpp
    Unit/TestInputAction.cpp
    Unit/TestInputContextStack.cpp
    Unit/TestInputDeviceManager.cpp
    Unit/TestInputModifiers.cpp
    Unit/TestInputNames.cpp
    Unit/TestInputTriggers.cpp
    Unit/TestJsonBackend.cpp
    Unit/TestLayerStack.cpp
    Unit/TestMountBackend.cpp
    Unit/TestPakArchive.cpp
    Unit/TestPropertyTree.cpp
    Unit/TestSourceCatalog.cpp
    Unit/TestSpecifications.cpp
    Unit/TestTime.cpp
    Unit/TestTransform.cpp
)

# -----------------------------------------------------------------------------
# Contract tests
# -----------------------------------------------------------------------------
glab_add_test_executable(rtrlab_contract_tests
    Contract/Core/TestDiagnosticsCore.cpp
    Contract/Core/TestEventBus.cpp
    Contract/Core/TestFileSystem.cpp
    Contract/Core/TestImGuiConsoleSink.cpp
    Contract/Core/TestLayerStackLifecycle.cpp
    Contract/Core/TestLogCategories.cpp
    Contract/Core/TestLogger.cpp
    Contract/Core/TestSerializationFileIO.cpp
)

glab_add_test_executable(rtrlab_contract_tests_log_min_level
    Contract/Core/TestDiagnosticsMinLevel.cpp
)

if(GLAB_BACKEND_OPENGL)
    # set_source_files_properties(
    #     ${CMAKE_CURRENT_SOURCE_DIR}/Support/GLTestContext.cpp
    #     PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON
    # )

    # glab_add_test_executable(rtrlab_contract_tests_opengl
    #     Contract/opengl/TestFramebuffer.cpp
    #     Contract/opengl/TestRenderPasses.cpp
    #     Contract/opengl/TestRenderTarget.cpp
    #     Support/GLTestContext.cpp
    # )

    # if(TARGET CompileShaders)
    #     add_dependencies(rtrlab_contract_tests_opengl CompileShaders)
    # endif()

    # glab_add_test_executable(rtrlab_integration_tests_opengl
    #     integration/opengl/TestRenderResults.cpp
    #     integration/opengl/TestSceneRenderer.cpp
    #     integration/opengl/TestShader.cpp
    #     integration/opengl/TestTexture.cpp
    #     Support/GLTestContext.cpp
    # )

    # if(TARGET CompileShaders)
    #     add_dependencies(rtrlab_integration_tests_opengl CompileShaders)
    # endif()
endif()
