file(COPY invalid.OpenKneeboardPlugin DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")

set(TEST_PLUGIN_PATH "${CMAKE_CURRENT_BINARY_DIR}/Test.OpenKneeboardPlugin")
add_custom_command(
    OUTPUT "${TEST_PLUGIN_PATH}"
    COMMAND
    "${CMAKE_COMMAND}" -E tar
    cf "${TEST_PLUGIN_PATH}" --format=zip
    --
    v1.json
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/v1.json"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)
add_custom_target(TestPlugin ALL DEPENDS "${TEST_PLUGIN_PATH}")