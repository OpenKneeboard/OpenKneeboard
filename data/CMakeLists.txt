set(
    DATA_SOURCES
    DCS-Aircraft-Mapping.json
    WebView2.js
    WebView2-SimHub.js
)
install(
    FILES
    ${DATA_SOURCES}
    TYPE DATA
)
list(
    TRANSFORM
    DATA_SOURCES
    PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/"
)
set(DATA_SOURCES "${DATA_SOURCES}" PARENT_SCOPE)

file(
    GLOB
    API_DEV_RESOURCES
    api-example-*.html
    OpenKneeboard-*.d.ts
)

install(
    FILES
    ${API_DEV_RESOURCES}
    DESTINATION
    "share/doc"
)