include(FetchContent)
FetchContent_Declare(
    imguiFetch
    URL "https://github.com/ocornut/imgui/archive/refs/tags/v1.90.zip"
    URL_HASH "SHA256=fbe086f5bfc631db509acea851734f6996d6d2ef8fdeacb3f097f6def21e8af8"
    DOWNLOAD_EXTRACT_TIMESTAMP ON
)

FetchContent_MakeAvailable(imguiFetch)
FetchContent_GetProperties(imguiFetch)

# build IMGUI core
file(GLOB IMGUI_SOURCES ${imguifetch_SOURCE_DIR}/*.cpp)
include(CMakePrintHelpers)
cmake_print_variables(imguifetch_SOURCE_DIR)
add_library(ImGui STATIC ${IMGUI_SOURCES})
target_include_directories(ImGui INTERFACE "${imguifetch_SOURCE_DIR}")

install(
    FILES
    "${imguifetch_SOURCE_DIR}/LICENSE"
    TYPE DOC
    RENAME "LICENSE-Dear IMGUI.txt"
)

add_library(ThirdParty::IMGUI ALIAS ImGui)