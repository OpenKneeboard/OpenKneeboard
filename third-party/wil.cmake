include_guard(GLOBAL)

# Used for nuget for WinUI3 (XAML)
set(WINDOWS_IMPLEMENTATION_LIBRARY_VERSION "1.0.250325.1" CACHE INTERNAL "")

find_package(wil CONFIG REQUIRED GLOBAL)

add_library(ThirdParty::WIL ALIAS WIL::WIL)