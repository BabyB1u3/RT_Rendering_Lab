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
