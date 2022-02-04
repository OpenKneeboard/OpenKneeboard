add_library(detours-ext STATIC detours-ext.cpp)
target_link_libraries(detours-ext PUBLIC detours OpenKneeboard-dprint)

add_library(OpenKneeboard-DllLoadWatcher STATIC DllLoadWatcher.cpp)
target_link_libraries(
  OpenKneeboard-DllLoadWatcher
  PRIVATE
  detours-ext
  OpenKneeboard-dprint)

add_library(OpenKneeboard-function-patterns STATIC function-patterns.cpp)
target_link_libraries(
  OpenKneeboard-function-patterns
  PRIVATE
  OpenKneeboard-dprint)

add_library(dxgi-hooks STATIC dxgi-offsets.c IDXGISwapChainPresentHook.cpp)
target_link_libraries(
  dxgi-hooks
  PRIVATE
  detours-ext
  OpenKneeboard-dprint
  OpenKneeboard-function-patterns)

add_library(d3d12-hooks STATIC d3d12-offsets.c ID3D12CommandQueueExecuteCommandListsHook.cpp)
target_link_libraries(d3d12-hooks PRIVATE detours-ext OpenKneeboard-dprint)

add_library(OpenKneeboard-ScopedRWX STATIC ScopedRWX.cpp)

add_library(
  oculus-hooks
  STATIC
  OculusEndFrameHook.cpp
  OculusKneeboard.cpp
  OVRProxy.cpp)
target_link_libraries(
  oculus-hooks
  PUBLIC
  detours-ext
  oculus-sdk-headers
  OpenKneeboard-DllLoadWatcher
  OpenKneeboard-dprint
  OpenKneeboard-SHM
  directxtk)

add_library(openvr-hooks STATIC IVRCompositorWaitGetPosesHook.cpp)
target_link_libraries(
  openvr-hooks
  PUBLIC
  detours-ext
  OpenKneeboard-DllLoadWatcher
  OpenKneeboard-ScopedRWX
  openvr-headers
  OpenKneeboard-dprint)

# This is a marker library that tells the main app not to inject the DLL
add_library(OpenKneeboard-AutoInject-Marker SHARED AutoInjectMarker.cpp)
sign_target(OpenKneeboard-AutoInject-Marker)

add_library(OpenKneeboard-nonvr-d3d11 MODULE NonVRD3D11Kneeboard.cpp)
sign_target(OpenKneeboard-nonvr-d3d11)
target_link_libraries(
  OpenKneeboard-nonvr-d3d11
  PRIVATE
  directxtk
  dxgi-hooks
  detours-ext
  OpenKneeboard-dprint
  OpenKneeboard-SHM
  OpenKneeboard-DllLoadWatcher)

add_library(OpenKneeboard-oculus-d3d11 MODULE OculusD3D11Kneeboard.cpp)
sign_target(OpenKneeboard-oculus-d3d11)
target_link_libraries(
  OpenKneeboard-oculus-d3d11
  PRIVATE
  dxgi-hooks
  detours-ext
  OpenKneeboard-dprint
  oculus-hooks
  oculus-sdk-headers)

add_library(OpenKneeboard-oculus-d3d12 MODULE OculusD3D12Kneeboard.cpp)
sign_target(OpenKneeboard-oculus-d3d12)
target_link_libraries(
  OpenKneeboard-oculus-d3d12
  PRIVATE
  d3d12-hooks
  detours-ext
  OpenKneeboard-dprint
  OpenKneeboard-ScopedRWX
  oculus-hooks
  oculus-sdk-headers)

add_library(OpenKneeboard-Injection-Bootstrapper MODULE InjectionBootstrapper.cpp)
sign_target(OpenKneeboard-Injection-Bootstrapper)
target_link_libraries(
  OpenKneeboard-Injection-Bootstrapper
  PRIVATE
  OpenKneeboard-RuntimeFiles
  detours-ext
  dxgi-hooks
  oculus-hooks
  openvr-hooks)

add_library(OpenKneeboard-TabletProxy MODULE TabletProxy.cpp)
sign_target(OpenKneeboard-TabletProxy)
target_link_libraries(
  OpenKneeboard-TabletProxy
  PRIVATE
  detours-ext
  OpenKneeboard-dprint
  OpenKneeboard-SHM
  OpenKneeboard-tablets)
