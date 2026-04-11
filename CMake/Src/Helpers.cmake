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

function(glab_copy_runtime_content target_name)
    add_custom_command(TARGET ${target_name} POST_BUILD
        # Copy source content (textures, config defaults, etc.)
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_SOURCE_DIR}/Project
                $<TARGET_FILE_DIR:${target_name}>/Project
    )

    if(EXISTS ${CMAKE_SOURCE_DIR}/Engine)
        add_custom_command(TARGET ${target_name} POST_BUILD
            # Copy engine-shipped defaults and shared resources.
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    ${CMAKE_SOURCE_DIR}/Engine
                    $<TARGET_FILE_DIR:${target_name}>/Engine
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
