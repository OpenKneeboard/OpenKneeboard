
set(SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/renderdoc")

add_library(renderdoc INTERFACE)
target_include_directories(renderdoc INTERFACE "${SOURCE_DIR}/include")
add_library(ThirdParty::RenderDoc ALIAS renderdoc)

install(
	FILES "${SOURCE_DIR}/LICENSE.md"
	TYPE DOC
	RENAME "LICENSE-ThirdParty-RenderDoc.txt"
)
