include(GoogleTest)

add_executable(rtrlab_unit_tests
    unit/TestTime.cpp
    unit/TestLayerStack.cpp
    unit/TestTransform.cpp
    unit/TestCamera.cpp
    unit/TestBuffers.cpp
    unit/TestDefaultCameraController.cpp
    unit/TestSpecifications.cpp
)

target_link_libraries(rtrlab_unit_tests
    PRIVATE
        RTRLabCore
        GTest::gtest_main
)

target_include_directories(rtrlab_unit_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/support
        ${CMAKE_SOURCE_DIR}/src
)

if(GLAB_ENABLE_PCH)
    target_precompile_headers(rtrlab_unit_tests PRIVATE
        "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/pch.h>"
    )
endif()

add_executable(rtrlab_integration_tests
    integration/TestShader.cpp
    integration/TestTexture.cpp
    integration/TestFramebuffer.cpp
    integration/TestRenderTarget.cpp

    support/GLTestContext.cpp
)

set_source_files_properties(
    ${CMAKE_CURRENT_SOURCE_DIR}/support/GLTestContext.cpp
    PROPERTIES
        SKIP_UNITY_BUILD_INCLUSION ON
)

target_link_libraries(rtrlab_integration_tests
    PRIVATE
        RTRLabCore
        GTest::gtest_main
)

if(TARGET CompileShaders)
    add_dependencies(rtrlab_integration_tests CompileShaders)
endif()

target_include_directories(rtrlab_integration_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/support
        ${CMAKE_SOURCE_DIR}/src
)

if(GLAB_ENABLE_PCH)
    target_precompile_headers(rtrlab_integration_tests PRIVATE
        "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/pch.h>"
    )
endif()

glab_enable_build_speedups(rtrlab_unit_tests)
glab_enable_build_speedups(rtrlab_integration_tests)

if(GLAB_ENABLE_WARNINGS)
    glab_set_warnings(rtrlab_unit_tests)
    glab_set_warnings(rtrlab_integration_tests)
endif()

if(GLAB_ENABLE_ASAN)
    glab_enable_sanitizers(rtrlab_unit_tests)
    glab_enable_sanitizers(rtrlab_integration_tests)
endif()

gtest_discover_tests(rtrlab_unit_tests)
gtest_discover_tests(rtrlab_integration_tests)
