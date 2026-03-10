include(GoogleTest)

add_executable(rtrlab_tests
    unit/TestTime.cpp
    unit/TestLayerStack.cpp
    unit/TestTransform.cpp
    unit/TestCamera.cpp
    unit/TestBuffers.cpp
)

target_link_libraries(rtrlab_tests
    PRIVATE
        RTRLabCore
        GTest::gtest_main
)

target_include_directories(rtrlab_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/support
        ${CMAKE_SOURCE_DIR}/src
)

if(GLAB_ENABLE_WARNINGS)
    glab_set_warnings(rtrlab_tests)
endif()

if(GLAB_ENABLE_ASAN)
    glab_enable_sanitizers(rtrlab_tests)
endif()

gtest_discover_tests(rtrlab_tests)