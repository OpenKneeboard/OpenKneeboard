add_library(detours-ext STATIC detours-ext.cpp)
target_link_libraries(detours-ext PUBLIC detours dprint)

add_library(d3d11-hooks STATIC d3d11-offsets.c D3D11DeviceHook.cpp)
target_link_libraries(d3d11-hooks PRIVATE detours-ext dprint)

add_library(
  openkneeboard-oculus-d3d11 MODULE oculus-d3d11.cpp)
target_link_libraries(
  openkneeboard-oculus-d3d11
  PRIVATE
  d3d11-hooks
  detours-ext
  dprint
  fmt
  oculus-sdk-headers
  openkneeboard-shm)
