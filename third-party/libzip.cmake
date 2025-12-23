find_package(libzip CONFIG GLOBAL)
if (NOT libzip_FOUND)
  return()
endif ()

add_library(ThirdParty::LibZip ALIAS libzip::zip)
