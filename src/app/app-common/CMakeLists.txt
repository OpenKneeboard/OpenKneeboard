ok_add_library(OpenKneeboard-Events STATIC Events.cpp)
target_link_libraries(
  OpenKneeboard-Events
  PUBLIC
  OpenKneeboard-config
  OpenKneeboard-dprint
  OpenKneeboard-task
  ThirdParty::WIL
  ThirdParty::CppWinRT
  ThirdParty::bindline
  PRIVATE
  OpenKneeboard-config
  OpenKneeboard-fatal
)
target_include_directories(OpenKneeboard-Events PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

file(GLOB_RECURSE APP_COMMON_SOURCES CONFIGURE_DEPENDS "*.cpp" "*.hpp")
list(FILTER APP_COMMON_SOURCES EXCLUDE REGEX "\\bEvents\\.[ch]pp$")

ok_add_library(OpenKneeboard-App-Common STATIC ${APP_COMMON_SOURCES})
target_compile_definitions(
  OpenKneeboard-App-Common
  PUBLIC
  "DIRECTINPUT_VERSION=0x0800"
)
target_link_windows_app_sdk(OpenKneeboard-App-Common)
target_link_libraries(
  OpenKneeboard-App-Common
  PUBLIC
  OpenKneeboard-Events
  OpenKneeboard-StateMachine
  ThirdParty::DirectXTK
  ThirdParty::JSON
  Cef::LibCef
  PRIVATE
  OpenKneeboard-D2DErrorRenderer
  OpenKneeboard-DXResources
  OpenKneeboard-Filesystem
  OpenKneeboard-APIEvent
  OpenKneeboard-EnumerateProcesses
  OpenKneeboard-GetSystemColor
  OpenKneeboard-PDFNavigation
  OpenKneeboard-RayIntersectsRect
  OpenKneeboard-RuntimeFiles
  OpenKneeboard-SHM
  OpenKneeboard-SteamVRKneeboard
  OpenKneeboard-ThreadGuard
  OpenKneeboard-UTF8
  OpenKneeboard-WindowCaptureControl
  OpenKneeboard-config
  OpenKneeboard-dprint
  OpenKneeboard-fatal
  OpenKneeboard-games
  OpenKneeboard-handles
  OpenKneeboard-scope_exit
  OpenKneeboard-semver
  OpenKneeboard-task
  System::D2d1
  System::Dinput8
  System::Dwmapi
  System::Dxguid
  System::Rpcrt4
  System::Wtsapi32
  ThirdParty::GeographicLib
  ThirdParty::LibZip
  ThirdParty::Lua
  ThirdParty::OTDIPC
  ThirdParty::OpenVR
  ThirdParty::WMM
  ThirdParty::magic_json_serialize_enum
)
target_link_windows_app_sdk(OpenKneeboard-App-Common)

target_include_directories(
  OpenKneeboard-App-Common
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/PageSource/include
  ${CMAKE_CURRENT_SOURCE_DIR}/Tab/include
  ${CMAKE_CURRENT_SOURCE_DIR}/TabsList/include
  ${CMAKE_CURRENT_SOURCE_DIR}/ToolbarItems/include
  ${CMAKE_CURRENT_SOURCE_DIR}/UILayers/include
  ${CMAKE_CURRENT_SOURCE_DIR}/UserInput/include
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

ok_add_library(OpenKneeboard::App::Common ALIAS OpenKneeboard-App-Common)
