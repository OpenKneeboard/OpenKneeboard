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

foreach(FILE IN LISTS API_DEV_RESOURCES)
  cmake_path(GET FILE FILENAME FILENAME)
  file(
    GENERATE
    OUTPUT "${BUILD_OUT_ROOT}/share/doc/${FILENAME}"
    INPUT "${FILENAME}"
  )
endforeach()