# -----------------------------------------------------------------------------
# Integration tests
# -----------------------------------------------------------------------------
set(GLAB_INTEGRATION_TEST_SOURCES
    Common/Integration/Core/Diagnostics/TestLoggerConsoleSink.cpp
    Common/Integration/Core/Input/TestInputRecordingIO.cpp
    Common/Integration/Core/Resource/TestCookPipeline.cpp
    Common/Integration/Core/Resource/TestMountBackend.cpp
    Common/Integration/Core/Resource/TestPackagePipeline.cpp
)

glab_add_test_executable(rtrlab_integration_tests "Integration." "integration;common"
    ${GLAB_INTEGRATION_TEST_SOURCES}
)
