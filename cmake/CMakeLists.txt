add_custom_target(runtime-dependencies)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}")

macro(scoped_include FILE_OR_MODULE)
  block(${ARGN})
    include(${FILE_OR_MODULE})
  endblock()
endmacro()

scoped_include(build32.cmake)

return(PROPAGATE CMAKE_MODULE_PATH)