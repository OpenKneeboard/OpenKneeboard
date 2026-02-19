add_subdirectory(app-common)

if (NOT ENABLE_APP)
  return()
endif ()

add_subdirectory(app-winui3)
ok_add_runtime_files(copy-quick-start-pdf "${CMAKE_SOURCE_DIR}/docs/Quick Start.pdf")

add_dependencies(OpenKneeboard-App-WinUI3 runtime-dependencies)