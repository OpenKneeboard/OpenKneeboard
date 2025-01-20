add_library(oculus-sdk-headers INTERFACE)
target_include_directories(oculus-sdk-headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/ovr_sdk_win_32.0.0/LibOVR/Include)

include(ok_add_license_file)
ok_add_license_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/ovr_sdk_win_32.0.0/LICENSE.txt"
	"LICENSE-ThirdParty-Oculus SDK.txt"
)
