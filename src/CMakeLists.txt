set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if (MSVC)
  add_definitions("/Zc:__cplusplus" "/DUNICODE=1" "/DNOMINMAX" "/DWIN32_LEAN_AND_MEAN" "/await")
endif()

add_definitions(
  "-DOPENKNEEBOARD_TEXTURE_WIDTH=768"
  "-DOPENKNEEBOARD_TEXTURE_HEIGHT=1024"
)

add_subdirectory(lib)

add_subdirectory(injectables)
add_subdirectory(dcs)
add_subdirectory(utilities)

add_subdirectory(tabs)
add_subdirectory(app)
