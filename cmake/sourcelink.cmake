set(SOURCELINK "" CACHE STRING "URL to retrieve the source")
if (SOURCELINK AND CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  message(STATUS "Using SOURCELINK: ${SOURCELINK}")
  cmake_path(
    NATIVE_PATH
    CMAKE_SOURCE_DIR
    NORMALIZE
    NATIVE_SOURCE
  )
  string(REPLACE "\\" "\\\\" NATIVE_SOURCE "${NATIVE_SOURCE}")
  file(
    WRITE
    "${CMAKE_CURRENT_BINARY_DIR}/sourcelink.json"
    "{ \"documents\": { \"${NATIVE_SOURCE}\\\\*\": \"${SOURCELINK}/*\" } }"
  )
  add_link_options("/SOURCELINK:${CMAKE_CURRENT_BINARY_DIR}/sourcelink.json")
elseif (SOURCELINK)
  message(FATAL_ERROR "SOURCELINK was specified, but not using MSVC")
endif ()
