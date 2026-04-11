install(CODE "
    set(stage_config \"\${CMAKE_INSTALL_CONFIG_NAME}\")
    if(NOT stage_config)
        set(stage_config \"${CMAKE_BUILD_TYPE}\")
    endif()

    set(stage_dir \"${CMAKE_BINARY_DIR}/Stage/\${stage_config}\")
    set(stage_runtime \"${GLAB_STAGE_SHIPPING_RUNTIME_NAME}\")
    set(stage_pak \"\${stage_dir}/Game.rtrpak\")
    set(stage_runtime_path \"\${stage_dir}/\${stage_runtime}\")

    if(NOT EXISTS \"\${stage_pak}\")
        message(FATAL_ERROR
            \"Missing staged shipping assets at '\${stage_pak}'. Build target 'rtrlab_stage_shipping' before running install.\")
    endif()

    if(NOT EXISTS \"\${stage_runtime_path}\")
        message(FATAL_ERROR
            \"Missing staged runtime at '\${stage_runtime_path}'. Build target 'rtrlab_stage_shipping' before running install.\")
    endif()

    file(INSTALL DESTINATION \"${CMAKE_INSTALL_PREFIX}\" TYPE EXECUTABLE
         FILES \"\${stage_runtime_path}\")
    file(INSTALL DESTINATION \"${CMAKE_INSTALL_PREFIX}\" TYPE FILE
         FILES \"\${stage_pak}\")

    foreach(stage_overlay_dir IN ITEMS DLC Patches Mods)
        if(EXISTS \"\${stage_dir}/\${stage_overlay_dir}\")
            file(INSTALL DESTINATION \"${CMAKE_INSTALL_PREFIX}\" TYPE DIRECTORY
                 FILES \"\${stage_dir}/\${stage_overlay_dir}\")
        endif()
    endforeach()
")
