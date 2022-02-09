# * Build from source as QPDF only publishes windows binaries for one particular C runtime
# * Hand-written cmake to avoid the need for msys2/bash

FetchContent_Declare(
  qpdf_source
  URL "https://github.com/qpdf/qpdf/releases/download/release-qpdf-10.5.0/qpdf-10.5.0.tar.gz"
  URL_HASH "SHA256=88257d36a44fd5c50b2879488324dd9cafc11686ae49d8c4922a4872203ce006"
)

FetchContent_MakeAvailable(qpdf_source)

file(
  GLOB_RECURSE libqpdf_sources
  "${qpdf_source_SOURCE_DIR}/libqpdf/*.cc"
  "${qpdf_source_SOURCE_DIR}/libqpdf/*.c"
)
# Only supporting native crypto for now
list(FILTER libqpdf_sources EXCLUDE REGEX
  ".*[/\\]QPDFCrypto_(gnutls|openssl).cc$"
)
# This .c file is #include'd from other .c files, not built directly
list(FILTER libqpdf_sources EXCLUDE REGEX
  ".*[/\\]md_helper.c$"
)

include(CheckTypeSize)
include(CheckIncludeFile)
check_type_size(size_t SIZEOF_SIZE_T)
check_include_file("stdint.h" HAVE_STDINT_H)
check_include_file("strings.h" HAVE_STRINGS_H)
check_include_file("string.h" HAVE_STRING_H)
check_include_file("sys/stat.h" HAVE_SYS_STAT_H)
check_include_file("sys/types.h" HAVE_SYS_TYPES_H)
check_include_file("unistd.h" HAVE_UNISTD_H)

# No explicit checks, but part of STD_HEADERS
check_include_file("stdlib.h" HAVE_STDLIB_H)
check_include_file("stdarg.h" HAVE_STDARG_H)
check_include_file("float.h" HAVE_FLOAT_H)
if(HAVE_STDLIB_H)
  include(CheckCSourceCompiles)
  check_c_source_compiles(
"#include <stdlib.h>
int main(int argc, char** argv) {
  void* x;
  memchr(x, 123, 123);
  free(x);
}"
    STDLIB_HAS_MEMCHR_AND_FREE
  )
  SET(
    STDC_HEADERS
    (
        HAVE_STDLIB_H
        AND HAVE_STDARG_H
        AND HAVE_FLOAT_H
        AND HAVE_STRING_H
        AND STDLIB_HAS_MEMCHR_AND_FREE
    )
  )
endif()

set(USE_CRYPTO_NATIVE ON)
set(DEFAULT_CRYPTO "native")
set(LL_FMT "ll")
# CXX_STANDARD=14 and CXX_STANDARD_REQUIRED are set on the target
set(HAVE_CXX14 ON)

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/qpdf-config.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/include/qpdf/qpdf-config.h")

add_library(qpdf STATIC ${libqpdf_sources})
target_link_libraries(qpdf PRIVATE libjpegTurbo zlib)
target_include_directories(qpdf PUBLIC "${qpdf_source_SOURCE_DIR}/include")
target_include_directories(
  qpdf
  PRIVATE
  "${qpdf_source_SOURCE_DIR}/libqpdf"
  "${CMAKE_CURRENT_BINARY_DIR}/include"
)
set_target_properties(
  qpdf
  PROPERTIES
  OUTPUT_NAME_DEBUG qpdfd
  CXX_STANDARD 14 
  CXX_STANDARD_REQUIRED ON
)

install(
  FILES
  "${qpdf_source_SOURCE_DIR}/LICENSE.txt"
  TYPE DOC
  RENAME "LICENSE-ThirdParty-qpdf.txt"
)
