option(GLAB_COMPILE_SHADERS "Compile shaders at build time" ON)
option(GLAB_SHADER_TARGET_GLSL  "Compile Slang shaders to GLSL 460"  ON)
option(GLAB_SHADER_TARGET_SPIRV "Compile Slang shaders to SPIR-V"   OFF)
option(GLAB_SHADER_TARGET_METAL "Compile Slang shaders to Metal Shading Language" OFF)

# ============================================================================
# Slang shader compilation (.slang -> backend-specific output per stage)
# ============================================================================
# Each .slang file is compiled per enabled target, once for vertexMain and
# once for fragmentMain (Metal emits both stages into a single file).
#
# Output layout:
#   ${CMAKE_BINARY_DIR}/shaders/glsl/   <Name>.vert.glsl / .frag.glsl
#   ${CMAKE_BINARY_DIR}/shaders/spirv/  <Name>.vert.spv  / .frag.spv
#   ${CMAKE_BINARY_DIR}/shaders/metal/  <Name>.metal
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
    set(SHADER_BASE_DIR   "${CMAKE_BINARY_DIR}/shaders")

    # Shader modules (dependencies for -I search path)
    set(SLANG_MODULE_DIR "${SHADER_SOURCE_DIR}/modules")

    set(SLANG_SHADERS
        TexturePreview
        ShadowDepth
        ForwardLit
        FlatColor
        UnlitTextured
        UnlitTransformed
    )

    if(GLAB_BACKEND_METAL)
        set(GLAB_SHADER_TARGET_METAL ON)
        set(GLAB_SHADER_TARGET_GLSL OFF)
        set(GLAB_SHADER_TARGET_SPIRV OFF)
    endif()

    if(GLAB_BACKEND_OPENGL)
        set(GLAB_SHADER_TARGET_GLSL ON)
        set(GLAB_SHADER_TARGET_METAL OFF)
        set(GLAB_SHADER_TARGET_SPIRV OFF)
    endif()

    set(ALL_OUTPUTS "")

    foreach(SHADER_NAME ${SLANG_SHADERS})
        set(INPUT "${SHADER_SOURCE_DIR}/${SHADER_NAME}.slang")

        # Collect module dependencies for rebuild tracking
        file(GLOB MODULE_DEPS "${SLANG_MODULE_DIR}/*.slang")

        # ── GLSL target ──
        if(GLAB_SHADER_TARGET_GLSL)
            set(GLSL_DIR "${SHADER_BASE_DIR}/glsl")

            set(VERT_OUTPUT "${GLSL_DIR}/${SHADER_NAME}.vert.glsl")
            add_custom_command(
                OUTPUT "${VERT_OUTPUT}"
                COMMAND "${CMAKE_COMMAND}" -E make_directory "${GLSL_DIR}"
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

            set(FRAG_OUTPUT "${GLSL_DIR}/${SHADER_NAME}.frag.glsl")
            add_custom_command(
                OUTPUT "${FRAG_OUTPUT}"
                COMMAND "${CMAKE_COMMAND}" -E make_directory "${GLSL_DIR}"
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
        endif()

        # ── SPIR-V target ──
        if(GLAB_SHADER_TARGET_SPIRV)
            set(SPIRV_DIR "${SHADER_BASE_DIR}/spirv")

            set(VERT_OUTPUT "${SPIRV_DIR}/${SHADER_NAME}.vert.spv")
            add_custom_command(
                OUTPUT "${VERT_OUTPUT}"
                COMMAND "${CMAKE_COMMAND}" -E make_directory "${SPIRV_DIR}"
                COMMAND "${SLANGC}" "${INPUT}"
                    -target spirv
                    -emit-spirv-directly
                    -matrix-layout-column-major
                    -stage vertex -entry vertexMain
                    -I "${SLANG_MODULE_DIR}"
                    -o "${VERT_OUTPUT}"
                DEPENDS "${INPUT}" ${MODULE_DEPS}
                COMMENT "Slang -> SPIR-V vertex: ${SHADER_NAME}"
                VERBATIM
            )

            set(FRAG_OUTPUT "${SPIRV_DIR}/${SHADER_NAME}.frag.spv")
            add_custom_command(
                OUTPUT "${FRAG_OUTPUT}"
                COMMAND "${CMAKE_COMMAND}" -E make_directory "${SPIRV_DIR}"
                COMMAND "${SLANGC}" "${INPUT}"
                    -target spirv
                    -emit-spirv-directly
                    -matrix-layout-column-major
                    -stage fragment -entry fragmentMain
                    -I "${SLANG_MODULE_DIR}"
                    -o "${FRAG_OUTPUT}"
                DEPENDS "${INPUT}" ${MODULE_DEPS}
                COMMENT "Slang -> SPIR-V fragment: ${SHADER_NAME}"
                VERBATIM
            )

            list(APPEND ALL_OUTPUTS "${VERT_OUTPUT}" "${FRAG_OUTPUT}")
        endif()

        # ── Metal target ──
        if(GLAB_SHADER_TARGET_METAL)
            set(METAL_DIR "${SHADER_BASE_DIR}/metal")

            # Metal emits both stages into a single file
            set(MTL_OUTPUT "${METAL_DIR}/${SHADER_NAME}.metal")
            add_custom_command(
                OUTPUT "${MTL_OUTPUT}"
                COMMAND "${CMAKE_COMMAND}" -E make_directory "${METAL_DIR}"
                COMMAND "${SLANGC}" "${INPUT}"
                    -target metal
                    -matrix-layout-column-major
                    -I "${SLANG_MODULE_DIR}"
                    -o "${MTL_OUTPUT}"
                DEPENDS "${INPUT}" ${MODULE_DEPS}
                COMMENT "Slang -> Metal: ${SHADER_NAME}"
                VERBATIM
            )

            list(APPEND ALL_OUTPUTS "${MTL_OUTPUT}")
        endif()

    endforeach()

    add_custom_target(CompileShaders ALL DEPENDS ${ALL_OUTPUTS})
endfunction()
