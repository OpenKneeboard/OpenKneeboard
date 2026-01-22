include_guard(GLOBAL)

# Expand `$<CONFIG>` in a string, and create all matching directorie.s
#
# This is useful for `target_include_directories(${TARGET} INTERFACE "${GENEX})`,
# as CMake requires all expansions exist
function(make_config_directories GENEX)
  foreach (CONFIG IN LISTS CMAKE_CONFIGURATION_TYPES)
    string(REPLACE "$<CONFIG>" "${CONFIG}" EXPANDED "${GENEX}")
    file(MAKE_DIRECTORY "${EXPANDED}")
  endforeach ()
endfunction()