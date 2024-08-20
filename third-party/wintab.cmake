add_library(wintab INTERFACE)
target_include_directories(wintab SYSTEM INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/wintab/include")

add_library(ThirdParty::Wintab ALIAS wintab)

# No copyright notice required according to file headers
