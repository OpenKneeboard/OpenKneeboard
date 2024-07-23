set(GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/gen/$<CONFIG>/include")
set(GENERATED_HEADER_DIR "${GENERATED_INCLUDE_DIR}/OpenKneeboard/Shaders/gen")
make_directory("${GENERATED_HEADER_DIR}")


macro(add_shader_with_ms_compiler COMPILER TARGET ENTRYPOINT SOURCE)
    set(SHADER_OUTPUT "${GENERATED_HEADER_DIR}/${TARGET}.hpp")
    add_custom_command(
        OUTPUT "${SHADER_OUTPUT}"
        DEPENDS "${SOURCE}"
        COMMAND
        "${COMPILER}"
        "$<$<CONFIG:Debug>:-O0>"
        "$<$<CONFIG:Debug>:-Zi>"
        -Fh "${SHADER_OUTPUT}"
        -E ${ENTRYPOINT}
        "${CMAKE_CURRENT_SOURCE_DIR}/${SOURCE}"
        ${ARGN}
    )
    add_library("${TARGET}" INTERFACE "${SHADER_OUTPUT}")
    target_include_directories("${TARGET}" INTERFACE "${GENERATED_INCLUDE_DIR}")
endmacro()

macro(add_dxc_shader TARGET ENTRYPOINT SOURCE)
    add_shader_with_ms_compiler(ThirdParty::dxc "${TARGET}" "${ENTRYPOINT}" "${SOURCE}" ${ARGN})
endmacro()

macro(add_fxc_shader TARGET ENTRYPOINT SOURCE)
    add_shader_with_ms_compiler(System::fxc "${TARGET}" "${ENTRYPOINT}" "${SOURCE}" ${ARGN})
endmacro()

add_dxc_shader(
    OpenKneeboard-SpriteBatch-SPIRV-PS
    SpritePixelShader
    SpriteBatch.hlsl
    -spirv -T ps_6_0
)

add_dxc_shader(
    OpenKneeboard-SpriteBatch-SPIRV-VS
    SpriteVertexShader
    SpriteBatch.hlsl
    -spirv -T vs_6_0
)

add_library(OpenKneeboard-SpriteBatch-SPIRV INTERFACE)
target_link_libraries(
    OpenKneeboard-SpriteBatch-SPIRV
    INTERFACE
    OpenKneeboard-SpriteBatch-SPIRV-PS
    OpenKneeboard-SpriteBatch-SPIRV-VS
)
target_include_directories(
    OpenKneeboard-SpriteBatch-SPIRV
    INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# Shader Model 5.0 targets D3D11.0 and above
add_fxc_shader(
    OpenKneeboard-Viewer-D3D-VS
    ViewerVertexShader
    Viewer.hlsl
    -T vs_5_0
)
add_fxc_shader(
    OpenKneeboard-Viewer-D3D-PS
    ViewerPixelShader
    Viewer.hlsl
    -T ps_5_0
)
add_library(OpenKneeboard-Viewer-Shaders INTERFACE)
target_link_libraries(
    OpenKneeboard-Viewer-Shaders
    INTERFACE
    OpenKneeboard-Viewer-D3D-VS
    OpenKneeboard-Viewer-D3D-PS
)
target_include_directories(
    OpenKneeboard-Viewer-Shaders
    INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)