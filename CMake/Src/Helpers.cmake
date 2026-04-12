# -----------------------------------------------------------------------------
# Local helpers
# -----------------------------------------------------------------------------
function(glab_enable_local_pch target_name)
    if(GLAB_ENABLE_PCH)
        target_precompile_headers(${target_name} PRIVATE
            "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/pch.h>"
        )
    endif()
endfunction()

function(glab_add_tool_target target_name source_file)
    add_executable(${target_name}
        ${source_file}
    )

    target_link_libraries(${target_name} PRIVATE
        RTRLabCore
    )

    if(GLAB_ENABLE_PCH)
        target_precompile_headers(${target_name} REUSE_FROM RTRLabCore)
    endif()

    glab_configure_local_target(${target_name})

    set_target_properties(${target_name} PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
endfunction()
