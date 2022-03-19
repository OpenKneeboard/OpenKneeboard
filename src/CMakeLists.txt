set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(sign_target.cmake)
configure_file("${CMAKE_SOURCE_DIR}/LICENSE" "${CMAKE_CURRENT_BINARY_DIR}/license.wix.txt" COPYONLY)

set(CPACK_GENERATOR "WIX")
set(CPACK_PACKAGE_NAME "OpenKneeboard")
set(CPACK_PACKAGE_VENDOR "Fred Emmott")
set(CPACK_WIX_UPGRADE_GUID "843c9331-0610-4ab1-9cf9-5305c896fb5b")
set(CPACK_WIX_LICENSE_RTF "${CMAKE_CURRENT_BINARY_DIR}/license.wix.txt")
include(CPack)

if (MSVC)
  add_compile_options(
    "/Zc:__cplusplus"
    "/await"
    # Lots of legacy C functions are unsafe, and raise warnings in third-party code
    "/wd4996"
    # Basically expected when you do diamond inheritance that isn't entirely pure virtuals
    "/wd4250")
endif()

add_compile_definitions(
  "UNICODE=1"
  "_UNICODE=1"
  "NOMINMAX=1"
  "WIN32_LEAN_AND_MEAN=1"
  "$<IF:$<CONFIG:Debug>,DEBUG,NDEBUG>"
  "OPENKNEEBOARD_BREAK=$<$<CONFIG:Debug>:__debugbreak()>"
  "BUILD_CONFIG=\"$<CONFIG>\""
)

# Always link this so we never accidentally use the SDK version
link_libraries(NuGet::CppWinRT::Base)

add_subdirectory(lib)

add_subdirectory(dcs-hook)
add_subdirectory(injectables)
add_subdirectory(games)

add_subdirectory(utilities)
add_subdirectory(app)
