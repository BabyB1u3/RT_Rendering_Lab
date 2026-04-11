set(GLAB_PACKAGE_CONTENT_COOKED_DIR "${CMAKE_BINARY_DIR}/Packaging/$<CONFIG>/Cooked")
set(GLAB_STAGE_RUNTIME_DIR "${CMAKE_BINARY_DIR}/Stage/$<CONFIG>")
set(GLAB_STAGE_RUNTIME_NAME "$<TARGET_FILE_NAME:RTRLab>")

add_custom_target(rtrlab_package_content
    COMMAND ${CMAKE_COMMAND} -E make_directory "${GLAB_PACKAGE_CONTENT_COOKED_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${GLAB_STAGE_RUNTIME_DIR}"
    COMMAND $<TARGET_FILE:rtr_asset_cook>
            --root "${CMAKE_SOURCE_DIR}"
            --out "${GLAB_PACKAGE_CONTENT_COOKED_DIR}"
    COMMAND $<TARGET_FILE:rtr_asset_pack>
            --source "${GLAB_PACKAGE_CONTENT_COOKED_DIR}"
            --out "${GLAB_STAGE_RUNTIME_DIR}"
    DEPENDS
        rtr_asset_cook
        rtr_asset_pack
    COMMENT "Packaging runtime content into ${GLAB_STAGE_RUNTIME_DIR}"
)

add_custom_target(rtrlab_stage_runtime
    COMMAND ${CMAKE_COMMAND} -E make_directory "${GLAB_STAGE_RUNTIME_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:RTRLab>
            "${GLAB_STAGE_RUNTIME_DIR}/$<TARGET_FILE_NAME:RTRLab>"
    DEPENDS
        RTRLab
        rtrlab_package_content
    COMMENT "Staging runtime into ${GLAB_STAGE_RUNTIME_DIR}"
)
