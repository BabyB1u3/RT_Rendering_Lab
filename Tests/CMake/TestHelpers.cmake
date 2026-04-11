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
