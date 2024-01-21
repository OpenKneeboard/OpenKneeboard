set(GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/gen/$<CONFIG>/include")
set(GENERATED_HEADER_DIR "${GENERATED_INCLUDE_DIR}/OpenKneeboard/Shaders/gen")
make_directory("${GENERATED_HEADER_DIR}")

macro(add_shader TARGET ENTRYPOINT SOURCE)
    set(SHADER_OUTPUT "${GENERATED_HEADER_DIR}/${TARGET}.h")
    add_custom_command(
        OUTPUT "${SHADER_OUTPUT}"
        DEPENDS "${SOURCE}"
        COMMAND
        ThirdParty::dxc
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

add_shader(
    OpenKneeboard-SpriteBatch-SPIRV-PS
    SpritePixelShader
    SpriteBatch.hlsl
    -T ps_6_0
)

add_shader(
    OpenKneeboard-SpriteBatch-SPIRV-VS
    SpriteVertexShader
    SpriteBatch.hlsl
    -T vs_6_0
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