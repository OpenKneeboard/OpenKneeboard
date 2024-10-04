set(
    TS_CPP_CLASSES
    DeveloperToolsSettingsPage
)
list(TRANSFORM TS_CPP_CLASSES APPEND ".cpp" OUTPUT_VARIABLE TS_CPP_SOURCES)
list(TRANSFORM TS_CPP_CLASSES APPEND ".hpp" OUTPUT_VARIABLE TS_CPP_HEADERS)
list(TRANSFORM TS_CPP_HEADERS PREPEND "include/OpenKneeboard/")

ok_add_library(
    OpenKneeboard-App-WebView-Headers
    INTERFACE
    INCLUDE_DIRECTORIES
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    HEADERS
    ${TS_CPP_HEADERS}
)

target_link_libraries(
    OpenKneeboard-App-WebView-Headers
    INTERFACE
    ThirdParty::JSON
    ThirdParty::magic_json_serialize_enum
    OpenKneeboard-json
    OpenKneeboard-task
)
ok_add_library(
    OpenKneeboard-App-WebView
    STATIC
    ${TS_CPP_SOURCES}
)
target_link_libraries(
    OpenKneeboard-App-WebView
    PUBLIC
    OpenKneeboard-App-WebView-Headers
    PRIVATE
    OpenKneeboard-App-Common
)
target_link_windows_app_sdk(OpenKneeboard-App-WebView)

ok_add_executable(
    dump-js-exports
    EXCLUDE_FROM_ALL
    dump-js-exports.cpp
)

target_link_libraries(
    dump-js-exports
    PRIVATE
    OpenKneeboard-App-WebView-Headers
    OpenKneeboard-Filesystem
    ThirdParty::magic_enum
)

set(UNFORMATTED_TS_DIR "${CMAKE_CURRENT_BINARY_DIR}/unformatted-typescript")
list(TRANSFORM TS_CPP_CLASSES REPLACE "(.+)" "${UNFORMATTED_TS_DIR}/\\1Native.ts" OUTPUT_VARIABLE TS_CPP_UNFORMATTED)
list(APPEND TS_CPP_UNFORMATTED "${UNFORMATTED_TS_DIR}/SystemTheme.d.ts")

add_custom_command(
    OUTPUT ${TS_CPP_UNFORMATTED}
    COMMAND "$<TARGET_FILE:dump-js-exports>"
    DEPENDS dump-js-exports ${TS_CPP_HEADERS}
    WORKING_DIRECTORY "${UNFORMATTED_TS_DIR}"
    VERBATIM
)

add_custom_target(generated-app-typescript-unformatted DEPENDS ${TS_CPP_UNFORMATTED})

find_program(YARN NAMES yarn REQUIRED)

set(YARN_STAMP_FILE "${CMAKE_CURRENT_SOURCE_DIR}/webview-content/node_modules/.yarn-integrity")
add_custom_command(
    OUTPUT "${YARN_STAMP_FILE}"
    COMMAND call "${YARN}" install
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/webview-content/yarn.lock"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/webview-content"
    VERBATIM
)

set(PRETTIER_STAMP_FILE "${CMAKE_CURRENT_SOURCE_DIR}/webview-content/generated/prettier.stamp")
set(CPP_TS_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/webview-content/generated")
list(TRANSFORM TS_CPP_CLASSES REPLACE "(.+)" "${CPP_TS_OUTPUT_DIR}/\\1Native.ts" OUTPUT_VARIABLE TS_CPP_OUTPUT)
add_custom_command(
    OUTPUT "${PRETTIER_STAMP_FILE}" ${TS_CPP_OUTPUT}
    DEPENDS "${TS_CPP_UNFORMATTED}" "${YARN_STAMP_FILE}"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different "${UNFORMATTED_TS_DIR}" "${CPP_TS_OUTPUT_DIR}"
    # `call` is needed due to https://gitlab.kitware.com/cmake/cmake/-/issues/26340
    COMMAND call "${YARN}" run prettier "${CPP_TS_OUTPUT_DIR}" --write
    COMMAND "${CMAKE_COMMAND}" -E touch "${PRETTIER_STAMP_FILE}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/webview-content"
    VERBATIM
)
add_custom_target(generated-app-typescript DEPENDS "${PRETTIER_STAMP_FILE}")

file(
    GLOB_RECURSE TYPESCRIPT_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/webview-content/src/*.ts"
)
file(
    GLOB_RECURSE OTHER_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/webview-content/*.html"
    "${CMAKE_CURRENT_SOURCE_DIR}/webview-content/*.js"
)

set(WEBVIEW_CONTENT_DIR "${BUILD_OUT_BINDIR}/webview-content/")
set(WEBPACK_OUT_DIR "${WEBVIEW_CONTENT_DIR}/dist")
set(WEBPACK_STAMP_FILE "${WEBVIEW_CONTENT_DIR}/.stamp")

set(COPY_STATIC_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/copy-static-sources.cmake")

add_custom_command(
    OUTPUT "${WEBPACK_STAMP_FILE}"
    DEPENDS "${PRETTIER_STAMP_FILE}" "${TYPESCRIPT_SOURCES}" "${OTHER_SOURCES}" "${COPY_STATIC_SOURCES}"
    COMMAND "${CMAKE_COMMAND}" -E rm -rf "${WEBPACK_OUT_DIR}"
    COMMAND
        "${CMAKE_COMMAND}"
        "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/webview-content"
        "-DDESTINATION=${WEBVIEW_CONTENT_DIR}"
        -P "${COPY_STATIC_SOURCES}"
    COMMAND call "${YARN}" run webpack --mode "$<IF:$<CONFIG:Debug>,development,production>" -o "${WEBPACK_OUT_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E touch "${WEBPACK_STAMP_FILE}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/webview-content"
    VERBATIM
)
add_custom_target(webview-content DEPENDS "${WEBPACK_STAMP_FILE}")
add_dependencies(runtime-dependencies webview-content)

install(DIRECTORY "${WEBVIEW_CONTENT_DIR}" DESTINATION "bin/webview-content")
