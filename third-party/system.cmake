set(
  SYSTEM_LIBRARIES
  D2d1
  D3d11
  D3d12
  Dbghelp
  Dinput8
  Dwmapi
  Dwrite
  Dxgi
  Dxguid
  Rpcrt4
  Shell32
  Shlwapi
  User32
  WindowsApp
)

foreach(LIBRARY ${SYSTEM_LIBRARIES})
  add_library("System::${LIBRARY}" INTERFACE IMPORTED GLOBAL)
  set_property(
    TARGET "System::${LIBRARY}"
    PROPERTY IMPORTED_LIBNAME "${LIBRARY}"
  )
endforeach()
