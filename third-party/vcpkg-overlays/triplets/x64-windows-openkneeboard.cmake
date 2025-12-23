set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE static)
set(VCPKG_LIBRARY_LINKAGE static)

# CEF can't be used with iterator debugging, so we need it off for everything
set(IDL "-D_ITERATOR_DEBUG_LEVEL=0 -D_HAS_ITERATOR_DEBUGGING=0")
set(VCPKG_CXX_FLAGS_DEBUG "${VCPKG_CXX_FLAGS_DEBUG} ${IDL}")
set(VCPKG_C_FLAGS_DEBUG "${VCPKG_C_FLAGS_DEBUG} ${IDL}")

# find_package(GeographicLib) will fail if it's built with a newer toolset than the app,
# so force everything to VS 2022 for now
set(VCPKG_PLATFORM_TOOLSET v143)

# There is no static version of the openvr API
if (PORT STREQUAL "openvr")
  set(VCPKG_LIBRARY_LINKAGE dynamic)
endif ()

