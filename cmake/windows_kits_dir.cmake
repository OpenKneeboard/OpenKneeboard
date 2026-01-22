include_guard(GLOBAL)
get_filename_component(
  WINDOWS_10_KITS_ROOT
  "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots;KitsRoot10]"
  ABSOLUTE CACHE
)
set(
  WINDOWS_10_KIT_DIR
  "${WINDOWS_10_KITS_ROOT}/bin/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}"
  CACHE
  PATH
  "Current Windows 10 kit directory"
)

message(STATUS "Windows 10 Kit path: ${WINDOWS_10_KIT_DIR}")