set(
    DATA_SOURCES
    DCS-Aircraft-Mapping.json
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