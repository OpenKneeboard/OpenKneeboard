set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if (MSVC)
  add_compile_options(
    "/Zc:__cplusplus"
    "/await"
    # Lots of legacy C functions are unsafe, and raise warnings in wxwidgets
    "/wd4996")
endif()

add_compile_definitions(
  "UNICODE=1"
  "NOMINMAX"
  "WIN32_LEAN_AND_MEAN"
  "$<IF:$<CONFIG:Debug>,DEBUG,NDEBUG>"
  "OPENKNEEBOARD_BREAK=$<$<CONFIG:Debug>:__debugbreak()>"
)

# Needed by c++/winrt in some environments/with some windows SDK versions
link_libraries(WindowsApp.lib)

include(sign_target.cmake)

add_subdirectory(lib)

add_subdirectory(dcs-hook)
add_subdirectory(injectables)
add_subdirectory(games)

add_subdirectory(utilities)
add_subdirectory(app)
