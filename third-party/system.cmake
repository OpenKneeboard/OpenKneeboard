include_guard(GLOBAL)

set(
  DELAYLOAD_SYSTEM_LIBRARIES
  Shell32
  Msi
  Wtsapi32
  Rpcrt4
  Shlwapi
  Dwmapi

  D2d1
  D3d11
  D3d12
  Dcomp
  Dinput8
  Dwrite
  Dxgi
)
set(
  SYSTEM_LIBRARIES
  ${DELAYLOAD_SYSTEM_LIBRARIES}
  Comctl32
  Dxguid
  Gdi32
  User32
  WindowsApp
)

add_library(System::delayimp IMPORTED INTERFACE GLOBAL)
set_target_properties(
  System::delayimp
  PROPERTIES
  IMPORTED_LIBNAME "delayimp"
)

foreach (LIBRARY ${SYSTEM_LIBRARIES})
  add_library("System::${LIBRARY}" INTERFACE IMPORTED GLOBAL)
  set_target_properties(
    "System::${LIBRARY}"
    PROPERTIES
    IMPORTED_LIBNAME "${LIBRARY}"
  )
  if (LIBRARY IN_LIST DELAYLOAD_SYSTEM_LIBRARIES)
    target_link_options(
      "System::${LIBRARY}"
      INTERFACE
      "/DELAYLOAD:${LIBRARY}.dll"
    )
    target_link_libraries("System::${LIBRARY}" INTERFACE System::delayimp)
  endif ()
endforeach ()

include(windows_kits_dir)

find_program(
  FXC_EXE
  fxc
  PATHS
  "${WINDOWS_10_KIT_DIR}/x64"
  "${WINDOWS_10_KIT_DIR}/x86"
  NO_DEFAULT_PATH
  DOC
  "Path to D3D11 shader compiler"
  REQUIRED)
add_executable(System::fxc IMPORTED GLOBAL)
set_target_properties(System::fxc PROPERTIES IMPORTED_LOCATION "${FXC_EXE}")