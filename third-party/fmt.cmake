include(FetchContent)

FetchContent_Declare(
  fmt
  URL https://github.com/fmtlib/fmt/releases/download/8.0.1/fmt-8.0.1.zip
  URL_HASH "SHA256=a627a56eab9554fc1e5dd9a623d0768583b3a383ff70a4312ba68f94c9d415bf"
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
  RENAME "LICENSE-ThirdParty-fmt.rst"
)
