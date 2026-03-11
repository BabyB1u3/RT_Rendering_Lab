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

add_executable(rtrlab_integration_tests
    integration/TestShader.cpp
    integration/TestTexture.cpp

    support/GLTestContext.cpp
)

target_link_libraries(rtrlab_integration_tests
    PRIVATE
        RTRLabCore
        GTest::gtest_main
)

target_include_directories(rtrlab_integration_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/support
        ${CMAKE_SOURCE_DIR}/src
)

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
