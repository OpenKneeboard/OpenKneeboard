include_guard(GLOBAL)

include(make_config_directories)

function(target_include_config_directories TARGET KIND)
  foreach (GENEX IN LISTS ARGN)
    make_config_directories("${GENEX}")
    target_include_directories("${TARGET}" "${KIND}" "${GENEX}")
  endforeach ()
endfunction()