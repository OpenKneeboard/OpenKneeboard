add_library(detours-ext STATIC detours-ext.cpp)
target_link_libraries(detours-ext PUBLIC detours OpenKneeboard-dprint)

add_library(OpenKneeboard-InjectedKneeboard STATIC InjectedKneeboard.cpp)

add_library(OpenKneeboard-DllLoadWatcher STATIC DllLoadWatcher.cpp)
target_link_libraries(
  OpenKneeboard-DllLoadWatcher
  PRIVATE
  detours-ext
  OpenKneeboard-RuntimeFiles
  OpenKneeboard-dprint)

add_library(dxgi-hooks STATIC dxgi-offsets.c IDXGISwapChainPresentHook.cpp)
target_link_libraries(dxgi-hooks PRIVATE detours-ext OpenKneeboard-dprint)

add_library(oculus-hooks STATIC OculusEndFrameHook.cpp OculusKneeboard.cpp)
target_link_libraries(
	oculus-hooks
	PUBLIC
	detours-ext
	oculus-sdk-headers
	OpenKneeboard-InjectedKneeboard
	OpenKneeboard-dprint
	OpenKneeboard-SHM
	directxtk)

add_library(openvr-hooks STATIC IVRCompositorWaitGetPosesHook.cpp)
target_link_libraries(
  openvr-hooks
  PUBLIC
  detours-ext
  OpenKneeboard-DllLoadWatcher
  openvr-headers
  OpenKneeboard-dprint)

add_library(OpenKneeboard-nonvr-d3d11 MODULE NonVRD3D11Kneeboard.cpp)
target_link_libraries(
  OpenKneeboard-nonvr-d3d11
  PRIVATE
	directxtk
  dxgi-hooks
  detours-ext
  OpenKneeboard-dprint
	OpenKneeboard-SHM
	OpenKneeboard-InjectedKneeboard)

add_library(OpenKneeboard-oculus-d3d11 MODULE OculusD3D11Kneeboard.cpp)
target_link_libraries(
  OpenKneeboard-oculus-d3d11
  PRIVATE
  dxgi-hooks
  detours-ext
  OpenKneeboard-dprint
  oculus-hooks
  oculus-sdk-headers)

add_library(OpenKneeboard-autodetect MODULE AutoDetectKneeboard.cpp)
target_link_libraries(
  OpenKneeboard-autodetect
  PRIVATE
  OpenKneeboard-RuntimeFiles
  detours-ext
  dxgi-hooks
  oculus-hooks
  openvr-hooks)
