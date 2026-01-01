set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE static)
set(VCPKG_LIBRARY_LINKAGE static)

# find_package(GeographicLib) will fail if it's built with a newer toolset than the app,
# so force everything to VS 2022 for now
set(VCPKG_PLATFORM_TOOLSET v143)

set(
  DYNAMIC_PORTS
  # Use DCS World's lua.dll
  "lua"
  # There is no static version of the openvr API
  "openvr"
)

if (PORT IN_LIST DYNAMIC_PORTS)
  set(VCPKG_LIBRARY_LINKAGE dynamic)
endif ()

