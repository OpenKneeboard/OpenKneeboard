add_library(detours-ext STATIC detours-ext.cpp)
target_link_libraries(detours-ext PUBLIC detours openkneeboard-dprint)

add_library(openkneeboard-InjectedKneeboard STATIC InjectedKneeboard.cpp)

add_library(dxgi-hooks STATIC dxgi-offsets.c IDXGISwapChainPresentHook.cpp)
target_link_libraries(dxgi-hooks PRIVATE detours-ext openkneeboard-dprint)

add_library(oculus-hooks STATIC OculusFrameHook.cpp OculusKneeboard.cpp)
target_link_libraries(
	oculus-hooks
	PUBLIC
	detours-ext
	oculus-sdk-headers
	openkneeboard-InjectedKneeboard
	openkneeboard-dprint
	openkneeboard-shm
	directxtk)

add_library(openvr-hooks STATIC IVRCompositorSubmitHook.cpp)
target_link_libraries(
  openvr-hooks
  PUBLIC
  detours-ext
  openvr-headers
  openkneeboard-dprint)

add_library(openkneeboard-nonvr-d3d11 MODULE NonVRD3D11Kneeboard.cpp)
target_link_libraries(
  openkneeboard-nonvr-d3d11
  PRIVATE
	directxtk
  dxgi-hooks
  detours-ext
  openkneeboard-dprint
	openkneeboard-shm
	openkneeboard-InjectedKneeboard)

add_library(openkneeboard-oculus-d3d11 MODULE OculusD3D11Kneeboard.cpp)
target_link_libraries(
  openkneeboard-oculus-d3d11
  PRIVATE
  dxgi-hooks
  detours-ext
  openkneeboard-dprint
  oculus-hooks
  oculus-sdk-headers)

add_library(openkneeboard-autodetect MODULE AutoDetectKneeboard.cpp)
target_link_libraries(
  openkneeboard-autodetect
  PRIVATE
  openkneeboard-runtime-files
  dxgi-hooks
  oculus-hooks
  openvr-hooks)
