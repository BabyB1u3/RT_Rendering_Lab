option(GLAB_COMPILE_SHADERS "Compile shaders at build time" ON)

# ============================================================================
# Slang shader compilation (.slang -> GLSL 460 per stage)
# ============================================================================
# Each .slang file is compiled twice: once for vertexMain, once for fragmentMain.
# Output goes to ${CMAKE_BINARY_DIR}/shaders/glsl/<Name>.<stage>.glsl
#
# Future: add more backend targets here when additional render backends land.
# ============================================================================

function(glab_compile_shaders)
    if(NOT GLAB_COMPILE_SHADERS)
        return()
    endif()

    if(NOT DEFINED SLANGC OR NOT EXISTS "${SLANGC}")
        message(FATAL_ERROR
            "SLANGC not set or not found at '${SLANGC}'.\n"
            "Include cmake/SetupSlang.cmake before calling this function.")
    endif()

    set(SHADER_SOURCE_DIR "${CMAKE_SOURCE_DIR}/assets/shaders")
    set(SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/shaders/glsl")

    # Shader modules (dependencies for -I search path)
    set(SLANG_MODULE_DIR "${SHADER_SOURCE_DIR}/modules")

    set(SLANG_SHADERS
        TexturePreview
        ShadowDepth
        ForwardLit
    )

    set(ALL_OUTPUTS "")

    foreach(SHADER_NAME ${SLANG_SHADERS})
        set(INPUT "${SHADER_SOURCE_DIR}/${SHADER_NAME}.slang")

        # Collect module dependencies for rebuild tracking
        file(GLOB MODULE_DEPS "${SLANG_MODULE_DIR}/*.slang")

        # --- Vertex stage ---
        set(VERT_OUTPUT "${SHADER_OUTPUT_DIR}/${SHADER_NAME}.vert.glsl")
        add_custom_command(
            OUTPUT "${VERT_OUTPUT}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${SHADER_OUTPUT_DIR}"
            COMMAND "${SLANGC}" "${INPUT}"
                -target glsl
                -profile glsl_460
                -matrix-layout-column-major
                -stage vertex -entry vertexMain
                -I "${SLANG_MODULE_DIR}"
                -o "${VERT_OUTPUT}"
            DEPENDS "${INPUT}" ${MODULE_DEPS}
            COMMENT "Slang -> GLSL vertex: ${SHADER_NAME}"
            VERBATIM
        )

        # --- Fragment stage ---
        set(FRAG_OUTPUT "${SHADER_OUTPUT_DIR}/${SHADER_NAME}.frag.glsl")
        add_custom_command(
            OUTPUT "${FRAG_OUTPUT}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${SHADER_OUTPUT_DIR}"
            COMMAND "${SLANGC}" "${INPUT}"
                -target glsl
                -profile glsl_460
                -matrix-layout-column-major
                -stage fragment -entry fragmentMain
                -I "${SLANG_MODULE_DIR}"
                -o "${FRAG_OUTPUT}"
            DEPENDS "${INPUT}" ${MODULE_DEPS}
            COMMENT "Slang -> GLSL fragment: ${SHADER_NAME}"
            VERBATIM
        )

        list(APPEND ALL_OUTPUTS "${VERT_OUTPUT}" "${FRAG_OUTPUT}")
    endforeach()

    add_custom_target(CompileShaders ALL DEPENDS ${ALL_OUTPUTS})
endfunction()
