include(${CMAKE_CURRENT_SOURCE_DIR}/CMake/SourceInventory.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/CMake/LocalHelpers.cmake)

# -----------------------------------------------------------------------------
# Core library
# -----------------------------------------------------------------------------
add_library(RTRLabCore STATIC
    ${GLAB_CORE_SOURCES}
    ${GLAB_CORE_HEADERS}
)

target_include_directories(RTRLabCore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(RTRLabCore PUBLIC
    glfw
    glm
    imgui
    magic_enum
    nlohmann_json
    spdlog
    stb
)

if(WIN32)
    target_link_libraries(RTRLabCore PUBLIC
        dbghelp
    )
endif()

if(UNIX AND NOT APPLE)
    target_link_libraries(RTRLabCore PUBLIC
        dl
    )
endif()

if(GLAB_BACKEND_OPENGL)
    target_link_libraries(RTRLabCore PUBLIC
        glad
        OpenGL::GL
    )

    target_compile_definitions(RTRLabCore PUBLIC
        GLAB_BACKEND_OPENGL
    )

    set_source_files_properties(
        ${CMAKE_CURRENT_SOURCE_DIR}/Core/App/Application.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Core/App/Window.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Core/Input/Input.cpp
        PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON
    )
endif()

if(GLAB_BACKEND_METAL)
    target_sources(RTRLabCore PRIVATE
        ${GLAB_METAL_SOURCES}
        ${GLAB_METAL_HEADERS}
    )

    set_source_files_properties(${GLAB_METAL_SOURCES}
        PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON
    )

    target_link_libraries(RTRLabCore PUBLIC
        "-framework AppKit"
        "-framework Metal"
        "-framework QuartzCore"
    )

    target_compile_definitions(RTRLabCore PUBLIC
        GLAB_BACKEND_METAL
    )
endif()

if(GLAB_BACKEND_VULKAN)
    target_compile_definitions(RTRLabCore PUBLIC
        GLAB_BACKEND_VULKAN
    )
endif()

target_compile_definitions(RTRLabCore PUBLIC
    GLM_ENABLE_EXPERIMENTAL
    $<$<CONFIG:Debug>:RTRLAB_CONFIG_DEBUG>
    $<$<CONFIG:RelWithDebInfo>:RTRLAB_CONFIG_RELWITHDEBINFO>
    $<$<CONFIG:Release>:RTRLAB_CONFIG_RELEASE>
    $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:GLAB_ROOT_DIR="${CMAKE_SOURCE_DIR}">
    $<$<CONFIG:Debug>:RTRLAB_LOG_MIN_LEVEL=0>
    $<$<CONFIG:RelWithDebInfo>:RTRLAB_LOG_MIN_LEVEL=1>
    $<$<CONFIG:Release>:RTRLAB_LOG_MIN_LEVEL=2>
)

if(MSVC)
    target_compile_definitions(RTRLabCore PUBLIC
        _CRT_SECURE_NO_WARNINGS
        NOMINMAX
    )
    target_compile_options(RTRLabCore PUBLIC
        /wd4005  # macro redefinition (APIENTRY: glad vs windows.h, both identical)
    )
endif()

if(APPLE)
    target_compile_definitions(RTRLabCore PUBLIC
        GL_SILENCE_DEPRECATION
    )
endif()

glab_enable_local_pch(RTRLabCore)
glab_configure_local_target(RTRLabCore)

# -----------------------------------------------------------------------------
# Main executable
# -----------------------------------------------------------------------------
add_executable(RTRLab
    main.cpp
)

target_link_libraries(RTRLab PRIVATE
    RTRLabCore
)

if(GLAB_ENABLE_PCH)
    target_precompile_headers(RTRLab REUSE_FROM RTRLabCore)
endif()

glab_copy_runtime_content(RTRLab)
glab_configure_local_target(RTRLab)

set_target_properties(RTRLab PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES
    ${GLAB_CORE_SOURCES}
    ${GLAB_CORE_HEADERS}
    ${GLAB_METAL_SOURCES}
    ${GLAB_METAL_HEADERS}
    main.cpp
)

# -----------------------------------------------------------------------------
# Tools
# -----------------------------------------------------------------------------
glab_add_tool_target(rtr_asset_cook Tools/AssetCookMain.cpp)
glab_add_tool_target(rtr_asset_pack Tools/AssetPackMain.cpp)

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

# -----------------------------------------------------------------------------
# Install rules
# -----------------------------------------------------------------------------
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
