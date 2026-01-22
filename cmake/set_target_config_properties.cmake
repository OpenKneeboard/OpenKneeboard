include_guard(GLOBAL)

function(set_target_config_properties TARGET PROPERTY_BASE GENEX)
  foreach (CONFIG IN LISTS CMAKE_CONFIGURATION_TYPES)
    string(REPLACE "$<CONFIG>" "${CONFIG}" EXPANDED "${GENEX}")
    string(TOUPPER "${CONFIG}" CONFIG_UPPER)
    set_target_properties(
      "${TARGET}"
      PROPERTIES
      "${PROPERTY_BASE}_${CONFIG_UPPER}" "${EXPANDED}"
    )
  endforeach ()
  # Set this so the property can easily be passed to ExternalProject arguments,
  # with generator expression evaluation happening there
  set_target_properties("${TARGET}" PROPERTIES "${PROPERTY_BASE}" "${GENEX}")
endfunction()