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

    find_package(Python3 REQUIRED COMPONENTS Interpreter)

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
        BasicLit
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
        #
        # Matrix convention: -matrix-layout-column-major
        #
        #   This flag tells Slang that the application (GLM) sends matrices
        #   in column-major order.  Slang always emits `layout(row_major)`
        #   in the generated GLSL so the GPU reads the buffer bytes as-is
        #   (no driver-side transpose).  Because column-major data declared
        #   as row_major makes the GPU see M^T, Slang compensates by
        #   flipping every mul() operand order in the generated GLSL:
        #
        #     Slang source          Compiled GLSL         Actual result
        #     mul(M, v)      →      v * M_var       →     M_glm * v   ✓
        #     mul(v, M)      →      M_var * v       →     v * M_glm   ✓
        #
        #   Rule for shader authors: write mul(M, v) for standard matrix
        #   transforms (e.g. mul(u_MVP, float4(pos, 1.0))).  Do NOT try to
        #   match the GLSL output order — Slang handles the compensation.
        #
        if(GLAB_SHADER_TARGET_GLSL)
            set(GLSL_DIR "${SHADER_BASE_DIR}/glsl")
            set(GLSL_FLATTEN_SCRIPT "${CMAKE_SOURCE_DIR}/tools/flatten_glsl_bindings.py")

            set(VERT_RAW_OUTPUT "${GLSL_DIR}/${SHADER_NAME}.vert.raw.glsl")
            set(VERT_REFLECT_OUTPUT "${GLSL_DIR}/${SHADER_NAME}.vert.reflect.json")
            set(VERT_OUTPUT "${GLSL_DIR}/${SHADER_NAME}.vert.glsl")
            add_custom_command(
                OUTPUT "${VERT_RAW_OUTPUT}" "${VERT_REFLECT_OUTPUT}"
                COMMAND "${CMAKE_COMMAND}" -E make_directory "${GLSL_DIR}"
                COMMAND "${SLANGC}" "${INPUT}"
                    -target glsl
                    -profile glsl_460
                    -matrix-layout-column-major
                    -stage vertex -entry vertexMain
                    -I "${SLANG_MODULE_DIR}"
                    -reflection-json "${VERT_REFLECT_OUTPUT}"
                    -o "${VERT_RAW_OUTPUT}"
                DEPENDS "${INPUT}" ${MODULE_DEPS}
                COMMENT "Slang -> GLSL vertex: ${SHADER_NAME}"
                VERBATIM
            )

            add_custom_command(
                OUTPUT "${VERT_OUTPUT}"
                COMMAND "${Python3_EXECUTABLE}" "${GLSL_FLATTEN_SCRIPT}"
                    --input "${VERT_RAW_OUTPUT}"
                    --output "${VERT_OUTPUT}"
                DEPENDS "${VERT_RAW_OUTPUT}" "${VERT_REFLECT_OUTPUT}" "${GLSL_FLATTEN_SCRIPT}"
                COMMENT "Flatten GLSL bindings for OpenGL vertex stage: ${SHADER_NAME}"
                VERBATIM
            )

            set(FRAG_RAW_OUTPUT "${GLSL_DIR}/${SHADER_NAME}.frag.raw.glsl")
            set(FRAG_REFLECT_OUTPUT "${GLSL_DIR}/${SHADER_NAME}.frag.reflect.json")
            set(FRAG_OUTPUT "${GLSL_DIR}/${SHADER_NAME}.frag.glsl")
            add_custom_command(
                OUTPUT "${FRAG_RAW_OUTPUT}" "${FRAG_REFLECT_OUTPUT}"
                COMMAND "${CMAKE_COMMAND}" -E make_directory "${GLSL_DIR}"
                COMMAND "${SLANGC}" "${INPUT}"
                    -target glsl
                    -profile glsl_460
                    -matrix-layout-column-major
                    -stage fragment -entry fragmentMain
                    -I "${SLANG_MODULE_DIR}"
                    -reflection-json "${FRAG_REFLECT_OUTPUT}"
                    -o "${FRAG_RAW_OUTPUT}"
                DEPENDS "${INPUT}" ${MODULE_DEPS}
                COMMENT "Slang -> GLSL fragment: ${SHADER_NAME}"
                VERBATIM
            )

            add_custom_command(
                OUTPUT "${FRAG_OUTPUT}"
                COMMAND "${Python3_EXECUTABLE}" "${GLSL_FLATTEN_SCRIPT}"
                    --input "${FRAG_RAW_OUTPUT}"
                    --output "${FRAG_OUTPUT}"
                DEPENDS "${FRAG_RAW_OUTPUT}" "${FRAG_REFLECT_OUTPUT}" "${GLSL_FLATTEN_SCRIPT}"
                COMMENT "Flatten GLSL bindings for OpenGL fragment stage: ${SHADER_NAME}"
                VERBATIM
            )

            list(APPEND ALL_OUTPUTS
                "${VERT_OUTPUT}"
                "${VERT_REFLECT_OUTPUT}"
                "${FRAG_OUTPUT}"
                "${FRAG_REFLECT_OUTPUT}"
            )
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

            set(METAL_REFLECT_ENRICH_SCRIPT "${CMAKE_SOURCE_DIR}/tools/enrich_metal_reflection.py")

            # Metal emits both stages into a single file
            set(MTL_OUTPUT "${METAL_DIR}/${SHADER_NAME}.metal")
            set(REFLECT_OUTPUT "${METAL_DIR}/${SHADER_NAME}.reflect.json")
            add_custom_command(
                OUTPUT "${MTL_OUTPUT}" "${REFLECT_OUTPUT}"
                COMMAND "${CMAKE_COMMAND}" -E make_directory "${METAL_DIR}"
                COMMAND "${SLANGC}" "${INPUT}"
                    -target metal
                    -matrix-layout-column-major
                    -I "${SLANG_MODULE_DIR}"
                    -reflection-json "${REFLECT_OUTPUT}"
                    -o "${MTL_OUTPUT}"
                COMMAND "${Python3_EXECUTABLE}" "${METAL_REFLECT_ENRICH_SCRIPT}"
                    --source "${INPUT}"
                    --reflection "${REFLECT_OUTPUT}"
                DEPENDS "${INPUT}" ${MODULE_DEPS} "${METAL_REFLECT_ENRICH_SCRIPT}"
                COMMENT "Slang -> Metal: ${SHADER_NAME}"
                VERBATIM
            )

            list(APPEND ALL_OUTPUTS "${MTL_OUTPUT}" "${REFLECT_OUTPUT}")
        endif()

    endforeach()

    add_custom_target(CompileShaders ALL DEPENDS ${ALL_OUTPUTS})
endfunction()
