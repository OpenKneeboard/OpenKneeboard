add_library(detours-ext STATIC detours-ext.cpp)
target_link_libraries(detours-ext PUBLIC detours dprint)

add_library(openkneeboard-InjectedKneeboard STATIC InjectedKneeboard.cpp)

add_library(dxgi-hooks STATIC dxgi-offsets.c IDXGISwapChainPresentHook.cpp)
target_link_libraries(dxgi-hooks PRIVATE detours-ext dprint)

add_library(oculus-hooks STATIC OculusFrameHook.cpp OculusKneeboard.cpp)
target_link_libraries(
	oculus-hooks
	PUBLIC
	detours-ext
	oculus-sdk-headers
	openkneeboard-InjectedKneeboard
	dprint
	openkneeboard-shm
	directxtk)

add_library(openkneeboard-d3d11 MODULE nonvr-d3d11.cpp NonVRD3D11Kneeboard.cpp)
target_link_libraries(
  openkneeboard-d3d11
  PRIVATE
	directxtk
  dxgi-hooks
  detours-ext
  dprint
	openkneeboard-shm
	openkneeboard-InjectedKneeboard)

add_library(
  openkneeboard-oculus-d3d11
  MODULE
  oculus-d3d11.cpp
  OculusD3D11Kneeboard.cpp)
target_link_libraries(
  openkneeboard-oculus-d3d11
  PRIVATE
  dxgi-hooks
  detours-ext
  dprint
  oculus-hooks
  oculus-sdk-headers)

add_library(openkneeboard-autodetect MODULE AutoDetectKneeboard.cpp)
target_link_libraries(
  openkneeboard-autodetect
  PRIVATE
  openkneeboard-runtime-files
  dxgi-hooks
  oculus-hooks)
