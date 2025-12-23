include_guard(GLOBAL)
include(system.cmake)

# Used for nuget
set(CPPWINRT_VERSION "2.0.240111.5" CACHE INTERNAL "")

find_package(cppwinrt CONFIG REQUIRED GLOBAL)
add_library(cppwinrt_with_deps INTERFACE)
target_link_libraries(
  cppwinrt_with_deps
  INTERFACE
  Microsoft::CppWinRT
  System::WindowsApp
)
add_library(ThirdParty::CppWinRT ALIAS cppwinrt_with_deps)

include(ok_add_license_file)
ok_add_license_file(
  "${OKB_VCPKG_SHARE_DIR}/cppwinrt/copyright"
  "LICENSE-ThirdParty-CppWinRT.txt"
)
