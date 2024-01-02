add_library(
    OpenXRToolkit-SavedState
    src/d3d11.cpp
)

target_include_directories(
    OpenXRToolkit-SavedState
    PRIVATE
    # Needed for OpenKneeboard::gTraceProvider
   "${CMAKE_SOURCE_DIR}/src/lib/include"
)

target_include_directories(
    OpenXRToolkit-SavedState
    PUBLIC
   "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

add_library(ThirdParty::OpenXRToolkit::SavedState ALIAS OpenXRToolkit-SavedState)