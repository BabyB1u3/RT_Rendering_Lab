set(GLAB_STAGE_SHIPPING_COOKED_DIR "${CMAKE_BINARY_DIR}/Packaging/$<CONFIG>/Cooked")
set(GLAB_STAGE_SHIPPING_DIR "${CMAKE_BINARY_DIR}/Stage/$<CONFIG>")
set(GLAB_STAGE_SHIPPING_RUNTIME_NAME "$<TARGET_FILE_NAME:RTRLab>")

add_custom_target(rtrlab_stage_shipping_assets
    COMMAND ${CMAKE_COMMAND} -E make_directory "${GLAB_STAGE_SHIPPING_COOKED_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${GLAB_STAGE_SHIPPING_DIR}"
    COMMAND $<TARGET_FILE:rtr_asset_cook>
            --root "${CMAKE_SOURCE_DIR}"
            --out "${GLAB_STAGE_SHIPPING_COOKED_DIR}"
    COMMAND $<TARGET_FILE:rtr_asset_pack>
            --source "${GLAB_STAGE_SHIPPING_COOKED_DIR}"
            --out "${GLAB_STAGE_SHIPPING_DIR}"
    DEPENDS
        rtr_asset_cook
        rtr_asset_pack
    COMMENT "Cooking and packaging shipping assets into ${GLAB_STAGE_SHIPPING_DIR}"
)

add_custom_target(rtrlab_stage_shipping
    COMMAND ${CMAKE_COMMAND} -E make_directory "${GLAB_STAGE_SHIPPING_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:RTRLab>
            "${GLAB_STAGE_SHIPPING_DIR}/$<TARGET_FILE_NAME:RTRLab>"
    DEPENDS
        RTRLab
        rtrlab_stage_shipping_assets
    COMMENT "Staging shipping runtime into ${GLAB_STAGE_SHIPPING_DIR}"
)
