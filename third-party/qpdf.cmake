find_package(qpdf CONFIG GLOBAL)
if (NOT qpdf_FOUND)
  return()
endif ()

add_library(qpdf_wrapper INTERFACE)
target_link_libraries(qpdf_wrapper INTERFACE qpdf::libqpdf)
target_compile_definitions(qpdf_wrapper INTERFACE POINTERHOLDER_TRANSITION=4)
add_library(ThirdParty::QPDF ALIAS qpdf_wrapper)
