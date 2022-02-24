include(FetchContent)

FetchContent_Declare(
  fmt
  URL https://github.com/fmtlib/fmt/releases/download/8.1.1/fmt-8.1.1.zip
  URL_HASH "SHA256=23778bad8edba12d76e4075da06db591f3b0e3c6c04928ced4a7282ca3400e5d"
)

FetchContent_GetProperties(fmt)

if(NOT fmt_POPULATED)
  FetchContent_Populate(fmt)
  add_subdirectory(${fmt_SOURCE_DIR} ${fmt_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

install(
  FILES
  "${fmt_SOURCE_DIR}/LICENSE.rst"
  TYPE DOC
  RENAME "LICENSE-ThirdParty-fmt.txt"
)
add_library(ThirdParty::fmt ALIAS fmt)
