add_library(d3d11-offsets STATIC d3d11-offsets.c)

add_library(openkneeboard-oculus-d3d11 MODULE oculus-d3d11.cpp)
target_link_libraries(openkneeboard-oculus-d3d11 d3d11-offsets detours fmt oculus-sdk-headers openkneeboard-shm dprint)
