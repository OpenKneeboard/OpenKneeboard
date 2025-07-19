if (NOT MSVC)
  return()
endif ()

add_link_options(
  "$<$<NOT:$<CONFIG:Debug>>:/INCREMENTAL:NO>"
  # COMDAT folding; drops off another 1MB
  "$<$<NOT:$<CONFIG:Debug>>:/OPT:ICF>"
  # Remove unused functions and data, in particular, WinRT types
  "$<$<NOT:$<CONFIG:Debug>>:/OPT:REF>"
)