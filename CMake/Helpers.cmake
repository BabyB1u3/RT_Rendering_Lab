# -----------------------------------------------------------------------------
# Shared helper functions
# -----------------------------------------------------------------------------
function(glab_set_warnings target_name)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

function(glab_enable_sanitizers target_name)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /fsanitize=address)
    else()
        target_compile_options(${target_name} PRIVATE -fsanitize=address -fno-omit-frame-pointer)
        target_link_options(${target_name} PRIVATE -fsanitize=address)
    endif()
endfunction()

function(glab_enable_parallel_compilation target_name)
    if(MSVC AND GLAB_ENABLE_MSVC_MP)
        target_compile_options(${target_name} PRIVATE /MP)
    endif()
endfunction()

function(glab_enable_release_symbols target_name)
    if(NOT MSVC OR NOT GLAB_ENABLE_RELEASE_SYMBOLS)
        return()
    endif()

    target_compile_options(${target_name} PRIVATE
        $<$<CONFIG:Release>:/Zi>
    )

    get_target_property(target_type ${target_name} TYPE)
    if(target_type STREQUAL "EXECUTABLE" OR
       target_type STREQUAL "SHARED_LIBRARY" OR
       target_type STREQUAL "MODULE_LIBRARY")
        target_link_options(${target_name} PRIVATE
            $<$<CONFIG:Release>:/DEBUG>
        )
    endif()
endfunction()

function(glab_enable_build_speedups target_name)
    glab_enable_parallel_compilation(${target_name})

    if(GLAB_ENABLE_UNITY_BUILD)
        set_target_properties(${target_name} PROPERTIES
            UNITY_BUILD ON
            UNITY_BUILD_BATCH_SIZE 8
        )
    endif()
endfunction()

function(glab_configure_local_target target_name)
    glab_enable_release_symbols(${target_name})
    glab_enable_build_speedups(${target_name})

    if(GLAB_ENABLE_WARNINGS)
        glab_set_warnings(${target_name})
    endif()

    if(GLAB_ENABLE_ASAN)
        glab_enable_sanitizers(${target_name})
    endif()
endfunction()
