set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE static)
set(VCPKG_LIBRARY_LINKAGE static)

# find_package(GeographicLib) will fail if it's built with a newer toolset than the app,
# so force everything to VS 2022 for now
set(VCPKG_PLATFORM_TOOLSET v143)

# There is no static version of the openvr API
if (PORT STREQUAL "openvr")
  set(VCPKG_LIBRARY_LINKAGE dynamic)
endif ()

