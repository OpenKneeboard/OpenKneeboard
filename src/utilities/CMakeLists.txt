include(GNUInstallDirs)

function(add_helper_executable TARGET)
  set(options 32BIT_ONLY)
  set(valueArgs)
  set(multiValueArgs)
  cmake_parse_arguments(HELPER_ARG "${options}" "${valueArgs}" "${multiValueArgs}" ${ARGN})

  ok_add_executable(${TARGET} EXCLUDE_FROM_ALL RUNTIME_DEPENDENCY ${ARGN})
  if (HELPER_ARG_32BIT_ONLY AND NOT BUILD_IS_32BIT)
    return()
  endif ()
endfunction()

function(add_utility_executable TARGET)
  ok_add_executable(
    ${TARGET}
    RUNTIME_EXE
    OUTPUT_SUBDIR "utilities"
    ${ARGN}
  )
endfunction()

include(remote-control.cmake)

add_utility_executable(
  OpenKneeboard-Plugin-Tab-Action
  WIN32
  plugin-tab-action.cpp
)
target_link_libraries(
  OpenKneeboard-Plugin-Tab-Action
  PRIVATE
  OpenKneeboard-dprint
  OpenKneeboard-APIEvent
  OpenKneeboard-tracing
)

add_utility_executable(
  OpenKneeboard-RemoteControl-SET_TAB
  WIN32
  set-tab-remote.cpp
  remote-traceprovider.cpp
)
target_link_libraries(
  OpenKneeboard-RemoteControl-SET_TAB
  PRIVATE
  OpenKneeboard-dprint
  OpenKneeboard-APIEvent
  OpenKneeboard-tracing
)

add_utility_executable(
  OpenKneeboard-RemoteControl-SET_PROFILE
  WIN32
  set-profile-remote.cpp
  remote-traceprovider.cpp
)
target_link_libraries(
  OpenKneeboard-RemoteControl-SET_PROFILE
  PRIVATE
  OpenKneeboard-APIEvent
  OpenKneeboard-dprint
  OpenKneeboard-tracing
)

add_utility_executable(
  OpenKneeboard-Viewer
  WIN32
  viewer.cpp
  viewer-settings.cpp
  viewer-d3d11.cpp
  viewer-d3d12.cpp
  viewer-vulkan.cpp
  dds.hpp
  icon.rc
)
target_link_libraries(
  OpenKneeboard-Viewer
  PRIVATE
  OpenKneeboard-fatal
  OpenKneeboard-config
  OpenKneeboard-D2DErrorRenderer
  OpenKneeboard-D3D11
  OpenKneeboard-D3D12
  OpenKneeboard-Vulkan
  OpenKneeboard-DXResources
  OpenKneeboard-Filesystem
  OpenKneeboard-APIEvent
  OpenKneeboard-GetSystemColor
  OpenKneeboard-RenderDoc
  OpenKneeboard-SHM
  OpenKneeboard-SHM-Client-D3D11
  OpenKneeboard-SHM-Client-D3D12
  OpenKneeboard-SHM-Client-Vulkan
  OpenKneeboard-Viewer-Shaders
  OpenKneeboard-scope_exit
  System::D2d1
  System::D3d11
  System::Dcomp
  System::Dxgi
  ThirdParty::DirectXTK
)

add_helper_executable(
  OpenKneeboard-OpenXR-Helper
  WIN32
  openxr-helper.cpp
  icon.rc
)
target_link_libraries(
  OpenKneeboard-OpenXR-Helper
  PRIVATE
  OpenKneeboard-RuntimeFiles
  OpenKneeboard-dprint
)

add_helper_executable(
  OpenKneeboard-WindowCaptureHook-Helper
  WIN32
  32BIT_ONLY
  WindowCaptureHook-helper.cpp
)
target_link_libraries(
  OpenKneeboard-WindowCaptureHook-Helper
  PRIVATE
  OpenKneeboard-WindowCaptureControl
  OpenKneeboard-consolelib
  OpenKneeboard-dprint
  OpenKneeboard-shims
  System::Shell32
)

add_executable(capi-test capi-test.cpp)
target_include_directories(
  capi-test
  PRIVATE
  "$<TARGET_PROPERTY:OpenKneeboard-c-api,INTERFACE_INCLUDE_DIRECTORIES>")
set_property(
  TARGET capi-test
  PROPERTY CXX_STANDARD 17)

ok_add_executable(symbolize-traces symbolize-traces.cpp)
target_link_libraries(
  symbolize-traces
  PRIVATE
  OpenKneeboard-shims
  ThirdParty::DIA
  ThirdParty::WIL
)
set_target_properties(
  symbolize-traces
  PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${BUILD_OUT_PDBDIR}"
)
add_custom_command(
  TARGET symbolize-traces
  POST_BUILD
  COMMAND
  "${CMAKE_COMMAND}" -E copy_if_different
  "${DIASDK_DLL}"
  "$<TARGET_FILE_DIR:symbolize-traces>/"
)

add_executable(
  task-test
  task-test.cpp
)
target_link_libraries(
  task-test
  PRIVATE
  ThirdParty::WIL
  OpenKneeboard-task
)