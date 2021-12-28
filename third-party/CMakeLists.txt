include(detours.cmake)
include(fmt.cmake)
include(json.cmake)
include(lualib.cmake)
include(wxWidgets.cmake)

add_library(oculus-sdk-headers INTERFACE)
target_include_directories(oculus-sdk-headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/ovr_sdk_win_32.0.0/LibOVR/Include)
