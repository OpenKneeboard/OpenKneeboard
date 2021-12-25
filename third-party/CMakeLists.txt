add_library(detours INTERFACE)
target_include_directories(detours INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/Detours/include)
target_link_libraries(detours INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/Detours/lib.X64/detours.lib)

add_library(oculus-sdk-headers INTERFACE)
target_include_directories(oculus-sdk-headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/ovr_sdk_win_32.0.0/LibOVR/Include)
#target_link_libraries(oculus-sdk INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/ovr_sdk_win_32.0.0/LibOVR/Lib/Windows/x64/Release/VS2017/LibOVR.lib)

add_library(fmt INTERFACE)
target_include_directories(fmt INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/fmt-8.0.1/include)
target_compile_definitions(fmt INTERFACE "-DFMT_HEADER_ONLY")

set(wxBUILD_SHARED OFF)
add_subdirectory(wxWidgets-3.1.5 EXCLUDE_FROM_ALL)