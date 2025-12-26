find_package(compressed-embed CONFIG REQUIRED)
include(CompressedEmbed)
add_compressed_embed_library(
  OpenKneeboard-JS-Sources
  NAMESPACE OpenKneeboard::CEF
  CLASSNAME JSSources
  OUTPUT_CPP "${CMAKE_CURRENT_BINARY_DIR}/JSSources.cpp"
  OUTPUT_HPP "${CMAKE_CURRENT_BINARY_DIR}/include/OpenKneeboard/CEF/JSSources.hpp"
  INPUTS
  OpenKneeboardNative OpenKneeboardNative.js
  OpenKneeboardAPI OpenKneeboardAPI.js
  SimHub SimHub.js
)
target_include_directories(
  OpenKneeboard-JS-Sources
  INTERFACE
  "${CMAKE_CURRENT_BINARY_DIR}/include"
)

set(
  DATA_SOURCES
  DCS-Aircraft-Mapping.json
)
include(ok_add_runtime_files)
ok_add_runtime_files(data ${DATA_SOURCES} DESTINATION share)

file(
  GLOB
  API_DEV_RESOURCES
  api-example-*.html
  OpenKneeboard-*.d.ts
)

foreach (FILE IN LISTS API_DEV_RESOURCES)
  cmake_path(GET FILE FILENAME FILENAME)
  file(
    GENERATE
    OUTPUT "${BUILD_OUT_ROOT}/share/doc/${FILENAME}"
    INPUT "${FILENAME}"
  )
endforeach ()