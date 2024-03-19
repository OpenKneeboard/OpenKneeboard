set(
  SYSTEM_LIBRARIES
  Comctl32
  D2d1
  D3d11
  D3d12
  Dbghelp
  Dinput8
  Dwmapi
  Dwrite
  Dxgi
  Dxguid
  Gdi32
  Msi
  Rpcrt4
  Shell32
  Shlwapi
  User32
  WindowsApp
  Wtsapi32
)

foreach(LIBRARY ${SYSTEM_LIBRARIES})
  add_library("System::${LIBRARY}" INTERFACE IMPORTED GLOBAL)
  set_property(
    TARGET "System::${LIBRARY}"
    PROPERTY IMPORTED_LIBNAME "${LIBRARY}"
  )
endforeach()

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