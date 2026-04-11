# -----------------------------------------------------------------------------
# Integration tests
# -----------------------------------------------------------------------------
set(GLAB_INTEGRATION_TEST_SOURCES
    Integration/TestCookPipeline.cpp
    Integration/TestLoggerConsoleSink.cpp
    Integration/TestMountBackend.cpp
    Integration/TestInputRecordingIO.cpp
    Integration/TestPackagePipeline.cpp
)

glab_add_test_executable(rtrlab_integration_tests "Integration." "integration;common"
    ${GLAB_INTEGRATION_TEST_SOURCES}
)
