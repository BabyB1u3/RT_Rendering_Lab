option(GLAB_COMPILE_SHADERS "Compile GLSL shaders to SPIR-V" ON)

function(glab_compile_shaders)
    if(NOT GLAB_COMPILE_SHADERS)
        return()
    endif()

    if(NOT TARGET ${GLSLANG_TARGET})
        message(FATAL_ERROR
            "GLSLANG_TARGET '${GLSLANG_TARGET}' not found.\n"
            "Check vendor/glslang setup or set GLAB_COMPILE_SHADERS=OFF.")
    endif()

    set(SHADER_SOURCE_DIR "${CMAKE_SOURCE_DIR}/assets/shaders")
    set(SHADER_SOURCES
        ForwardLit.vert   ForwardLit.frag
        ShadowDepth.vert  ShadowDepth.frag
        TexturePreview.vert TexturePreview.frag
    )

    set(SPV_OUTPUTS "")
    foreach(SHADER ${SHADER_SOURCES})
        set(INPUT  "${SHADER_SOURCE_DIR}/${SHADER}")
        set(OUTPUT "${SHADER_SOURCE_DIR}/${SHADER}.spv")

        add_custom_command(
            OUTPUT ${OUTPUT}
            COMMAND $<TARGET_FILE:${GLSLANG_TARGET}> -G --auto-map-locations --auto-map-bindings -o ${OUTPUT} ${INPUT}
            DEPENDS ${INPUT} ${GLSLANG_TARGET}
            COMMENT "SPIR-V: ${SHADER}"
            VERBATIM
        )

        list(APPEND SPV_OUTPUTS ${OUTPUT})
    endforeach()

    add_custom_target(CompileShaders DEPENDS ${SPV_OUTPUTS})
endfunction()
