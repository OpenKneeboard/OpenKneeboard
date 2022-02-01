add_library(oculus-sdk-headers INTERFACE)
target_include_directories(oculus-sdk-headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/ovr_sdk_win_32.0.0/LibOVR/Include)

install(
	FILES "${CMAKE_CURRENT_SOURCE_DIR}/ovr_sdk_win_32.0.0/LICENSE.txt"
	TYPE DOC
	RENAME "LICENSE-ThirdParty-OculusSDK.txt"
)
