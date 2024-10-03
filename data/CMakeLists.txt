set(
    DATA_SOURCES
    DCS-Aircraft-Mapping.json
    WebView2.js
    WebView2-SimHub.js
)
ok_add_runtime_files(data ${DATA_SOURCES} DESTINATION share)

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