# Implementation of C++23 std::out_ptr
#
# From https://github.com/soasis/out_ptr
#
# Embedded as it's tiny, and there's no tags.
#
# Remove in favor of std::out_ptr when C++23 is available


add_library(outptr INTERFACE)
target_include_directories(outptr INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/out_ptr/include")
add_library(ThirdParty::OutPtr ALIAS outptr)

install(
	FILES "${CMAKE_CURRENT_SOURCEDIR}/LICENSE"
	TYPE DOC
	RENAME "LICENSE-ThirdParty-out_ptr.txt"
)

