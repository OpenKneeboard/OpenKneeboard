add_library(detours-ext STATIC detours-ext.cpp)
target_link_libraries(detours-ext PUBLIC detours dprint)

add_library(openkneeboard-InjectedKneeboard STATIC InjectedKneeboard.cpp)

add_library(d3d11-hooks STATIC d3d11-offsets.c IDXGISwapChainPresentHook.cpp)
target_link_libraries(d3d11-hooks PRIVATE detours-ext dprint)

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

add_library(openkneeboard-oculus-d3d11-lib STATIC OculusD3D11Kneeboard.cpp)
target_link_libraries(
  openkneeboard-oculus-d3d11-lib
  PUBLIC
  d3d11-hooks
  detours-ext
  dprint
  oculus-hooks
  oculus-sdk-headers)

add_library(
  openkneeboard-oculus-d3d11
  MODULE
  oculus-d3d11.cpp)
target_link_libraries(openkneeboard-oculus-d3d11 PRIVATE openkneeboard-oculus-d3d11-lib)

add_library(
  openkneeboard-autodetect
  MODULE
  AutoDetectKneeboard.cpp)
target_link_libraries(
  openkneeboard-autodetect
  PRIVATE
  openkneeboard-oculus-d3d11-lib)
